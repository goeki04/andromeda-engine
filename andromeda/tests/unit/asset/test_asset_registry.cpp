// AssetRegistry in memory: what a scan finds, how records are looked up, moved and renamed.
// Everything that goes through asset_db.json is in test_asset_database.cpp.

#include <gtest/gtest.h>
#include <set>
#include "a_AssetTestProject.hpp"

using namespace Andromeda;
using namespace Andromeda::AssetManagement;
using AssetRegistryTest = Andromeda::Test::AssetTestProject;

// ----------------------------------------------------------------------------------------------
// Scanning
// ----------------------------------------------------------------------------------------------

TEST_F(AssetRegistryTest, ScanRegistersKnownAssetTypesOnly) {
    addAsset("models/apo.obj");
    addAsset("textures/wall.png");
    addAsset("notes/todo.unknown");
    addAsset(".gitignore");

    const AssetRegistry registry = startRegistry();

    ASSERT_EQ(registry.getRecords().size(), 2u);
    EXPECT_EQ(registry.findByPath(assets() / "models/apo.obj")->type, AssetType::Mesh);
    EXPECT_EQ(registry.findByPath(assets() / "textures/wall.png")->type, AssetType::Texture);
}

TEST_F(AssetRegistryTest, RecordDescribesItsFile) {
    const std::filesystem::path file = addAsset("models/Room 2.1.obj");

    const AssetRegistry registry = startRegistry();

    const AssetRecord* record = registry.findByPath(file);
    ASSERT_NE(record, nullptr);
    EXPECT_EQ(record->name, "Room 2.1");
    EXPECT_EQ(record->path, keyOf(file));
    EXPECT_EQ(record->type, AssetType::Mesh);
    EXPECT_EQ(record->fileTimestamp,
              static_cast<u64>(std::filesystem::last_write_time(file).time_since_epoch().count()));
}

TEST_F(AssetRegistryTest, ScanningAgainAddsNothingAndKeepsTheGuids) {
    const std::filesystem::path file = addAsset("models/apo.obj");
    AssetRegistry registry = startRegistry();
    const Guid before = guidOf(registry, file);

    registry.scanDirectory(assets());

    EXPECT_EQ(registry.getRecords().size(), 1u);
    EXPECT_EQ(guidOf(registry, file), before);
}

TEST_F(AssetRegistryTest, ScanFindsFilesInNestedFolders) {
    const std::filesystem::path file = addAsset("a/b/c/d/deep.fbx");

    const AssetRegistry registry = startRegistry();

    EXPECT_NE(registry.findByPath(file), nullptr);
}

TEST_F(AssetRegistryTest, ScanningAMissingFolderIsHarmless) {
    AssetRegistry registry(m_Paths);

    EXPECT_NO_THROW(registry.scanDirectory(m_Root / "not_there"));
    EXPECT_TRUE(registry.getRecords().empty());
}

TEST_F(AssetRegistryTest, FileNamesOutsideTheSystemCodePageAreRegistered) {
    // Regression: the scan converted paths with generic_string(), which throws on Windows for
    // characters the code page lacks - one such file stopped the editor at startup.
    addAsset("models/\xC3\xBC" "ber.obj");          // über.obj
    addAsset("models/\xE6\x97\xA5\xE6\x9C\xAC.png"); // 日本.png
    addAsset("models/\xF0\x9F\x98\x80.fbx");         // an emoji

    AssetRegistry registry(m_Paths);
    ASSERT_NO_THROW(registry.scanDirectory(assets()));

    std::set<std::string> names;
    for (const AssetRecord& record : registry.getRecords()) {
        names.insert(record.name);
    }
    EXPECT_EQ(names, (std::set<std::string>{ "\xC3\xBC" "ber", "\xE6\x97\xA5\xE6\x9C\xAC", "\xF0\x9F\x98\x80" }));
}

TEST_F(AssetRegistryTest, ImportingAKnownFileDoesNotDuplicateIt) {
    const std::filesystem::path file = addAsset("models/apo.obj");
    AssetRegistry registry = startRegistry();

    registry.importAsset(file);

    EXPECT_EQ(registry.getRecords().size(), 1u);
}

TEST_F(AssetRegistryTest, ImportAddsASingleFile) {
    const std::filesystem::path file = addAsset("models/apo.obj");
    AssetRegistry registry(m_Paths);

    registry.importAsset(file);

    ASSERT_EQ(registry.getRecords().size(), 1u);
    EXPECT_EQ(registry.getRecords().front().path, keyOf(file));
}

// ----------------------------------------------------------------------------------------------
// Lookup
// ----------------------------------------------------------------------------------------------

TEST_F(AssetRegistryTest, FindByPathAcceptsEverySpellingOfTheSamePath) {
    const std::filesystem::path file = addAsset("models/apo.obj");
    const AssetRegistry registry = startRegistry();
    const AssetRecord* expected = registry.findByPath(file);
    ASSERT_NE(expected, nullptr);

    EXPECT_EQ(registry.findByPath(assets() / "models/./apo.obj"), expected);
    EXPECT_EQ(registry.findByPath(assets() / "textures/../models/apo.obj"), expected);
#ifdef _WIN32
    // What the file watcher and drag and drop deliver.
    EXPECT_EQ(registry.findByPath(std::filesystem::path(file).make_preferred()), expected);
#endif
}

TEST_F(AssetRegistryTest, UnknownPathOrGuidFindsNothing) {
    addAsset("models/apo.obj");
    const AssetRegistry registry = startRegistry();

    EXPECT_EQ(registry.findByPath(assets() / "models/other.obj"), nullptr);
    EXPECT_EQ(registry.findByGuid(Guid{}), nullptr);
}

TEST_F(AssetRegistryTest, NewGuidsAreDistinctRandomVersion4Uuids) {
    for (int i = 0; i < 64; ++i) {
        addAsset("models/m" + std::to_string(i) + ".obj");
    }

    const AssetRegistry registry = startRegistry();

    std::set<std::string> seen;
    for (const AssetRecord& record : registry.getRecords()) {
        EXPECT_EQ(record.guid.bytes[6] >> 4, 4) << "version nibble of " << record.guid.toString();
        EXPECT_EQ(record.guid.bytes[8] & 0xC0, 0x80) << "variant bits of " << record.guid.toString();
        seen.insert(record.guid.toString());
    }
    EXPECT_EQ(seen.size(), 64u);
}

// ----------------------------------------------------------------------------------------------
// moveAsset: the registry follows a file that was moved by someone else
// ----------------------------------------------------------------------------------------------

TEST_F(AssetRegistryTest, MoveKeepsTheGuidAndUpdatesPathAndName) {
    const std::filesystem::path file = addAsset("models/apo.obj");
    AssetRegistry registry = startRegistry();
    const Guid guid = guidOf(registry, file);
    const std::filesystem::path target = assets() / "props/crate.obj";

    ASSERT_TRUE(registry.moveAsset(guid, target));

    const AssetRecord* record = registry.findByGuid(guid);
    ASSERT_NE(record, nullptr);
    EXPECT_EQ(record->path, keyOf(target));
    EXPECT_EQ(record->name, "crate");
    EXPECT_EQ(registry.findByPath(target), record);
    EXPECT_EQ(registry.findByPath(file), nullptr);
}

TEST_F(AssetRegistryTest, MoveOntoAnotherRecordsPathIsRejected) {
    const std::filesystem::path first = addAsset("models/a.obj");
    const std::filesystem::path second = addAsset("models/b.obj");
    AssetRegistry registry = startRegistry();
    const Guid firstGuid = guidOf(registry, first);

    EXPECT_FALSE(registry.moveAsset(firstGuid, second));

    EXPECT_EQ(registry.findByPath(first)->guid, firstGuid);
    EXPECT_NE(registry.findByPath(second)->guid, firstGuid);
}

TEST_F(AssetRegistryTest, MovingAnUnknownGuidIsRejected) {
    AssetRegistry registry(m_Paths);

    EXPECT_FALSE(registry.moveAsset(Guid{}, assets() / "x.obj"));
}

// ----------------------------------------------------------------------------------------------
// renameAsset: file and record change together, the Guid stays
// ----------------------------------------------------------------------------------------------

TEST_F(AssetRegistryTest, RenameMovesTheFileAndKeepsTheGuid) {
    const std::filesystem::path file = addAsset("models/apo.obj", "mesh bytes");
    AssetRegistry registry = startRegistry();
    const Guid guid = guidOf(registry, file);

    const RenameResult result = registry.renameAsset(guid, "crate");

    ASSERT_EQ(result.status, RenameStatus::Renamed);
    const std::filesystem::path renamed = assets() / "models/crate.obj";
    EXPECT_FALSE(std::filesystem::exists(file));
    EXPECT_EQ(m_Dir.read(renamed), "mesh bytes");
    EXPECT_EQ(guidOf(registry, renamed), guid);
    EXPECT_EQ(registry.findByGuid(guid)->name, "crate");
}

TEST_F(AssetRegistryTest, RenameKeepsTheExtension) {
    const std::filesystem::path file = addAsset("models/apo.fbx");
    AssetRegistry registry = startRegistry();

    ASSERT_EQ(registry.renameAsset(guidOf(registry, file), "Room 2.1").status, RenameStatus::Renamed);

    EXPECT_TRUE(std::filesystem::exists(assets() / "models/Room 2.1.fbx"));
}

TEST_F(AssetRegistryTest, RenameToTheCurrentNameChangesNothing) {
    const std::filesystem::path file = addAsset("models/apo.obj");
    AssetRegistry registry = startRegistry();

    EXPECT_EQ(registry.renameAsset(guidOf(registry, file), "apo").status, RenameStatus::Unchanged);
    EXPECT_TRUE(std::filesystem::exists(file));
}

TEST_F(AssetRegistryTest, RenameWithAnInvalidNameTouchesNothing) {
    const std::filesystem::path file = addAsset("models/apo.obj");
    AssetRegistry registry = startRegistry();
    const Guid guid = guidOf(registry, file);

    EXPECT_EQ(registry.renameAsset(guid, "a:b").status, RenameStatus::InvalidName);

    EXPECT_TRUE(std::filesystem::exists(file));
    EXPECT_EQ(registry.findByGuid(guid)->name, "apo");
}

TEST_F(AssetRegistryTest, RenameOntoAnExistingFileIsRefusedAndBothFilesSurvive) {
    const std::filesystem::path file = addAsset("models/apo.obj", "apo");
    const std::filesystem::path other = addAsset("models/crate.obj", "crate");
    AssetRegistry registry = startRegistry();

    EXPECT_EQ(registry.renameAsset(guidOf(registry, file), "crate").status, RenameStatus::NameTaken);

    EXPECT_EQ(m_Dir.read(file), "apo");
    EXPECT_EQ(m_Dir.read(other), "crate");
}

TEST_F(AssetRegistryTest, RenameOntoAFileTheRegistryDoesNotKnowIsRefusedToo) {
    // A file that was copied in after the last scan: the registry has no record for it, but it
    // must not be overwritten all the same.
    const std::filesystem::path file = addAsset("models/apo.obj");
    AssetRegistry registry = startRegistry();
    const std::filesystem::path unregistered = addAsset("models/crate.obj", "not registered yet");

    EXPECT_EQ(registry.renameAsset(guidOf(registry, file), "crate").status, RenameStatus::NameTaken);
    EXPECT_EQ(m_Dir.read(unregistered), "not registered yet");
}

TEST_F(AssetRegistryTest, ChangingOnlyTheCaseIsARename) {
    // On Windows "Apo.obj" already "exists" - it is the same file. That must not count as taken.
    const std::filesystem::path file = addAsset("models/apo.obj");
    AssetRegistry registry = startRegistry();
    const Guid guid = guidOf(registry, file);

    ASSERT_EQ(registry.renameAsset(guid, "Apo").status, RenameStatus::Renamed);

    EXPECT_EQ(registry.findByGuid(guid)->name, "Apo");
    std::vector<std::string> onDisk;
    for (const auto& entry : std::filesystem::directory_iterator(assets() / "models")) {
        onDisk.push_back(Filesystem::toUtf8(entry.path().filename()));
    }
    EXPECT_EQ(onDisk, std::vector<std::string>{ "Apo.obj" });
}

TEST_F(AssetRegistryTest, RenamingAnUnknownGuidIsReported) {
    AssetRegistry registry(m_Paths);

    EXPECT_EQ(registry.renameAsset(Guid{}, "x").status, RenameStatus::UnknownAsset);
}
