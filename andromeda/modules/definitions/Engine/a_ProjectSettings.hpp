#pragma once
#include <string>
#include <vector>
#include "a_primitives.hpp"
#include <nlohmann/json.hpp>
namespace Andromeda
{
    struct ProjectSettings
    {
        struct General {
            std::string projectName = "Andromeda";
            std::string version = "0.1.0";
            std::string startupScene;
            bool reopenLastScene = true;
            bool autosave = true;
            i32 autosaveMinutes = 5;
        } general;

        struct Rendering
        {
            i32 graphicsApi = 0;
            bool vsync = true;
            i32 msaaSamples = 4;
        } rendering;

        struct Assets {
            std::vector<std::string> directories{ "assets" };
            std::string databaseDirectory = "ProjectSettings";
        } assets;
    };

    // JSON for ProjectSettings/project.json. WITH_DEFAULT: a key missing from the file keeps the value
    // from the struct, so a file written before a field existed still loads.
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ProjectSettings::General, projectName, version, startupScene,
                                                    reopenLastScene, autosave, autosaveMinutes)
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ProjectSettings::Rendering, graphicsApi, vsync, msaaSamples)
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ProjectSettings::Assets, directories, databaseDirectory)
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ProjectSettings, general, rendering, assets)
}