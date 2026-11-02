// Path helpers, directory listings and the atomic write every project file goes through.

#include <gtest/gtest.h>
#include <algorithm>
#include <array>
#include <string>
#include <vector>
#include "a_filesystem.hpp"
#include "a_TempDirectory.hpp"

using namespace Andromeda;
using Andromeda::Test::TempDirectory;

namespace {
    // Listings come back in directory order, which the file system decides - compare them sorted.
    std::vector<std::string> sorted(std::vector<std::string> paths) {
        std::ranges::sort(paths);
        return paths;
    }
}

TEST(Filesystem, StringToLower_ChangesOnlyAsciiLetters) {
    std::string text = "AbC-1_\xC3\x84"; // "Ä" in UTF-8 must come through byte for byte
    Filesystem::stringToLower(text);
    EXPECT_EQ(text, "abc-1_\xC3\x84");
}

TEST(Filesystem, GetFileName_AcceptsBothSeparators) {
    // Directory scans hand out '/', Win32 APIs (file watcher, drag and drop) '\'.
    EXPECT_EQ(Filesystem::getFileName("assets/models/apo.obj"), "apo");
    EXPECT_EQ(Filesystem::getFileName("assets\\models\\apo.obj"), "apo");
}

TEST(Filesystem, GetFileName_RemovesOnlyTheLastExtension) {
    EXPECT_EQ(Filesystem::getFileName("assets/models/Room 2.1.obj"), "Room 2.1");
    EXPECT_EQ(Filesystem::getFileName("assets/README"), "README");
}

TEST(Filesystem, GetFileExtension_FollowsTheDotfileRule) {
    EXPECT_EQ(Filesystem::getFileExtension("assets/archive.tar.gz"), ".gz");
    EXPECT_EQ(Filesystem::getFileExtension("assets/.gitignore"), "");
    EXPECT_EQ(Filesystem::getFileExtension("assets/README"), "");
}

TEST(Filesystem, RecursiveListing_FiltersExtensionsCaseInsensitively) {
    TempDirectory dir;
    dir.write("a.obj");
    dir.write("sub/deeper/B.OBJ");
    dir.write("sub/c.png");
    const std::array<std::string, 1> filter{ ".obj" };

    const std::vector<std::string> found = Filesystem::getAllFilesInDirectoryRecursive(dir.path().string(), filter);

    EXPECT_EQ(sorted(found), sorted({ (dir / "a.obj").generic_string(), (dir / "sub/deeper/B.OBJ").generic_string() }));
}

TEST(Filesystem, FlatListing_FiltersCaseInsensitivelyLikeTheRecursiveOne) {
    // Regression: the flat listing compared extensions as written on disk, so an icon saved as
    // ".PNG" was missing while the recursive listing found it.
    TempDirectory dir;
    dir.write("icon.PNG");
    dir.write("notes.txt");
    const std::array<std::string_view, 1> filter{ ".png" };

    const std::vector<std::string> found = Filesystem::getAllFilesInDirectory(dir.path().string(), filter);

    EXPECT_EQ(found, std::vector<std::string>{ (dir / "icon.PNG").generic_string() });
}

TEST(Filesystem, FlatListing_DoesNotDescendIntoSubfolders) {
    TempDirectory dir;
    dir.write("top.txt");
    dir.write("sub/nested.txt");

    EXPECT_EQ(Filesystem::getAllFilesInDirectory(dir.path().string()),
              std::vector<std::string>{ (dir / "top.txt").generic_string() });
}

TEST(Filesystem, MissingDirectory_GivesEmptyListingsInsteadOfThrowing) {
    TempDirectory dir;
    const std::string missing = (dir / "does_not_exist").string();

    EXPECT_TRUE(Filesystem::getAllFilesInDirectoryRecursive(missing, {}).empty());
    EXPECT_TRUE(Filesystem::getAllFilesInDirectory(missing).empty());
    EXPECT_TRUE(Filesystem::getAllDirectories(missing).empty());
}

TEST(Filesystem, WriteFileAtomic_ReplacesTheWholeContent) {
    TempDirectory dir;
    dir.write("settings.json", "a much longer old content");

    ASSERT_TRUE(Filesystem::writeFileAtomic(dir / "settings.json", "new"));

    EXPECT_EQ(dir.read("settings.json"), "new");
    EXPECT_FALSE(std::filesystem::exists(dir / "settings.json.tmp"));
}

TEST(Filesystem, WriteFileAtomic_KeepsLineEndingsAsGiven) {
    // Binary mode: on Windows a text-mode stream would turn every "\n" into "\r\n", so the same
    // project file would differ byte for byte between platforms.
    TempDirectory dir;

    ASSERT_TRUE(Filesystem::writeFileAtomic(dir / "file.json", "{\n}\n"));

    EXPECT_EQ(dir.read("file.json"), "{\n}\n");
}

TEST(Filesystem, WriteFileAtomic_CreatesMissingFolders) {
    TempDirectory dir;

    ASSERT_TRUE(Filesystem::writeFileAtomic(dir / "ProjectSettings/nested/asset_db.json", "{}"));

    EXPECT_EQ(dir.read("ProjectSettings/nested/asset_db.json"), "{}");
}

TEST(Filesystem, WriteFileAtomic_FailsCleanlyWhenTheTargetCannotBeReplaced) {
    // A folder in the target's place cannot be replaced by a file: the write must report it and
    // leave no temporary file behind.
    TempDirectory dir;
    std::filesystem::create_directories(dir / "blocked");

    EXPECT_FALSE(Filesystem::writeFileAtomic(dir / "blocked", "content"));

    EXPECT_TRUE(std::filesystem::is_directory(dir / "blocked"));
    EXPECT_FALSE(std::filesystem::exists(dir / "blocked.tmp"));
}

TEST(Filesystem, Utf8_RoundTripsNonAsciiPaths) {
    const std::string text = "modelle/\xC3\xBC" "ber/\xE6\x97\xA5\xE6\x9C\xAC.obj"; // modelle/über/日本.obj

    EXPECT_EQ(Filesystem::toUtf8(Filesystem::fromUtf8(text)), text);
}

TEST(Filesystem, FromUtf8_CreatesTheFileUnderItsRealName) {
    // The point of fromUtf8(): built from a plain std::string, Windows would read the bytes in the
    // system code page and create "Ã¼ber.txt" instead.
    TempDirectory dir;
    const std::string name = "\xC3\xBC" "ber.txt"; // über.txt
    dir.write(Filesystem::fromUtf8(name), "x");

    std::vector<std::string> names;
    for (const auto& entry : std::filesystem::directory_iterator(dir.path())) {
        names.push_back(Filesystem::toUtf8(entry.path().filename()));
    }
    EXPECT_EQ(names, std::vector<std::string>{ name });
}

TEST(Filesystem, ToUtf8_DoesNotThrowForCharactersOutsideTheCodePage) {
    // path::string() throws on Windows for characters the system code page lacks.
    const std::filesystem::path path = Filesystem::fromUtf8("\xF0\x9F\x98\x80.obj"); // an emoji

    EXPECT_NO_THROW({ (void)Filesystem::toUtf8(path); });
}
