#pragma once

/**
 * @file a_FolderDialog.hpp
 * @brief Backend-agnostic native folder picker.
 *
 * @details Include this header, never a platform one. On Windows the implementation lives in
 *          modules/os/windows/a_FolderDialog.cpp and opens the Explorer dialog (IFileOpenDialog).
 */

#include <string>
#include <string_view>
#include <vector>

namespace Andromeda::OS {

    /**
     * @brief Opens the native folder picker and blocks until it is closed.
     * @param ownerWindow Native handle of the parent window (an HWND on Windows), may be nullptr.
     *                    With a parent the dialog stays in front of it and blocks it while open.
     * @param startFolder Folder the dialog opens in, as UTF-8; empty for the system default.
     * @return The chosen folders as absolute UTF-8 paths, empty if the dialog was cancelled.
     */
#ifdef _WIN32
    std::vector<std::string> pickFolders(void* ownerWindow, std::string_view startFolder);
#else
    // No native dialog on this platform yet. Returning nothing keeps the Linux build linking;
    // the editor still lets the path be typed in by hand.
    inline std::vector<std::string> pickFolders(void*, std::string_view) { return {}; }
#endif
}
