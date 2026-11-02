#include "a_filesystem.hpp"
#include "a_logger.hpp"
#include <filesystem>
#include <cctype>
#include <ranges>
#include <fstream>
namespace Andromeda {

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
            Filesystem::stringToLower(ext);
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

    std::string Filesystem::getFileExtension(const std::string_view path)
    {
        // std::filesystem rather than a hand-rolled find_last_of('.'): it knows that a leading
        // dot belongs to the name, so ".gitignore" comes back without an extension instead of
        // being read as one.
        return std::filesystem::path(path).extension().string();
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

    bool Filesystem::writeFileAtomic(const std::filesystem::path& file, const std::string_view text)
    {
        namespace fs = std::filesystem;
        std::error_code ec;
        if (file.has_parent_path()) {
            fs::create_directories(file.parent_path(), ec);
        }

        fs::path temp = file;
        temp += ".tmp";
        bool written = false;
        {
            std::ofstream out(temp, std::ios::binary | std::ios::trunc);
            out.write(text.data(), static_cast<std::streamsize>(text.size()));
            written = static_cast<bool>(out);
        }
        if (written) {
            fs::rename(temp, file, ec);
            if (!ec) {
                return true;
            }
        }
        fs::remove(temp, ec);
        return false;
    }

    std::filesystem::path Filesystem::fromUtf8(const std::string_view utf8)
    {
        return std::filesystem::path(std::u8string(utf8.begin(), utf8.end()));
    }

    std::string Filesystem::toUtf8(const std::filesystem::path& path)
    {
        const std::u8string text = path.generic_u8string();
        return std::string(text.begin(), text.end());
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
