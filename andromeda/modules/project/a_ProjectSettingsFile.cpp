#include "a_ProjectSettingsFile.hpp"
#include "a_filesystem.hpp"
#include <fstream>
#include <nlohmann/json.hpp>

namespace Andromeda::ProjectSettingsFile {

    LoadResult load(const std::filesystem::path& file) {
        LoadResult result;

        std::error_code ec;
        const bool exists = std::filesystem::exists(file, ec);
        if (ec) {
            // Cannot even tell whether it is there - treat it like a broken file, so nobody overwrites it.
            result.status = Status::Invalid;
            result.error = ec.message();
            return result;
        }
        if (!exists) {
            result.status = Status::Missing;
            return result;
        }

        std::ifstream in(file, std::ios::binary);
        if (!in) {
            // Present but unreadable, e.g. locked by another program. Reporting it as Missing would
            // let the caller write the defaults over the user's settings.
            result.status = Status::Invalid;
            result.error = "the file could not be opened";
            return result;
        }

        // Third argument false: broken JSON is reported by is_discarded() instead of an exception.
        const nlohmann::json root = nlohmann::json::parse(in, nullptr, false);
        if (root.is_discarded()) {
            result.status = Status::Invalid;
            result.error = "not valid JSON";
            return result;
        }
        try {
            // get() builds a new object first, so a wrong value leaves result.settings on the defaults.
            result.settings = root.get<ProjectSettings>();
            result.status = Status::Loaded;
        } catch (const nlohmann::json::exception& e) {
            result.status = Status::Invalid;
            result.error = e.what();
        }
        return result;
    }

    bool save(const std::filesystem::path& file, const ProjectSettings& settings, std::string* error) {
        std::string text;
        try {
            text = nlohmann::json(settings).dump(4);
        } catch (const nlohmann::json::exception& e) {
            // dump() throws on strings that are not valid UTF-8.
            if (error) {
                *error = e.what();
            }
            return false;
        }
        if (!Filesystem::writeFileAtomic(file, text)) {
            if (error) {
                *error = "the file could not be written";
            }
            return false;
        }
        return true;
    }
}
