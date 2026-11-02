#pragma once
#include <string>
#include <unordered_map>
#include <array>
#include <vector>
#include <filesystem>
#include "a_primitives.hpp"
#include <string_view>
#include <span>
#include "a_filesystem.hpp"
#include "a_Guid.hpp"
#include "a_texture.hpp"

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

    struct AssetRecord {
        Guid guid;
        std::string name;
        std::string path;
        AssetType type = AssetType::Unknown;
        u64 fileTimestamp = 0;
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
        AssetRegistry();
        /**
         * @brief Imports an asset from the given file path.
         * @param rawPath The file path of the asset to import.
         */
        void importAsset(const std::string& rawPath);
        /**
         * @brief Adds a directory to the list of directories to import assets from.
         * @param directory The directory path to add.
         */
        void addImportDirectory(const std::string& directory);
        /**
         * @brief Removes a directory from the list of directories to import assets from.
         * @param directory The directory path to remove.
         */
        void removeImportDirectory(const std::string& directory);

        [[nodiscard]] const AssetRecord* findByGuid(const Guid& guid) const;

        [[nodiscard]] const AssetRecord* findByPath(const std::string& rawPath) const;

        void scanDirectory(const std::string& directory);

        bool loadDatabase(const std::filesystem::path& file);

        bool saveDatabase(const std::filesystem::path& file) const;

        [[nodiscard]] const std::vector<std::string>& getImportDirectories() const;
        [[nodiscard]] const std::vector<AssetRecord>& getRecords() const;
    private:
        std::vector<std::string> m_ImportDirectories;
        std::unordered_map<Guid, size_t> m_AssetRecordIndex;
        std::unordered_map<std::string, Guid> m_PathToGuid;
        std::vector<AssetRecord> m_AssetRecordsList;
        std::filesystem::path m_ProjectRoot;
        std::unordered_map<std::string, Guid> m_KnownGuids;
        /**
         * @brief True if @p path is new to the registry and of a known asset type.
         * @details Checked before anything touches the file, so unknown and already registered
         *          files cost no file system call. @p path must already be normalized.
         */
        [[nodiscard]] bool isImportable(const std::string& path) const;

        /**
         * @brief Creates the record for a file that passed isImportable().
         * @param path          Normalized path of the file.
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