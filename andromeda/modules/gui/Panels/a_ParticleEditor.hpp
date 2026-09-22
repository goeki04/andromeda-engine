#pragma once
#include "imgui_node_editor.h"
#include "a_EditorPanel.hpp"
#include "a_EditorContext.hpp"
#include "a_particle_group.hpp"
#include <span>
#include <string>
#include <vector>
#include "a_Primitives.hpp"
#include "a_Nodes.hpp"
namespace Andromeda::ECS::Component {
    struct ParticleSystem;
}

namespace Andromeda::Gui {
    namespace ed = ax::NodeEditor;

    class ParticleEditor : public EditorPanel {
    
        public:
        ed::EditorContext* m_NodeEditorContext = nullptr;
        explicit ParticleEditor(const char* name) : EditorPanel(name) { 
            m_IsOpen = false; 

            ed::Config config;
            config.CustomZoomLevels.push_back(0.25f);
            config.CustomZoomLevels.push_back(0.5f);
            config.CustomZoomLevels.push_back(0.75f);
            config.CustomZoomLevels.push_back(1.0f);
            config.SettingsFile = "ParticleEditor.json";
            config.NavigateButtonIndex = 2; // mouse button index for navigation (0 = left, 1 = right, 2 = middle)
            m_NodeEditorContext = ed::CreateEditor(&config);
        }
        /**
         * @brief Floating overlay in the top-left corner of the canvas listing the system's graph variables.
         * @details The variables belong to the ParticleSystem and are shared by the graphs of all its groups.
         *          Each row has a drag handle; dropping it on the canvas creates a matching node.
         * @param canvasPos Top-left of the canvas in screen space, captured before ed::Begin().
         * @param canvasSize Default overlay size; after the first frame the user-resized size from imgui.ini wins.
         * @param rounding Corner rounding of the overlay, matched to the nodes.
         * @param system The selected ParticleSystem.
         * @note Call after ed::End(), still inside the panel's ImGui::Begin()/End().
         */
        void drawVariablesWindow(const ImVec2& canvasPos, const ImVec2& canvasSize, float rounding,
                                 ECS::Component::ParticleSystem& system);
        /** @copydoc EditorPanel::onGuiRender */
        void onGuiRender(EditorContext& ctx) override;

        ~ParticleEditor() {
            if (m_NodeEditorContext) {
                ed::DestroyEditor(m_NodeEditorContext);
                m_NodeEditorContext = nullptr;
            }
        }

        private:
        ImGuiTextFilter m_VariableFilter; ///< Search text of the variables overlay.
        ECS::Entity m_ShownEntity = ECS::INVALID_ENTITY_ID; ///< Entity whose graph was drawn last frame.
        ImGuiTextFilter m_AddNodeFilter;  ///< Search text of the "Add Node" popup.
        ed::PinId m_PendingLinkPin;       ///< Pin a link was dragged from into empty space; the next node from the popup connects to it.
    };
} 