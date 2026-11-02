#include "a_AssetRegistry.hpp"
#include "a_filesystem.hpp"
#include <random>
#include <filesystem>
#include <fstream>
#include <nlohmann/adl_serializer.hpp>
#include <nlohmann/json.hpp>

namespace {
    std::string normalizePath(std::string_view path) {
        return std::filesystem::path(path).lexically_normal().generic_string();
    }
}

namespace Andromeda::AssetManagement {
    void AssetRegistry::addImportDirectory(const std::string& directory) {
        m_ImportDirectories.push_back(directory);
    }

    void AssetRegistry::removeImportDirectory(const std::string& directory) {
        m_ImportDirectories.erase(std::remove(m_ImportDirectories.begin(), m_ImportDirectories.end(), directory), m_ImportDirectories.end());
    }

    const AssetRecord* AssetRegistry::findByGuid(const Guid& guid) const {
        const auto it = m_AssetRecordIndex.find(guid);
        if (it != m_AssetRecordIndex.end()) {
            return &m_AssetRecordsList[it->second];
        }
        return nullptr;
    }

    const AssetRecord* AssetRegistry::findByPath(const std::string& rawPath) const {
        const std::string path = normalizePath(rawPath);
        const auto it = m_PathToGuid.find(path);
        if (it != m_PathToGuid.end()) {
            return findByGuid(it->second);
        }
        return nullptr;
    }

    bool AssetRegistry::moveAsset(const Guid& guid, const std::string& rawNewPath) {
        const auto it = m_AssetRecordIndex.find(guid);
        if (it == m_AssetRecordIndex.end()) {
            return false;
        }
        const std::string newPath = normalizePath(rawNewPath);
        if (m_PathToGuid.contains(newPath)) {
            return false;
        }
        AssetRecord& record = m_AssetRecordsList[it->second];
        m_PathToGuid.erase(record.path);
        record.path = newPath;
        record.name = Filesystem::getFileName(newPath);
        m_PathToGuid[newPath] = guid;
        return true;
    }

    void AssetRegistry::scanDirectory(const std::string& directory) {
        namespace fs = std::filesystem;
        std::error_code ec;

        for (fs::recursive_directory_iterator it(directory, fs::directory_options::skip_permission_denied, ec), end;
             !ec && it != end; it.increment(ec)) {
            std::error_code fileEc;
            if (!it->is_regular_file(fileEc)) {
                continue;
            }
            const std::string path = normalizePath(it->path().generic_string());
            if (!isImportable(path)) {
                continue;
            }
            // On Windows the directory listing already carries the modification time, so the entry
            // answers without another call to the OS. Asking std::filesystem::last_write_time(path)
            // instead opens each file again and made the scan about six times slower.
            const auto lastWriteTime = it->last_write_time(fileEc);
            if (fileEc) {
                continue;
            }
            addRecord(path, lastWriteTime);
        }
    }

    bool AssetRegistry::saveDatabase(const std::filesystem::path& file) const
    {
        namespace fs = std::filesystem;
        nlohmann::json root;
        root["assets"] = nlohmann::json::array();
        for (const AssetRecord& record : m_AssetRecordsList) {
            const fs::path relative = fs::path(record.path).lexically_relative(m_ProjectRoot);
            const bool insideProject = !relative.empty() && *relative.begin() != "..";
            root["assets"].push_back({
                {"guid", record.guid.toString()},
                {"path", insideProject ? relative.generic_string() : record.path},
                {"timestamp", record.fileTimestamp},
            });
        }

        std::error_code ec;
        fs::create_directories(file.parent_path(), ec);

        const fs::path temp = file.string() + ".tmp";
        {
            std::ofstream out(temp);
            if (!out) {
                return false;
            }
            out << root.dump(4);
            if (!out) {
                return false;
            }
        }
        fs::rename(temp, file, ec);
        return !ec;
    }

    bool AssetRegistry::loadDatabase(const std::filesystem::path& file)
    {
        std::ifstream in(file);
        if (!in) {
            return true;
        }
        const nlohmann::json root = nlohmann::json::parse(in, nullptr, false);
        if (root.is_discarded() || !root.contains("assets") || !root["assets"].is_array()) {
            return false;
        }

        for (const nlohmann::json& entry : root["assets"])
        {
            if (!entry.contains("guid") || !entry.contains("path"))
            {
                continue;
            }
            const std::string stored = entry["path"].get<std::string>();
            if (stored.empty()) {
                continue;
            }
            const std::optional<Guid> guid = Guid::fromString(entry["guid"].get<std::string>());
            if (!guid)
            {
                continue;
            }
            const std::string path = normalizePath((m_ProjectRoot / entry["path"].get<std::string>()).generic_string());
            m_KnownGuids.emplace(path, *guid);
        }
        return true;
    }

    // ASSET_PATH is "<project>/assets/" - its parent is the project root.
    AssetRegistry::AssetRegistry()
        : m_ProjectRoot(std::filesystem::path(ASSET_PATH).parent_path().parent_path()) {}

    void AssetRegistry::importAsset(const std::string& rawPath) {
        const std::string path = normalizePath(rawPath);
        if (!isImportable(path)) {
            return;
        }
        std::error_code ec;
        const auto lastWriteTime = std::filesystem::last_write_time(path, ec);
        if (ec) {
            return;
        }
        addRecord(path, lastWriteTime);
    }

    bool AssetRegistry::isImportable(const std::string& path) const {
        // Double import prevention
        if (m_PathToGuid.contains(path)) {
            return false;
        }
        return getAssetTypeFromPath(path) != AssetType::Unknown;
    }

    void AssetRegistry::addRecord(const std::string& path, const std::filesystem::file_time_type lastWriteTime) {
        AssetRecord record;
        const auto known = m_KnownGuids.find(path);
        const bool reuse = known != m_KnownGuids.end() && !m_AssetRecordIndex.contains(known->second);
        const Guid guid = reuse ? known->second : generate_uuid_v4_binary();
        record.guid = guid;
        record.name = Filesystem::getFileName(path);
        record.path = path;
        record.type = getAssetTypeFromPath(path);
        record.fileTimestamp = static_cast<u64>(lastWriteTime.time_since_epoch().count());
        m_AssetRecordsList.push_back(std::move(record));
        m_PathToGuid[m_AssetRecordsList.back().path] = guid;
        m_AssetRecordIndex[guid] = m_AssetRecordsList.size() - 1;
    }

    Guid AssetRegistry::generate_uuid_v4_binary() {
            // Seeded once per thread with 256 bits. A single rd() call would only
            // give 32 bits and allow just 2^32 distinct GUIDs.
            thread_local std::mt19937_64 gen = [] {
                std::random_device rd;
                std::seed_seq seq{rd(), rd(), rd(), rd(), rd(), rd(), rd(), rd()};
                return std::mt19937_64{seq};
            }();

            std::array<u8, 16> bytes;

            const u64 part1 = gen();
            const u64 part2 = gen();

            for (int i = 0; i < 8; ++i) {
                bytes[i] = static_cast<u8>((part1 >> (i * 8)) & 0xFF);
                bytes[8 + i] = static_cast<u8>((part2 >> (i * 8)) & 0xFF);
            }

            bytes[6] = static_cast<u8>((bytes[6] & 0x0F) | 0x40); // version 4 (random)
            bytes[8] = static_cast<u8>((bytes[8] & 0x3F) | 0x80); // variant RFC 9562
                        
            return Guid{bytes};
        }


    const std::vector<std::string>& AssetRegistry::getImportDirectories() const {
            return m_ImportDirectories;
        }

    const std::vector<AssetRecord>& AssetRegistry::getRecords() const {
            return m_AssetRecordsList;
        }
}