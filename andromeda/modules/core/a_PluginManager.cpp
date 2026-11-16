#include "a_PluginManager.hpp"
#include "a_logger.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
namespace Andromeda {
    namespace {
        // The manifest is found by its extension, not by a fixed name: "<anyName>.aplugin" holds the
        // JSON. A plugin can keep its own .json files (config, data) next to it without them being
        // mistaken for the manifest.
        constexpr const char* manifestExtension = ".aplugin";
    }

/* std::vector<PluginEntry> PluginManager::getPluginList(const std::filesystem::path& pluginDirectory) {
        namespace fs = std::filesystem;
        // Plugins are per project, and a project without a Plugins folder simply uses none - no warning.
        if (!fs::exists(pluginDirectory)) {
            return {};
        }
        if (!fs::is_directory(pluginDirectory)) {
            A_WARN("Plugin path exists but is not a directory: {}", pluginDirectory.string());
            return {};
        }

        std::vector<PluginEntry> pluginList;

        // One subfolder per plugin: <pluginDirectory>/<PluginName>/<anyName>.aplugin, with the plugin's
        // library next to it. The folder is stored so the library can be loaded relative to it.
        for (const auto& entry : fs::directory_iterator(pluginDirectory)) {
            if (!entry.is_directory()) {
                continue;
            }

            const fs::path pluginFolder = entry.path();

            std::vector<fs::path> manifests;
            for (const auto& item : fs::directory_iterator(pluginFolder)) {
                if (item.is_regular_file() && item.path().extension() == manifestExtension) {
                    manifests.push_back(item.path());
                }
            }

            if (manifests.empty()) {
                A_WARN("Plugin folder has no {} manifest, skipping: {}", manifestExtension, pluginFolder.string());
                continue;
            }

            // Exactly one manifest per folder - with two it is unclear which one describes the plugin,
            // so the folder is listed with an error instead of guessing.
            if (manifests.size() > 1) {
                A_WARN("Plugin folder has more than one {} manifest: {}", manifestExtension, pluginFolder.string());
                pluginList.push_back(PluginEntry{{}, std::string("More than one ") + manifestExtension + " manifest in plugin folder", pluginFolder});
                continue;
            }

            const fs::path& manifestPath = manifests.front();

            std::ifstream file(manifestPath);
            if (!file.is_open()) {
                A_WARN("Failed to open plugin manifest file: {}", manifestPath.string());
                continue;
            }

            try {
                nlohmann::json jsonManifest;
                file >> jsonManifest;

                PluginManifest manifest = jsonManifest.get<PluginManifest>();
                pluginList.push_back(PluginEntry{std::move(manifest), {}, pluginFolder});
            } catch (const nlohmann::json::exception& e) {
                A_WARN("Failed to read plugin manifest file: {}. Error: {}", manifestPath.string(), e.what());
                pluginList.push_back(PluginEntry{{}, e.what(), pluginFolder});
            }
        }

        return pluginList;
    }
    */
}