#pragma once
#include <string>
#include <unordered_map>
#include <array>
#include <vector>
#include <filesystem>
#include <system_error>
#include "a_primitives.hpp"
#include <string_view>
#include <span>
#include "a_filesystem.hpp"
#include "a_Guid.hpp"
#include "a_ProjectPaths.hpp"

namespace Andromeda::AssetManagement {

    enum class AssetType {
            Unknown,
            Mesh,
            Texture,
            Material,
            GraphicsShader,
            ComputeShader,
            Scene,
            PythonScript,
            GLSL,
            HLSL,
            TextFile,
            Markdown,
            Json,
            Xml,
            Count
    };

    constexpr auto AssetExtensions = std::to_array<std::pair<std::string_view, AssetType>>({
        {".obj", AssetType::Mesh},
        {".fbx", AssetType::Mesh},
        {".gltf", AssetType::Mesh},
        {".png", AssetType::Texture},
        {".jpg", AssetType::Texture},
        {".jpeg", AssetType::Texture},
        {".comp", AssetType::ComputeShader},
        {".glsl", AssetType::GLSL},
        {".hlsl", AssetType::HLSL},
        {".frag", AssetType::GraphicsShader},
        {".vert", AssetType::GraphicsShader},
        {".mat", AssetType::Material},
        {".scene", AssetType::Scene},
        {".py", AssetType::PythonScript},
        {".txt", AssetType::TextFile},
        {".md", AssetType::Markdown},
        {".json", AssetType::Json},
        {".xml", AssetType::Xml}
    });

    constexpr AssetType getAssetTypeFromExtension(std::string extension) {
        Filesystem::stringToLower(extension);
        const auto it = std::ranges::find(AssetExtensions, extension,
            &std::pair<std::string_view, AssetType>::first);
        return it != AssetExtensions.end() ? it->second : AssetType::Unknown;
    }

    inline AssetType getAssetTypeFromPath(std::string_view path) {
        return getAssetTypeFromExtension(Filesystem::getFileExtension(path));
    }

    /**
     * @brief True if @p name can become the name of an asset file on every platform we target.
     * @details @p name is the file name without its extension. Rejected are an empty name, the
     *          characters Windows refuses (\ / : * ? " < > | and control characters), a trailing dot
     *          or space - Windows silently drops those, so the file would end up under a different
     *          name than the registry records - and the reserved device names (CON, NUL, COM1, ...).
     */
    bool isValidAssetFileName(std::string_view name);

    /**
     * @brief One file the registry knows.
     * @details Text fields are UTF-8 with '/' separators, so they can go straight into a JSON file or an
     *          ImGui label. Turn @c path back into a filesystem path with Filesystem::fromUtf8().
     */
    struct AssetRecord {
        Guid guid;
        std::string name;  ///< File name without extension, e.g. "Room 2.1".
        std::string path;  ///< Absolute, normalized path of the file.
        AssetType type = AssetType::Unknown;
        u64 fileTimestamp = 0;
    };

    /** @brief Outcome of AssetRegistry::renameAsset(). */
    enum class RenameStatus {
        Renamed,      ///< File and record carry the new name.
        Unchanged,    ///< The new name is the current one; nothing was touched.
        UnknownAsset, ///< No record with that Guid.
        InvalidName,  ///< Rejected by isValidAssetFileName().
        NameTaken,    ///< Another file (or another record) already uses the new name.
        FileError,    ///< The operating system refused the rename, see RenameResult::error.
    };

    struct RenameResult {
        RenameStatus status = RenameStatus::Unchanged;
        std::error_code error; ///< The operating system's reason for a FileError.
    };

    /**
     * @class AssetRegistry
     * @brief Loads assets from disk and keeps one AssetRecord per asset, identified by a Guid.
     * @details The registry is the single source of truth for what exists on disk: it discovers
     *          files, classifies them via getAssetTypeFromPath(), assigns each a Guid and loads
     *          their data. Everything else in the engine refers to an asset by that Guid rather
     *          than by its path, so a file can be renamed or moved without breaking references.
     *
     *          The registry does not know which scene uses which asset - that relationship is
     *          owned by AssetManager. A scene asks the registry for an asset's data; it never
     *          stores the registry's records itself.
     * @see AssetManager
     * @see AssetRecord
     */
    class AssetRegistry {
    public:
        /**
         * @param paths The project the assets belong to. asset_db.json stores paths relative to its
         *              root, so the same database works wherever the project folder is.
         */
        explicit AssetRegistry(ProjectPaths paths);
        /**
         * @brief Imports an asset from the given file path.
         * @param path The file path of the asset to import.
         */
        void importAsset(const std::filesystem::path& path);
        /**
         * @brief Adds a directory to the list of directories to import assets from.
         * @param directory The directory path to add.
         */
        void addImportDirectory(const std::filesystem::path& directory);
        /**
         * @brief Removes a directory from the list of directories to import assets from.
         * @param directory The directory path to remove.
         */
        void removeImportDirectory(const std::filesystem::path& directory);

        [[nodiscard]] const AssetRecord* findByGuid(const Guid& guid) const;

        /** @brief The record of the file at @p path, whatever its spelling ("a/./b.obj", "a\\b.obj"). */
        [[nodiscard]] const AssetRecord* findByPath(const std::filesystem::path& path) const;

        /**
         * @brief Points an existing record at a new path after its file was renamed or moved.
         * @details The Guid stays the same - that is what lets everything referring to the asset
         *          survive the rename. Only the registry is updated, the file itself has to be
         *          moved by the caller beforehand.
         * @param guid    Guid of the record to update.
         * @param newPath The file's new path; the extension, and with it the type, is kept.
         * @return False if the Guid is unknown or another record already uses the new path.
         */
        bool moveAsset(const Guid& guid, const std::filesystem::path& newPath);

        /**
         * @brief Forgets the record of a file that was deleted.
         * @details Only the registry is updated, the file itself has to be removed by the caller.
         *          The Guid is remembered for the file's path: if the file comes back (Recycle Bin,
         *          git checkout), the next scan hands it its old Guid, so scenes still find it.
         * @return False if the Guid is unknown.
         */
        bool removeAsset(const Guid& guid);

        /**
         * @brief Renames an asset's file on disk and updates its record; the Guid stays the same.
         * @param guid    Guid of the asset to rename.
         * @param newName New file name without extension; the extension is kept.
         * @details Changing only the case ("apo" -> "Apo") is allowed: on Windows the old file is
         *          found under the new name, which must not count as the name being taken.
         */
        RenameResult renameAsset(const Guid& guid, std::string_view newName);

        void scanDirectory(const std::filesystem::path& directory);

        /**
         * @brief Reads the Guids of a previous session from asset_db.json.
         * @details Nothing becomes a record here - a Guid is only handed out again when scanning or
         *          importing finds its file. Entries that are incomplete or malformed are skipped.
         * @return True if the Guids were read or there is no file yet (first start). False if the file
         *         exists but could not be read: it must then not be overwritten, so it can be repaired.
         */
        bool loadDatabase(const std::filesystem::path& file);

        /** @brief Writes every record to asset_db.json, replacing the file atomically. */
        bool saveDatabase(const std::filesystem::path& file) const;

        [[nodiscard]] const std::vector<std::string>& getImportDirectories() const;
        [[nodiscard]] const std::vector<AssetRecord>& getRecords() const;
    private:
        std::vector<std::string> m_ImportDirectories;
        std::unordered_map<Guid, size_t> m_AssetRecordIndex;
        std::unordered_map<std::string, Guid> m_PathToGuid;
        std::vector<AssetRecord> m_AssetRecordsList;
        ProjectPaths m_Paths;
        std::unordered_map<std::string, Guid> m_KnownGuids;
        /**
         * @brief True if @p path is new to the registry and of a known asset type.
         * @details Checked before anything touches the file, so unknown and already registered
         *          files cost no file system call. @p path must already be a normalized key.
         */
        [[nodiscard]] bool isImportable(const std::string& path) const;

        /**
         * @brief Creates the record for a file that passed isImportable().
         * @param path          Normalized key of the file.
         * @param lastWriteTime Modification time of the file. scanDirectory() passes the one the
         *                      directory listing already carries, importAsset() asks the OS for it.
         */
        void addRecord(const std::string& path, std::filesystem::file_time_type lastWriteTime);
        /**
         * @brief Generates a random UUID v4 (RFC 9562) for a new asset.
         * @details 122 of the 128 bits are random, the rest encode version and variant.
         * @return A new, unique asset GUID in canonical big-endian byte order.
         */
        static Guid generate_uuid_v4_binary();
    };
}
