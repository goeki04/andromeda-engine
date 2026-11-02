#pragma once

/**
 * @file a_ProjectPaths.hpp
 * @brief Conversion between paths as project files store them and real paths on disk.
 */

#include <filesystem>

namespace Andromeda {

    /**
     * @class ProjectPaths
     * @brief Knows the project root and turns stored paths into real ones and back.
     * @details Project files (project.json, asset_db.json) keep paths inside the project relative to
     *          its root, so a project still works after its folder was moved or checked out somewhere
     *          else. Paths outside the project stay absolute. Everything that reads or writes such a
     *          path goes through this class, so that rule exists exactly once.
     *
     *          Works on std::filesystem::path only. Turning a path into text is the caller's choice:
     *          Filesystem::toUtf8() for files and the UI.
     */
    class ProjectPaths {
    public:
        /**
         * @param root The project folder, the one that contains assets/ and ProjectSettings/.
         *             A trailing separator is ignored.
         */
        explicit ProjectPaths(const std::filesystem::path& root);

        /** @brief The project folder, normalized and without trailing separator. */
        [[nodiscard]] const std::filesystem::path& root() const { return m_Root; }

        /**
         * @brief Where project.json lives.
         * @details Fixed rather than configurable: the file that configures folders cannot itself be
         *          found through a configured folder.
         */
        [[nodiscard]] std::filesystem::path settingsFile() const;

        /**
         * @brief Stored path -> real path.
         * @details A relative path is anchored at the root, an absolute one is returned as it is.
         *          The result is lexically normalized ("a/./b/../c" becomes "a/c").
         */
        [[nodiscard]] std::filesystem::path resolve(const std::filesystem::path& stored) const;

        /**
         * @brief Real path -> stored path.
         * @details Relative to the root when the path lies inside the project, absolute otherwise.
         *          A path that is relative already is taken as relative to the root. The result is
         *          normalized and has no trailing separator, so "assets" and "assets/" end up as the
         *          same entry. A drive root such as "C:/" keeps its separator - without it, "C:" would
         *          mean the current directory of drive C.
         */
        [[nodiscard]] std::filesystem::path toStored(const std::filesystem::path& path) const;

    private:
        std::filesystem::path m_Root;
    };
}
