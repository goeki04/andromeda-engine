#pragma once

/**
 * @file a_file_event.hpp
 * @brief What a file watcher reports, independent of the OS that reported it.
 *
 * @details Lives in `definitions` rather than in `os` because both sides need it and they
 *          sit on different layers: `os` produces these values from the native
 *          notification codes, and a_EventTypes.hpp wraps them in an engine event. Putting
 *          them here keeps `definitions` from having to depend on `os`.
 */

#include <filesystem>
#include <string_view>

namespace Andromeda {

    /**
     * @enum FileAction
     * @brief What happened to a watched file, independent of the native notification codes.
     */
    enum class FileAction {
        Added,       /**< The file or directory appeared. */
        Removed,     /**< The file or directory is gone. */
        Modified,    /**< Contents, size or write time changed. */
        RenamedFrom, /**< Old name of a rename; the matching RenamedTo follows. */
        RenamedTo,   /**< New name of a rename. */
        Rescan       /**< Notifications were dropped - re-read the whole directory. */
    };

    /** @brief One reported change: what happened, and to which path. */
    struct FileEvent {
        FileAction action;
        std::filesystem::path path;
    };

    /** @brief Human readable name of an action, for logging. */
    constexpr std::string_view toString(FileAction action) {
        switch (action) {
            case FileAction::Added:       return "Added";
            case FileAction::Removed:     return "Removed";
            case FileAction::Modified:    return "Modified";
            case FileAction::RenamedFrom: return "RenamedFrom";
            case FileAction::RenamedTo:   return "RenamedTo";
            case FileAction::Rescan:      return "Rescan";
        }
        return "Unknown";
    }
}
