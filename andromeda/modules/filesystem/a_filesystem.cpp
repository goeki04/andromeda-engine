#include "a_filesystem.hpp"
#include "a_logger.hpp"
#include <filesystem>
#include <cctype>
#include <ranges>
#include <fstream>
namespace Andromeda {
    /// <summary>
    /// This function sets the device type based on the path.
    /// Sets the device type to default if not found.
    /// </summary>
    /// <param name="path">path which used to determine the device type based on the directory</param>
    /// <returns></returns>
    deviceType Filesystem::findDeviceTypeByPath(const std::string& path) {
        std::string lowerString = path;
        stringToLower(lowerString);
        for (auto& [key, value] : m_DirectoryNames) {
            if (lowerString.find(key) != std::string::npos) {
                return value;
            }
        }
        A_WARN("Device type not found for path: {}", path);
        return deviceType::DEFAULT;
    }

    std::vector<std::string> Filesystem::getAllFilesInDirectoryRecursive(const std::string& directory, std::span<const std::string> filter)
    {
        std::vector<std::string> filePaths;
        std::error_code ec;

        std::filesystem::recursive_directory_iterator it(directory, std::filesystem::directory_options::skip_permission_denied, ec);

        for (const auto& entry : it) {
            if (ec) break;
            if (!entry.is_regular_file(ec)) continue;

            std::string ext = entry.path().extension().generic_string();
            Filesystem::stringToLower(ext);
            const bool allowed =
                filter.empty() ||
                std::ranges::find(filter, ext) != filter.end();

            if (allowed) {
                filePaths.emplace_back(entry.path().generic_string());
            }
        }

        return filePaths;
    }

    std::vector<std::string> Filesystem::getAllFilesInDirectory(std::string_view directory, std::span<const std::string_view> filter)
    {
        std::vector<std::string> filePaths;
        std::error_code ec;
        for (const auto& entry : std::filesystem::directory_iterator(directory, ec)) {
            if (ec) break;
            if (!entry.is_regular_file()) continue;

            std::string ext = entry.path().extension().generic_string();
            const bool allowed = filter.empty() ||
                std::ranges::find(filter, ext) != filter.end();

            if (allowed) {
                filePaths.emplace_back(entry.path().generic_string());
            }
        }
        return filePaths;
    }

    std::vector<std::string> Filesystem::getAllFilesInDirectory(const std::string& directory)
    {
        std::vector<std::string> filePaths;
        std::error_code ec;
        for (const auto& entry : std::filesystem::directory_iterator(directory, ec)) {
            if (ec) break;
            if (!entry.is_regular_file()) continue;
            filePaths.emplace_back(entry.path().generic_string());
        }
        return filePaths;
    }


    std::string Filesystem::getFileName(const std::string& path)
    {
        // Both separators: paths reaching this function come from two sources with different
        // spellings - the directory scans above hand out generic_string() with '/', while
        // anything originating from a Win32 API (the file watcher, drag and drop) carries the
        // native '\'. Matching only '/' silently returned the whole path as the "file name".
        const size_t namePos = path.find_last_of("/\\");
        const std::string objectName = (namePos == std::string::npos) ? path : path.substr(namePos + 1);

        const size_t dotPos = objectName.find_last_of('.');
        std::string fileName = (dotPos == std::string::npos) ? objectName : objectName.substr(0, dotPos);
        return fileName;
    }

    std::string Filesystem::readFile(const std::string& path)
    {
        std::ifstream file(path, std::ios::in | std::ios::binary);

        if (!file.is_open()) {
            throw std::runtime_error("Could not open file: " + path);
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }

    /// Returns a list of directory names within the specified path
    std::vector<Filesystem::Directory> Filesystem::getAllDirectories(const std::string& path)
    {
        std::vector<Directory> directories;
        std::error_code ec;

        if (!std::filesystem::exists(path, ec) || !std::filesystem::is_directory(path, ec)) {
            return directories;
        }

        for (const auto& entry : std::filesystem::directory_iterator(path, ec)) {
            if (ec) break;
            if (entry.is_directory()) {
                std::string directoryName = entry.path().filename().generic_string();
                std::string combinedDirectoryPath = entry.path().generic_string();
                std::vector<std::string> fileNames = getAllFilesInDirectory(combinedDirectoryPath);
                directories.emplace_back(directoryName, fileNames);
            }
        }

        return directories;
    }
}
