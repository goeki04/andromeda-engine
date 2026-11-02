#pragma once
#include <filesystem>

namespace Andromeda::OS{

    #ifdef _WIN32
    bool revealInFileBrowser(const std::filesystem::path& path);
    #else
    inline bool revealInFileBrowser(const std::filesystem::path& path){
        return false;
    }
    #endif
}