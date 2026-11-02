// asset_db.json: the one promise that matters is that an asset keeps its Guid across restarts,
// renames and moves of the whole project - every saved scene refers to assets by that Guid.

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include "a_AssetTestProject.hpp"
#include "a_filesystem.hpp"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

using namespace Andromeda;
using namespace Andromeda::AssetManagement;
using AssetDatabaseTest = Andromeda::Test::AssetTestProject;

namespace {
    nlohmann::json readJson(const Andromeda::Test::TempDirectory& dir, const std::filesystem::path& file) {
        return nlohmann::json::parse(dir.read(file));
    }
}

TEST_F(AssetDatabaseTest, GuidsSurviveARestart) {
    const std::filesystem::path mesh = addAsset("models/apo.obj");
    const std::filesystem::path texture = addAsset("textures/wall.png");
    Guid meshGuid;
    Guid textureGuid;
    {
        const AssetRegistry firstSession = startRegistry();
        meshGuid = guidOf(firstSession, mesh);
        textureGuid = guidOf(firstSession, texture);
        ASSERT_TRUE(firstSession.saveDatabase(database()));
    }

    const AssetRegistry secondSession = startRegistry();

    EXPECT_EQ(guidOf(secondSession, mesh), meshGuid);
    EXPECT_EQ(guidOf(secondSession, texture), textureGuid);
}

TEST_F(AssetDatabaseTest, PathsAreStoredRelativeToTheProject) {
    addAsset("models/apo.obj");
    ASSERT_TRUE(startRegistry().saveDatabase(database()));

    const nlohmann::json root = readJson(m_Dir, database());

    ASSERT_EQ(root.at("assets").size(), 1u);
    EXPECT_EQ(root["assets"][0].at("path"), "assets/models/apo.obj");
}

TEST_F(AssetDatabaseTest, ProjectCanBeMovedTogetherWithItsDatabase) {
    // The reason paths are stored relative: a project checked out to another folder (another PC,
    // another user name) must keep every Guid.
    const std::filesystem::path mesh = addAsset("models/apo.obj");
    Guid guid;
    {
        const AssetRegistry before = startRegistry();
        guid = guidOf(before, mesh);
        ASSERT_TRUE(before.saveDatabase(database()));
    }
    const std::filesystem::path movedRoot = m_Dir.path() / "Elsewhere" / "Game";
    std::filesystem::create_directories(movedRoot.parent_path());
    std::filesystem::rename(m_Root, movedRoot);

    AssetRegistry after{ ProjectPaths(movedRoot) };
    ASSERT_TRUE(after.loadDatabase(movedRoot / "ProjectSettings" / "asset_db.json"));
    after.scanDirectory(movedRoot / "assets");

    EXPECT_EQ(guidOf(after, movedRoot / "assets" / "models" / "apo.obj"), guid);
}

TEST_F(AssetDatabaseTest, AssetsOutsideTheProjectAreStoredAbsolute) {
    const std::filesystem::path shared = m_Dir.write("SharedAssets/rock.obj");
    AssetRegistry registry(m_Paths);
    registry.scanDirectory(m_Dir.path() / "SharedAssets");
    ASSERT_TRUE(registry.saveDatabase(database()));

    const nlohmann::json root = readJson(m_Dir, database());

    EXPECT_EQ(root["assets"][0].at("path"), keyOf(shared));
}

TEST_F(AssetDatabaseTest, FileHasNoVersionField) {
    // Agreed format: the database carries no version number.
    addAsset("models/apo.obj");
    ASSERT_TRUE(startRegistry().saveDatabase(database()));

    EXPECT_FALSE(readJson(m_Dir, database()).contains("version"));
}

TEST_F(AssetDatabaseTest, SavingTwiceWithoutChangesGivesTheSameFile) {
    // The file is versioned in git: a save that reorders entries would show up as a change.
    addAsset("models/apo.obj");
    addAsset("models/crate.obj");
    addAsset("textures/wall.png");
    const AssetRegistry registry = startRegistry();

    ASSERT_TRUE(registry.saveDatabase(database()));
    const std::string first = m_Dir.read(database());
    ASSERT_TRUE(registry.saveDatabase(database()));

    EXPECT_EQ(m_Dir.read(database()), first);
}

TEST_F(AssetDatabaseTest, NonAsciiPathsKeepTheirGuid) {
    // Regression: paths were written in the system code page, which is not valid UTF-8 - JSON's
    // dump() threw and the editor ended while saving.
    const std::filesystem::path file = addAsset("modelle/\xC3\xBC" "ber/\xE6\x97\xA5\xE6\x9C\xAC.obj");
    Guid guid;
    {
        const AssetRegistry before = startRegistry();
        guid = guidOf(before, file);
        ASSERT_TRUE(before.saveDatabase(database()));
    }

    EXPECT_EQ(readJson(m_Dir, database())["assets"][0].at("path"), "assets/modelle/\xC3\xBC" "ber/\xE6\x97\xA5\xE6\x9C\xAC.obj");
    EXPECT_EQ(guidOf(startRegistry(), file), guid);
}

TEST_F(AssetDatabaseTest, MissingFileMeansFirstStart) {
    AssetRegistry registry(m_Paths);

    EXPECT_TRUE(registry.loadDatabase(database()));
}

TEST_F(AssetDatabaseTest, BrokenJsonIsRejected) {
    m_Dir.write(database(), "{ \"assets\": [ ");
    AssetRegistry registry(m_Paths);

    EXPECT_FALSE(registry.loadDatabase(database()));
}

TEST_F(AssetDatabaseTest, JsonOfTheWrongShapeIsRejected) {
    for (const std::string text : { "{}", R"({ "assets": 5 })", "[]", "42" }) {
        SCOPED_TRACE(text);
        m_Dir.write(database(), text);
        AssetRegistry registry(m_Paths);

        EXPECT_FALSE(registry.loadDatabase(database()));
    }
}

TEST_F(AssetDatabaseTest, DamagedEntriesAreSkippedInsteadOfEndingTheEditor) {
    // Regression: a Guid or path of the wrong type made get<std::string>() throw during startup.
    // A damaged entry now only costs that file its Guid; the intact one keeps its own.
    addAsset("models/intact.obj");
    addAsset("models/a.obj");
    addAsset("models/b.obj");
    const std::string intactGuid = "969c6c10ad374b528d49ea4cab8b5fce";
    m_Dir.write(database(), R"({ "assets": [
        { "guid": 12345, "path": "assets/models/a.obj" },
        { "guid": "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "path": 7 },
        { "path": "assets/models/b.obj" },
        { "guid": "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb", "path": "" },
        { "guid": "not a guid", "path": "assets/models/b.obj" },
        "just a string",
        { "guid": ")" + intactGuid + R"(", "path": "assets/models/intact.obj" }
    ] })");

    AssetRegistry registry(m_Paths);
    ASSERT_TRUE(registry.loadDatabase(database()));
    registry.scanDirectory(assets());

    EXPECT_EQ(registry.getRecords().size(), 3u);
    EXPECT_EQ(guidOf(registry, assets() / "models/intact.obj").toString(), intactGuid);
}

TEST_F(AssetDatabaseTest, DuplicateGuidGoesToOneFileOnly) {
    // A hand-merged file can list the same Guid twice; two records sharing it would break every lookup.
    addAsset("models/a.obj");
    addAsset("models/b.obj");
    const std::string guid = "969c6c10ad374b528d49ea4cab8b5fce";
    m_Dir.write(database(), R"({ "assets": [
        { "guid": ")" + guid + R"(", "path": "assets/models/a.obj" },
        { "guid": ")" + guid + R"(", "path": "assets/models/b.obj" }
    ] })");

    const AssetRegistry registry = startRegistry();

    const Guid a = guidOf(registry, assets() / "models/a.obj");
    const Guid b = guidOf(registry, assets() / "models/b.obj");
    EXPECT_NE(a, b);
    EXPECT_TRUE(a.toString() == guid || b.toString() == guid);
}

TEST_F(AssetDatabaseTest, EntryForADeletedFileCreatesNoRecord) {
    m_Dir.write(database(), R"({ "assets": [
        { "guid": "969c6c10ad374b528d49ea4cab8b5fce", "path": "assets/models/gone.obj" }
    ] })");

    const AssetRegistry registry = startRegistry();

    EXPECT_TRUE(registry.getRecords().empty());
}

TEST_F(AssetDatabaseTest, RenamedFileKeepsItsGuidAfterARestart) {
    const std::filesystem::path file = addAsset("models/apo.obj");
    Guid guid;
    {
        AssetRegistry session = startRegistry();
        guid = guidOf(session, file);
        ASSERT_EQ(session.renameAsset(guid, "crate").status, RenameStatus::Renamed);
        ASSERT_TRUE(session.saveDatabase(database()));
    }

    EXPECT_EQ(guidOf(startRegistry(), assets() / "models/crate.obj"), guid);
}

#ifdef _WIN32
TEST_F(AssetDatabaseTest, LockedFileIsNotMistakenForAMissingOne) {
    // "No file" would let the engine save over it and hand every asset a new Guid. A file that is
    // there but cannot be opened - here: held open exclusively by another program - must be rejected.
    m_Dir.write(database(), R"({ "assets": [] })");
    const HANDLE lock = CreateFileW(database().c_str(), GENERIC_READ, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    ASSERT_NE(lock, INVALID_HANDLE_VALUE);

    AssetRegistry registry(m_Paths);
    const bool loaded = registry.loadDatabase(database());
    CloseHandle(lock);

    EXPECT_FALSE(loaded);
}
#endif
