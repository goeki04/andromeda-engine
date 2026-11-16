#pragma once
#include <string>
#include <nlohmann/json.hpp>
#include <filesystem>
#include "a_primitives.hpp"
namespace Andromeda {
    /*
    struct PluginManifest {
        std::string id;
        std::string name;
        std::string version;
        std::string description;
        std::string author;
        std::string library;
        std::string icon;
        i32 apiVersion = 0;
    };

    struct PluginEntry {
        PluginManifest manifest;
        std::string error;
        std::filesystem::path folder;
    };

    inline void from_json(const nlohmann::json& j, PluginManifest& manifest) {
        // Required: without them the plugin can be neither identified nor loaded.
        j.at("id").get_to(manifest.id);
        j.at("name").get_to(manifest.name);
        j.at("library").get_to(manifest.library);
        j.at("apiVersion").get_to(manifest.apiVersion);

        // Optional: a plugin without an icon or description is still a valid plugin.
        manifest.version = j.value("version", std::string{});
        manifest.description = j.value("description", std::string{});
        manifest.author = j.value("author", std::string{});
        manifest.icon = j.value("icon", std::string{});
    }

    inline constexpr i32 PLUGIN_API_VERSION = 1;

    using GetApiVersionFn = i32 (*)();
    using CreatePluginFn  = IPlugin* (*)();
    using DestroyPluginFn = void (*)(IPlugin*);

    class IPlugin {
    public:

        virtual ~IPlugin() = default;
        virtual void onLoad() = 0;
        virtual void onUnload() = 0;
    };*/
}