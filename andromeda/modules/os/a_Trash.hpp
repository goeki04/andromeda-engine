#pragma once
#include <filesystem>
namespace Andromeda::OS {
	#if defined(_WIN32) || defined(__APPLE__)
    bool moveToTrash(const std::filesystem::path& path);
	#else
    inline bool moveToTrash(const std::filesystem::path& path) {
        return false;
    }
	#endif
}
