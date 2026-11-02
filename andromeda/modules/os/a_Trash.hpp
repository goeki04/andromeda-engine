#pragma once
#include <filesystem>
namespace Andromeda::OS {
	#ifdef _WIN32
    bool moveToTrash(const std::filesystem::path& path);
	#else
    inline bool moveToTrash(const std::filesystem::path& path) {
        return false;
    }
	#endif
}