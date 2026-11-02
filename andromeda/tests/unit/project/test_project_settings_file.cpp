// project.json: what loads, what counts as broken, and that a broken file is never overwritten.

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include "a_ProjectSettingsFile.hpp"
#include "a_TempDirectory.hpp"

using namespace Andromeda;
using Andromeda::Test::TempDirectory;
namespace SettingsFile = Andromeda::ProjectSettingsFile;

TEST(ProjectSettingsFile, MissingFileIsReportedAsMissingWithDefaults) {
    TempDirectory dir;

    const SettingsFile::LoadResult result = SettingsFile::load(dir / "project.json");

    EXPECT_EQ(result.status, SettingsFile::Status::Missing);
    EXPECT_EQ(result.settings.general.projectName, ProjectSettings{}.general.projectName);
    EXPECT_EQ(result.settings.assets.directories, ProjectSettings{}.assets.directories);
}

TEST(ProjectSettingsFile, SaveThenLoadGivesBackEveryValue) {
    TempDirectory dir;
    ProjectSettings settings;
    settings.general.projectName = "Nebula";
    settings.general.version = "2.3.4";
    settings.general.startupScene = "scenes/main.scene";
    settings.general.reopenLastScene = false;
    settings.general.autosave = false;
    settings.general.autosaveMinutes = 17;
    settings.rendering.graphicsApi = 2;
    settings.rendering.vsync = false;
    settings.rendering.msaaSamples = 8;
    settings.assets.directories = { "assets", "C:/Shared/Assets", "\xC3\xBC" "ber" };
    settings.assets.databaseDirectory = "Data";

    ASSERT_TRUE(SettingsFile::save(dir / "project.json", settings));
    const SettingsFile::LoadResult result = SettingsFile::load(dir / "project.json");

    ASSERT_EQ(result.status, SettingsFile::Status::Loaded);
    EXPECT_EQ(nlohmann::json(result.settings), nlohmann::json(settings));
}

TEST(ProjectSettingsFile, KeysMissingFromTheFileKeepTheirDefault) {
    // A file written before a setting existed must still load.
    TempDirectory dir;
    dir.write("project.json", R"({ "general": { "projectName": "Nebula" } })");

    const SettingsFile::LoadResult result = SettingsFile::load(dir / "project.json");

    ASSERT_EQ(result.status, SettingsFile::Status::Loaded);
    EXPECT_EQ(result.settings.general.projectName, "Nebula");
    EXPECT_EQ(result.settings.general.autosaveMinutes, ProjectSettings{}.general.autosaveMinutes);
    EXPECT_EQ(result.settings.rendering.msaaSamples, ProjectSettings{}.rendering.msaaSamples);
    EXPECT_EQ(result.settings.assets.directories, ProjectSettings{}.assets.directories);
}

TEST(ProjectSettingsFile, UnknownKeysAreIgnored) {
    // A file written by a newer version with settings this build does not know.
    TempDirectory dir;
    dir.write("project.json", R"({ "general": { "projectName": "Nebula", "futureSetting": 1 }, "physics": {} })");

    const SettingsFile::LoadResult result = SettingsFile::load(dir / "project.json");

    ASSERT_EQ(result.status, SettingsFile::Status::Loaded);
    EXPECT_EQ(result.settings.general.projectName, "Nebula");
}

TEST(ProjectSettingsFile, BrokenJsonIsInvalidAndKeepsTheDefaults) {
    TempDirectory dir;
    dir.write("project.json", R"({ "general": { "projectName": )");

    const SettingsFile::LoadResult result = SettingsFile::load(dir / "project.json");

    EXPECT_EQ(result.status, SettingsFile::Status::Invalid);
    EXPECT_FALSE(result.error.empty());
    EXPECT_EQ(result.settings.general.projectName, ProjectSettings{}.general.projectName);
}

TEST(ProjectSettingsFile, ValueOfTheWrongTypeMakesTheFileInvalid) {
    // Rather than dropping just that one setting silently: the user should learn the file is off.
    TempDirectory dir;
    dir.write("project.json", R"({ "rendering": { "vsync": "yes" } })");

    const SettingsFile::LoadResult result = SettingsFile::load(dir / "project.json");

    EXPECT_EQ(result.status, SettingsFile::Status::Invalid);
    EXPECT_FALSE(result.error.empty());
}

TEST(ProjectSettingsFile, LoadingABrokenFileDoesNotChangeIt) {
    // The ResourceManager relies on this to leave the file for repair.
    TempDirectory dir;
    const std::string broken = "{ not json";
    dir.write("project.json", broken);

    (void)SettingsFile::load(dir / "project.json");

    EXPECT_EQ(dir.read("project.json"), broken);
}

TEST(ProjectSettingsFile, SaveReportsWhyItFailed) {
    TempDirectory dir;
    std::filesystem::create_directories(dir / "project.json"); // a folder where the file should go
    std::string error;

    EXPECT_FALSE(SettingsFile::save(dir / "project.json", ProjectSettings{}, &error));
    EXPECT_FALSE(error.empty());
}

TEST(ProjectSettingsFile, TextThatIsNotUtf8IsReportedNotThrown) {
    // ImGui fields deliver UTF-8, but a broken byte sequence must fail the save, not end the editor.
    TempDirectory dir;
    ProjectSettings settings;
    settings.general.projectName = "\xC3\x28"; // invalid UTF-8
    std::string error;

    EXPECT_FALSE(SettingsFile::save(dir / "project.json", settings, &error));
    EXPECT_FALSE(error.empty());
    EXPECT_FALSE(std::filesystem::exists(dir / "project.json"));
}
