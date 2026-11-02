#pragma once

/**
 * @file a_ProjectSettingsFile.hpp
 * @brief Reading and writing project.json.
 */

#include <filesystem>
#include <string>
#include "a_ProjectSettings.hpp"

namespace Andromeda::ProjectSettingsFile {

    /** @brief What load() found. */
    enum class Status {
        Loaded,  ///< The file was read; LoadResult::settings holds its values.
        Missing, ///< There is no file yet - a new project. Writing the defaults is safe.
        Invalid, ///< The file exists but could not be used. It must not be overwritten, so it can be repaired.
    };

    struct LoadResult {
        Status status = Status::Missing;
        ProjectSettings settings; ///< The file's values when Loaded, the defaults otherwise.
        std::string error;        ///< Why the file is Invalid, for the log; empty otherwise.
    };

    /**
     * @brief Reads project settings from @p file.
     * @details Keys missing from the file keep their default value and unknown keys are ignored, so
     *          files written by older or newer versions still load. A value of the wrong type makes
     *          the whole file Invalid rather than silently dropping that one setting.
     */
    [[nodiscard]] LoadResult load(const std::filesystem::path& file);

    /**
     * @brief Writes @p settings to @p file, replacing it atomically (see Filesystem::writeFileAtomic).
     * @param error Receives the reason when saving fails; may be null.
     * @return False if the settings could not be turned into JSON or the file not be written.
     */
    bool save(const std::filesystem::path& file, const ProjectSettings& settings, std::string* error = nullptr);
}
