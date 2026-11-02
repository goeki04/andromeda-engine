// Which files become assets, and which names an asset file may get.

#include <gtest/gtest.h>
#include <cctype>
#include <string>
#include <vector>
#include "a_AssetRegistry.hpp"

using namespace Andromeda::AssetManagement;

// Compile-time check: getAssetTypeFromExtension() is constexpr, and stays so - the build stops here otherwise.
static_assert(getAssetTypeFromExtension(".OBJ") == AssetType::Mesh);

TEST(AssetType, EveryListedExtensionMapsToItsTypeInAnyCase) {
    // Driven by the table itself, so a newly added extension is covered without touching this test.
    for (const auto& [extension, type] : AssetExtensions) {
        std::string upper(extension);
        for (char& c : upper) {
            c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        }
        SCOPED_TRACE(std::string(extension));
        EXPECT_EQ(getAssetTypeFromExtension(std::string(extension)), type);
        EXPECT_EQ(getAssetTypeFromExtension(upper), type);
    }
}

TEST(AssetType, UnknownOrMissingExtensionIsUnknown) {
    EXPECT_EQ(getAssetTypeFromExtension(".xyz"), AssetType::Unknown);
    EXPECT_EQ(getAssetTypeFromExtension(""), AssetType::Unknown);
    EXPECT_EQ(getAssetTypeFromExtension("."), AssetType::Unknown);
    EXPECT_EQ(getAssetTypeFromExtension("obj"), AssetType::Unknown); // without the dot it is not an extension
}

TEST(AssetType, PathIsClassifiedByItsLastExtensionOnly) {
    EXPECT_EQ(getAssetTypeFromPath("assets/models/Room 2.1.obj"), AssetType::Mesh);
    EXPECT_EQ(getAssetTypeFromPath("assets/models/backup.obj.bak"), AssetType::Unknown);
}

TEST(AssetType, DotfileHasNoExtension) {
    // A file literally called ".json" is a hidden file, not a JSON asset.
    EXPECT_EQ(getAssetTypeFromPath("assets/.json"), AssetType::Unknown);
    EXPECT_EQ(getAssetTypeFromPath("assets/.gitignore"), AssetType::Unknown);
}

TEST(AssetFileName, AcceptsOrdinaryNames) {
    for (const std::string name : { "apo", "Room 2.1", "über", "CONSOLE", "COM10", " leading space" }) {
        SCOPED_TRACE(name);
        EXPECT_TRUE(isValidAssetFileName(name));
    }
}

TEST(AssetFileName, RejectsEmptyName) {
    EXPECT_FALSE(isValidAssetFileName(""));
}

TEST(AssetFileName, RejectsCharactersWindowsRefuses) {
    for (const char c : std::string("\\/:*?\"<>|\t")) {
        const std::string name = std::string("a") + c + "b";
        SCOPED_TRACE(name);
        EXPECT_FALSE(isValidAssetFileName(name));
    }
}

TEST(AssetFileName, RejectsTrailingDotOrSpace) {
    // Windows silently drops them: "apo." would be created as "apo", and the registry would point
    // at a file that does not exist.
    EXPECT_FALSE(isValidAssetFileName("apo."));
    EXPECT_FALSE(isValidAssetFileName("apo "));
    EXPECT_FALSE(isValidAssetFileName(".."));
}

TEST(AssetFileName, RejectsReservedDeviceNamesInAnyCase) {
    for (const std::string name : { "CON", "con", "PRN", "AUX", "NUL", "COM1", "com9", "LPT1", "LPT9" }) {
        SCOPED_TRACE(name);
        EXPECT_FALSE(isValidAssetFileName(name));
    }
}
