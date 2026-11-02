#pragma once

/**
 * @file a_AssetTestProject.hpp
 * @brief A throwaway project folder for the asset registry tests.
 */

#include <gtest/gtest.h>
#include <filesystem>
#include <string>
#include <string_view>
#include "a_AssetRegistry.hpp"
#include "a_ProjectPaths.hpp"
#include "a_TempDirectory.hpp"

namespace Andromeda::Test {

    /**
     * @brief Fixture with an empty project "Game" in a temp folder: Game/assets and Game/ProjectSettings.
     * @details Paths given to the helpers are UTF-8 and relative to Game/assets, so tests read like the
     *          project tree they build.
     */
    class AssetTestProject : public ::testing::Test {
    protected:
        TempDirectory m_Dir;
        std::filesystem::path m_Root = (m_Dir.path() / "Game").lexically_normal();
        ProjectPaths m_Paths{ m_Root };

        [[nodiscard]] std::filesystem::path assets() const { return m_Root / "assets"; }
        [[nodiscard]] std::filesystem::path database() const { return m_Root / "ProjectSettings" / "asset_db.json"; }

        /** @brief Creates assets/@p relative and returns its full path. */
        std::filesystem::path addAsset(std::string_view relative, std::string_view content = "data") const {
            return m_Dir.write(std::filesystem::path("Game") / "assets" / Filesystem::fromUtf8(relative), content);
        }

        /** @brief The key the registry uses for @p file: normalized, '/', UTF-8. */
        [[nodiscard]] static std::string keyOf(const std::filesystem::path& file) {
            return Filesystem::toUtf8(file.lexically_normal());
        }

        /** @brief A registry as the engine builds it at startup: database read, assets/ scanned. */
        [[nodiscard]] AssetManagement::AssetRegistry startRegistry() const {
            AssetManagement::AssetRegistry registry(m_Paths);
            EXPECT_TRUE(registry.loadDatabase(database()));
            registry.scanDirectory(assets());
            return registry;
        }

        /** @brief The Guid of the record for @p file; fails the test if there is none. */
        [[nodiscard]] static Guid guidOf(const AssetManagement::AssetRegistry& registry, const std::filesystem::path& file) {
            const AssetManagement::AssetRecord* record = registry.findByPath(file);
            EXPECT_NE(record, nullptr) << "no record for " << keyOf(file);
            return record ? record->guid : Guid{};
        }
    };
}
