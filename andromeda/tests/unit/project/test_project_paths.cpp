// How project files store paths: relative inside the project, so the project survives being moved.

#include <gtest/gtest.h>
#include "a_ProjectPaths.hpp"
#include "a_TempDirectory.hpp"

using Andromeda::ProjectPaths;
using Andromeda::Test::TempDirectory;

class ProjectPathsTest : public ::testing::Test {
protected:
    TempDirectory m_Dir;
    ProjectPaths m_Paths{ m_Dir.path() / "MyGame" };
    std::filesystem::path m_Root = (m_Dir.path() / "MyGame").lexically_normal();
};

TEST_F(ProjectPathsTest, TrailingSeparatorOfTheRootIsIgnored) {
    const ProjectPaths withSlash(m_Root.string() + "/");

    EXPECT_EQ(withSlash.root(), m_Paths.root());
    EXPECT_EQ(withSlash.toStored(m_Root / "assets"), "assets");
}

TEST_F(ProjectPathsTest, SettingsFileHasAFixedPlace) {
    EXPECT_EQ(m_Paths.settingsFile(), m_Root / "ProjectSettings" / "project.json");
}

TEST_F(ProjectPathsTest, PathInsideTheProjectIsStoredRelative) {
    EXPECT_EQ(m_Paths.toStored(m_Root / "assets" / "models"), std::filesystem::path("assets/models"));
}

TEST_F(ProjectPathsTest, PathOutsideTheProjectIsStoredAbsolute) {
    const std::filesystem::path shared = m_Dir.path() / "SharedAssets";

    EXPECT_EQ(m_Paths.toStored(shared), shared.lexically_normal());
}

TEST_F(ProjectPathsTest, SiblingFolderWithTheSameNamePrefixIsOutside) {
    // "MyGameBackup" starts with "MyGame" as text but is not inside it.
    const std::filesystem::path sibling = m_Dir.path() / "MyGameBackup" / "assets";

    EXPECT_TRUE(m_Paths.toStored(sibling).is_absolute());
}

TEST_F(ProjectPathsTest, StoredFormHasNoTrailingSeparator) {
    // Otherwise "assets" and "assets/" become two entries in the directory list.
    EXPECT_EQ(m_Paths.toStored(m_Root / "assets/"), std::filesystem::path("assets"));
    EXPECT_EQ(m_Paths.toStored("assets/"), std::filesystem::path("assets"));
}

TEST_F(ProjectPathsTest, StoredFormIsNormalized) {
    EXPECT_EQ(m_Paths.toStored(m_Root / "assets/./models/../textures"), std::filesystem::path("assets/textures"));
}

TEST_F(ProjectPathsTest, RelativeInputIsTakenAsRelativeToTheRoot) {
    // A path typed into the settings panel, e.g. "assets/sounds".
    EXPECT_EQ(m_Paths.toStored("assets/sounds"), std::filesystem::path("assets/sounds"));
}

TEST_F(ProjectPathsTest, ResolveAnchorsRelativePathsAtTheRoot) {
    EXPECT_EQ(m_Paths.resolve("assets/models"), m_Root / "assets" / "models");
}

TEST_F(ProjectPathsTest, ResolveKeepsAbsolutePaths) {
    const std::filesystem::path shared = (m_Dir.path() / "SharedAssets").lexically_normal();

    EXPECT_EQ(m_Paths.resolve(shared), shared);
}

TEST_F(ProjectPathsTest, ResolveUndoesToStored) {
    for (const std::filesystem::path& real : { m_Root / "assets" / "models", (m_Dir.path() / "Shared").lexically_normal(), m_Root }) {
        SCOPED_TRACE(real.string());
        EXPECT_EQ(m_Paths.resolve(m_Paths.toStored(real)), real);
    }
}

#ifdef _WIN32
TEST(ProjectPaths, DriveRootKeepsItsSeparator) {
    // "C:" without the slash means "the current directory of drive C" - a different folder.
    const ProjectPaths paths("D:/Projects/MyGame");

    EXPECT_EQ(paths.toStored("C:/"), std::filesystem::path("C:/"));
}
#endif
