#include "a_AssetRegistry.hpp"
#include "a_filesystem.hpp"
#include <random>
#include <filesystem>
#include <fstream>
#include <utility>
#include <nlohmann/adl_serializer.hpp>
#include <nlohmann/json.hpp>

namespace {
    // The registry's spelling of a path: normalized, '/' separators, UTF-8. One spelling per file,
    // so "a/./b.obj" and "a\\b.obj" find the same record. UTF-8 rather than path::generic_string():
    // that converts to the Windows code page and throws for characters it cannot represent, which
    // would let a single oddly named file stop the whole scan.
    std::string toKey(const std::filesystem::path& path) {
        return Andromeda::Filesystem::toUtf8(path.lexically_normal());
    }

    // By the extension of the path rather than getAssetTypeFromPath(std::string_view): that one would
    // read the UTF-8 key in the system code page.
    Andromeda::AssetManagement::AssetType typeOf(const std::string& key) {
        return Andromeda::AssetManagement::getAssetTypeFromExtension(
            Andromeda::Filesystem::toUtf8(Andromeda::Filesystem::fromUtf8(key).extension()));
    }
}

namespace Andromeda::AssetManagement {
    bool isValidAssetFileName(const std::string_view name) {
        if (name.empty() || name.back() == '.' || name.back() == ' ') {
            return false;
        }
        for (const char c : name) {
            if (static_cast<unsigned char>(c) < 0x20 || std::string_view("\\/:*?\"<>|").find(c) != std::string_view::npos) {
                return false;
            }
        }
        // Windows reserves these device names, with or without an extension.
        std::string upper(name);
        std::ranges::transform(upper, upper.begin(), [](const char c) {
            return (c >= 'a' && c <= 'z') ? static_cast<char>(c - ('a' - 'A')) : c;
        });
        constexpr std::array<std::string_view, 4> reserved{ "CON", "PRN", "AUX", "NUL" };
        if (std::ranges::find(reserved, upper) != reserved.end()) {
            return false;
        }
        const bool numberedDevice = upper.size() == 4 && (upper.starts_with("COM") || upper.starts_with("LPT")) &&
                                    upper[3] >= '1' && upper[3] <= '9';
        return !numberedDevice;
    }

    AssetRegistry::AssetRegistry(ProjectPaths paths)
        : m_Paths(std::move(paths)) {}

    void AssetRegistry::addImportDirectory(const std::filesystem::path& directory) {
        m_ImportDirectories.push_back(toKey(directory));
    }

    void AssetRegistry::removeImportDirectory(const std::filesystem::path& directory) {
        std::erase(m_ImportDirectories, toKey(directory));
    }

    const AssetRecord* AssetRegistry::findByGuid(const Guid& guid) const {
        const auto it = m_AssetRecordIndex.find(guid);
        if (it != m_AssetRecordIndex.end()) {
            return &m_AssetRecordsList[it->second];
        }
        return nullptr;
    }

    const AssetRecord* AssetRegistry::findByPath(const std::filesystem::path& path) const {
        const auto it = m_PathToGuid.find(toKey(path));
        if (it != m_PathToGuid.end()) {
            return findByGuid(it->second);
        }
        return nullptr;
    }

    bool AssetRegistry::moveAsset(const Guid& guid, const std::filesystem::path& newPath) {
        const auto it = m_AssetRecordIndex.find(guid);
        if (it == m_AssetRecordIndex.end()) {
            return false;
        }
        const std::string key = toKey(newPath);
        if (m_PathToGuid.contains(key)) {
            return false;
        }
        AssetRecord& record = m_AssetRecordsList[it->second];
        m_PathToGuid.erase(record.path);
        record.path = key;
        record.name = Filesystem::getFileName(key);
        m_PathToGuid[key] = guid;
        return true;
    }

    bool AssetRegistry::removeAsset(const Guid& guid) {
        const auto it = m_AssetRecordIndex.find(guid);
        if (it == m_AssetRecordIndex.end()) {
            return false;
        }
        const size_t index = it->second;
        m_KnownGuids[m_AssetRecordsList[index].path] = guid;
        m_PathToGuid.erase(m_AssetRecordsList[index].path);
        m_AssetRecordIndex.erase(it);
        m_AssetRecordsList.erase(m_AssetRecordsList.begin() + static_cast<std::ptrdiff_t>(index));
        // erase() moves every later record one slot down - their stored indices have to follow,
        // or findByGuid() would hand out the wrong record. Erase rather than swap-and-pop keeps the
        // order of the browser and of asset_db.json stable.
        for (size_t i = index; i < m_AssetRecordsList.size(); ++i) {
            m_AssetRecordIndex[m_AssetRecordsList[i].guid] = i;
        }
        return true;
    }

    RenameResult AssetRegistry::renameAsset(const Guid& guid, const std::string_view newName) {
        namespace fs = std::filesystem;
        const AssetRecord* record = findByGuid(guid);
        if (record == nullptr) {
            return { RenameStatus::UnknownAsset };
        }
        if (!isValidAssetFileName(newName)) {
            return { RenameStatus::InvalidName };
        }

        const fs::path oldPath = Filesystem::fromUtf8(record->path);
        fs::path fileName = Filesystem::fromUtf8(newName);
        fileName += oldPath.extension();
        const fs::path newPath = oldPath.parent_path() / fileName;
        if (newPath == oldPath) {
            return { RenameStatus::Unchanged };
        }

        // Checked before the file is touched: once it is renamed, a failing moveAsset() would leave
        // file and record disagreeing. equivalent() lets a case-only change through - on Windows the
        // old file answers to the new name, it is not a second file.
        std::error_code ec;
        const bool takenOnDisk = fs::exists(newPath, ec) && !fs::equivalent(oldPath, newPath, ec);
        if (takenOnDisk || m_PathToGuid.contains(toKey(newPath))) {
            return { RenameStatus::NameTaken };
        }

        fs::rename(oldPath, newPath, ec);
        if (ec) {
            return { RenameStatus::FileError, ec };
        }
        moveAsset(guid, newPath);
        return { RenameStatus::Renamed };
    }

    void AssetRegistry::scanDirectory(const std::filesystem::path& directory) {
        namespace fs = std::filesystem;
        std::error_code ec;

        for (fs::recursive_directory_iterator it(directory, fs::directory_options::skip_permission_denied, ec), end;
             !ec && it != end; it.increment(ec)) {
            std::error_code fileEc;
            if (!it->is_regular_file(fileEc)) {
                continue;
            }
            const std::string path = toKey(it->path());
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
        nlohmann::json root;
        root["assets"] = nlohmann::json::array();
        for (const AssetRecord& record : m_AssetRecordsList) {
            root["assets"].push_back({
                {"guid", record.guid.toString()},
                {"path", Filesystem::toUtf8(m_Paths.toStored(Filesystem::fromUtf8(record.path)))},
                {"timestamp", record.fileTimestamp},
            });
        }
        // Every string in here is UTF-8 from toUtf8() or toString(), so dump() cannot throw.
        return Filesystem::writeFileAtomic(file, root.dump(4));
    }

    bool AssetRegistry::loadDatabase(const std::filesystem::path& file)
    {
        std::error_code ec;
        const bool exists = std::filesystem::exists(file, ec);
        if (ec) {
            return false;
        }
        if (!exists) {
            return true;
        }
        // Present but unreadable (locked, no permission): reporting "no file" here would let the
        // caller save over it and hand every asset a new Guid.
        std::ifstream in(file, std::ios::binary);
        if (!in) {
            return false;
        }
        const nlohmann::json root = nlohmann::json::parse(in, nullptr, false);
        if (root.is_discarded() || !root.contains("assets") || !root["assets"].is_array()) {
            return false;
        }

        for (const nlohmann::json& entry : root["assets"])
        {
            // A hand-edited or damaged entry is skipped, never thrown on: get<std::string>() on a number
            // would end the editor during startup. The file asset just gets a new Guid.
            if (!entry.is_object()) {
                continue;
            }
            const auto guidField = entry.find("guid");
            const auto pathField = entry.find("path");
            if (guidField == entry.end() || pathField == entry.end() || !guidField->is_string() || !pathField->is_string()) {
                continue;
            }
            const std::string& stored = pathField->get_ref<const std::string&>();
            if (stored.empty()) {
                continue;
            }
            const std::optional<Guid> guid = Guid::fromString(guidField->get_ref<const std::string&>());
            if (!guid)
            {
                continue;
            }
            m_KnownGuids.emplace(toKey(m_Paths.resolve(Filesystem::fromUtf8(stored))), *guid);
        }
        return true;
    }

    void AssetRegistry::importAsset(const std::filesystem::path& path) {
        const std::string key = toKey(path);
        if (!isImportable(key)) {
            return;
        }
        std::error_code ec;
        const auto lastWriteTime = std::filesystem::last_write_time(path, ec);
        if (ec) {
            return;
        }
        addRecord(key, lastWriteTime);
    }

    bool AssetRegistry::isImportable(const std::string& path) const {
        // Double import prevention
        if (m_PathToGuid.contains(path)) {
            return false;
        }
        return typeOf(path) != AssetType::Unknown;
    }

    void AssetRegistry::addRecord(const std::string& path, const std::filesystem::file_time_type lastWriteTime) {
        AssetRecord record;
        const auto known = m_KnownGuids.find(path);
        const bool reuse = known != m_KnownGuids.end() && !m_AssetRecordIndex.contains(known->second);
        const Guid guid = reuse ? known->second : generate_uuid_v4_binary();
        record.guid = guid;
        record.name = Filesystem::getFileName(path);
        record.path = path;
        record.type = typeOf(path);
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
