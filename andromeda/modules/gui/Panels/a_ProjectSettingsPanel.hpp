#pragma once
#include "a_EditorPanel.hpp"
#include "a_primitives.hpp"
#include <string>
#include <string_view>
#include <vector>
namespace Andromeda::Gui {
    struct EditorContext;
    /**
     * @brief The categories listed on the left side of the Project Settings panel.
     * @note When adding a value, also add its case to tabName(), tabIcon() and the switch in drawCall(),
     *       otherwise it shows up as "Unknown". Keep Count last - it is the number of categories, not a category.
     */
    enum class ProjectSettingsTab { General, Rendering, Assets, Plugins, Input, Scripting, Count };

    class ProjectSettingsPanel : public EditorPanel {
    public:
        explicit ProjectSettingsPanel(const char* name) : EditorPanel(name) { m_IsOpen = true; }

        void addDirectory(std::string_view directory);
        void drawGeneral(EditorContext& ctx);
        void drawRendering(EditorContext& ctx);
        void drawAssets(EditorContext& ctx);
        void drawPlugins(EditorContext& ctx);
        void drawInput(EditorContext& ctx);
        void drawScripting(EditorContext& ctx);

        void drawCall(EditorContext& ctx);

        void initPanel(EditorContext& ctx) override;
        void onGuiRender(EditorContext& ctx) override;

        void readSettings(const char* line) override;
        void writeSettings(ImGuiTextBuffer& buf) const override;
    private:
        // The text fields are plain char arrays because ImGui::InputText edits a buffer in place.
        // 260 is MAX_PATH on Windows. None of these values is saved yet - that needs a project file.

        /** @brief Values of the General category. */
        struct GeneralSettings {
            char projectName[128] = "Andromeda";
            char version[32] = "0.1.0";
            char startupScene[260] = "";
            bool reopenLastScene = true;
            bool autosave = true;
            i32 autosaveMinutes = 5;
        };

        /** @brief Values of the Rendering category (backend only). */
        struct RenderingSettings {
            i32 graphicsApi = 0;  ///< Index into the API list in the .cpp, same order as AndromedaGXAPI (renderer.h).
            bool vsync = true;
            i32 msaaSamples = 4;  ///< 1 means MSAA off.
        };

        /** @brief Values of the Assets category. */
        struct AssetSettings {
            std::vector<std::string> directories{ "assets" };  ///< Without trailing slash, see addDirectory().
            char databaseDirectory[260] = ".andromeda/";  ///< Folder that holds asset_db.json.
        };

        GeneralSettings m_General;
        RenderingSettings m_Rendering;
        AssetSettings m_Assets;
        i32 m_SelectedDirectory = -1;   ///< Row selected in the asset directory list, -1 for none.
        char m_NewDirectory[260] = "";  ///< Path typed next to the + button.

        ProjectSettingsTab m_SelectedTab = ProjectSettingsTab::General;
    };
}