#pragma once
#include <filesystem>

namespace Andromeda::OS{

#if defined(_WIN32) || defined(__APPLE__)
    bool revealInFileBrowser(const std::filesystem::path& path);
    #else
    inline bool revealInFileBrowser(const std::filesystem::path& path){
        return false;
    }
    #endif
}
