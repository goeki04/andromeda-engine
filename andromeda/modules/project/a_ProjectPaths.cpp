#include "a_ProjectPaths.hpp"

namespace Andromeda {
    namespace {
        // "assets/" -> "assets". Paths without a relative part ("C:/", "/") keep their separator.
        std::filesystem::path withoutTrailingSeparator(std::filesystem::path path) {
            while (!path.has_filename() && path.has_relative_path()) {
                path = path.parent_path();
            }
            return path;
        }
    }

    ProjectPaths::ProjectPaths(const std::filesystem::path& root)
        : m_Root(withoutTrailingSeparator(root.lexically_normal())) {}

    std::filesystem::path ProjectPaths::settingsFile() const {
        return m_Root / "ProjectSettings" / "project.json";
    }

    std::filesystem::path ProjectPaths::resolve(const std::filesystem::path& stored) const {
        // operator/ returns the right-hand side unchanged when it is absolute.
        return withoutTrailingSeparator((m_Root / stored).lexically_normal());
    }

    std::filesystem::path ProjectPaths::toStored(const std::filesystem::path& path) const {
        const std::filesystem::path normal = withoutTrailingSeparator(path.lexically_normal());
        if (normal.is_relative()) {
            return normal;
        }
        const std::filesystem::path relative = normal.lexically_relative(m_Root);
        const bool insideProject = !relative.empty() && *relative.begin() != "..";
        return insideProject ? relative : normal;
    }
}
