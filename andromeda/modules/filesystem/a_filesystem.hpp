#pragma once

/**
 * @file a_filesystem.hpp
 * @brief Directory/file enumeration helpers, atomic file writes and UTF-8 path conversion.
 */

#include <array>
#include <string_view>
#include <string>
#include <algorithm>
#include <filesystem>
#include <span>
#include <vector>
namespace Andromeda {
	/**
	 * @namespace Andromeda::Filesystem
	 * @brief Free functions and helpers for traversing the asset directory tree.
	 */
	namespace Filesystem{

		/**
		 * @struct Directory
		 * @brief A directory's name together with the list of files it contains.
		 */
		struct Directory
		{
			std::string name;                ///< Name of the directory (typically the leaf folder name).
			std::vector<std::string> files;  ///< Paths/names of the files contained within.
		};

		/**
		 * @brief Converts a string to lowercase in place (ASCII).
		 * @param data The string to transform; modified directly.
		 */
		/**
		 * @brief Returns the directory a file path points into, without the file name.
		 * @return The directory, or "." when the path has no directory part at all.
		 */
		inline std::string getDirectory(const std::string& path) {
			const std::filesystem::path parent = std::filesystem::path(path).parent_path();
			return parent.empty() ? std::string(".") : parent.generic_string();
		}

		/**
		 * @brief Resolves a path that a data file stated relative to its own location.
		 * @details Model files name their textures relative to themselves. An already absolute
		 *          path is returned unchanged, so files that hardcode one still work.
		 * @param baseDirectory Directory the relative path is anchored to.
		 * @param relative Path as written in the data file.
		 */
		inline std::string resolveRelativeTo(const std::string& baseDirectory, const std::string& relative) {
			if (relative.empty()) {
				return {};
			}
			std::filesystem::path candidate(relative);
			if (candidate.is_absolute()) {
				return candidate.generic_string();
			}
			return (std::filesystem::path(baseDirectory) / candidate).lexically_normal().generic_string();
		}

        /**
         * @brief Converts all ASCII uppercase letters in a string to lowercase, in place.
         * @details Only 'A'-'Z' are changed; every other byte, including UTF-8 multi-byte
         *          sequences such as umlauts, is left untouched. Unlike std::tolower this does
         *          not depend on the current locale, so the result is the same on every machine,
         *          and it is usable in constant expressions.
         *          Intended for case-insensitive comparison of identifiers like file extensions,
         *          not for natural-language text.
         * @param data The string to convert; modified in place.
         */
		constexpr void stringToLower(std::string& data) {
            std::ranges::transform(data, data.begin(), [](const char c) {
                return (c >= 'A' && c <= 'Z') ? static_cast<char>(c + ('a' - 'A')) : c;
            });
        }

		/**
		 * @brief Recursively collects all files under a directory whose extension matches the filter.
		 * @param directory Root directory to search.
		 * @param filter Allowed file extensions in lowercase, compared case-insensitively like in
		 *               getAllFilesInDirectory(). An empty span accepts all files.
		 * @return Paths of all matching files found at any depth.
		 */
		std::vector<std::string> getAllFilesInDirectoryRecursive(const std::string& directory, std::span<const std::string> filter);
		/**
		 * @brief Collects files directly inside a directory (non-recursive) matching the filter.
		 * @param directory Directory to search.
		 * @param filter Allowed file extensions in lowercase; a file's extension is lowercased before
		 *               the comparison, so ".PNG" on disk matches ".png". An empty span accepts all files.
		 * @return Paths of the matching files in the top level of @p directory.
		 */
		std::vector<std::string> getAllFilesInDirectory(std::string_view directory, std::span<const std::string_view> filter);

		/**
		 * @brief Collects all files directly inside a directory (non-recursive, unfiltered).
		 * @param directory Directory to search.
		 * @return Paths of all files in the top level of @p directory.
		 */
		std::vector<std::string> getAllFilesInDirectory(const std::string& directory);

		/**
		 * @brief Extracts the file name without its extension from a full path.
		 * @details Accepts both '/' and '\' as separators. Only the last extension is removed, so
		 *          "Room 2.1.obj" gives "Room 2.1".
		 * @param path The full file path.
		 * @return The final path component without its extension.
		 */
		std::string getFileName(const std::string& path);

		/**
		 * @brief Extracts the extension from a path, including the leading dot.
		 * @details Follows the std::filesystem rule: a leading dot is part of the name, not a
		 *          separator, so a dotfile such as ".gitignore" has no extension. A path without
		 *          a dot, or one ending in a separator, yields an empty string.
		 * @param path The file path to inspect.
		 * @return The extension as it is written on disk, e.g. ".obj" - the case is not changed,
		 *         so comparisons against a fixed list should lower-case it first.
		 */
		std::string getFileExtension(std::string_view path);

		std::string readFile(const std::string& path);

		/**
		 * @brief Replaces @p file with @p text so that it is either fully written or left as it was.
		 * @details The text goes into "<file>.tmp" first, which is then renamed over the target: a
		 *          crash or a full disk mid-write leaves the old file intact instead of half a file.
		 *          Missing parent folders are created. Written in binary mode, so the bytes on disk are
		 *          exactly @p text on every platform.
		 * @return False if the text could not be written or not be moved into place; the original
		 *         file is untouched then and no temporary file is left behind.
		 */
		bool writeFileAtomic(const std::filesystem::path& file, std::string_view text);

		/**
		 * @brief Builds a path from UTF-8 text, such as a string from a JSON file or an ImGui field.
		 * @details On Windows a path constructed from a plain std::string reads its bytes in the
		 *          system code page, which turns "ü" from a UTF-8 file into two wrong characters.
		 */
		std::filesystem::path fromUtf8(std::string_view utf8);

		/**
		 * @brief The path as UTF-8 text with '/' separators - the form stored in files and shown in the UI.
		 * @details Never throws. path::string() and generic_string() convert to the system code page
		 *          on Windows and throw for characters it cannot represent, e.g. a Japanese file name.
		 */
		std::string toUtf8(const std::filesystem::path& path);

		/**
		 * @brief Enumerates all immediate subdirectories of a path, each as a @c Directory record.
		 * @param path The parent directory to scan.
		 * @return One @c Directory entry per subfolder.
		 */
		std::vector<Directory> getAllDirectories(const std::string& path);
	};
}