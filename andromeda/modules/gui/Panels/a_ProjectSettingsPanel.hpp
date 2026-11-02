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

        void addDirectory(EditorContext& ctx, std::string_view directory);
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
        // The values themselves live in ProjectSettings (definitions/Engine/a_ProjectSettings.hpp), owned by
        // the ResourceManager: it needs the asset directories at startup, before any panel exists. This
        // panel only edits them through ctx.projectSettings and has them saved after each finished edit.

        bool m_Dirty = false;           ///< Set by a finished edit; onGuiRender saves and clears it.
        i32 m_SelectedDirectory = -1;   ///< Row selected in the asset directory list, -1 for none.
        std::string m_NewDirectory;     ///< Path typed next to the + button.
        std::string m_DatabaseInput;    ///< Full path shown in the database folder field, refreshed while not edited.

        ProjectSettingsTab m_SelectedTab = ProjectSettingsTab::General;
    };
}