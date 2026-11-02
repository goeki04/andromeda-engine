#pragma once

/**
 * @file a_TempDirectory.hpp
 * @brief A private folder per test, removed again afterwards.
 */

#include <filesystem>
#include <string>
#include <string_view>

namespace Andromeda::Test {

    /**
     * @class TempDirectory
     * @brief A fresh, empty folder for one test, deleted when the object goes out of scope.
     * @details Created as <system temp>/andromeda_tests/<Suite>.<Test>_<random>, so a folder left behind
     *          by a crashed run can still be traced back to its test. Tests must never touch the real
     *          project folders (assets/, ProjectSettings/) - they create what they need in here.
     */
    class TempDirectory {
    public:
        TempDirectory();
        ~TempDirectory();
        TempDirectory(const TempDirectory&) = delete;
        TempDirectory& operator=(const TempDirectory&) = delete;

        [[nodiscard]] const std::filesystem::path& path() const { return m_Path; }

        /** @brief path() / @p relative - a location inside the folder, not necessarily existing. */
        [[nodiscard]] std::filesystem::path operator/(const std::filesystem::path& relative) const;

        /**
         * @brief Writes @p content to @p relative, creating missing folders on the way.
         * @param relative Path inside the folder; an absolute path is used as it is. UTF-8 file names
         *                 have to be built with Filesystem::fromUtf8(), a plain std::string is read in
         *                 the system code page.
         * @return The full path of the written file.
         */
        std::filesystem::path write(const std::filesystem::path& relative, std::string_view content = {}) const;

        /** @brief The bytes of @p relative (or of an absolute path), or "" if it cannot be read. */
        [[nodiscard]] std::string read(const std::filesystem::path& relative) const;

    private:
        std::filesystem::path m_Path;
    };
}
