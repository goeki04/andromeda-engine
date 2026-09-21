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
         * @brief Floating overlay in the top-left corner of the canvas listing each group's graph variables.
         * @param canvasPos Top-left of the canvas in screen space, captured before ed::Begin().
         * @param canvasSize Default overlay size; after the first frame the user-resized size from imgui.ini wins.
         * @param rounding Corner rounding of the overlay, matched to the nodes.
         * @param groups The particle groups of the selected ParticleSystem.
         * @note Call after ed::End(), still inside the panel's ImGui::Begin()/End().
         */
        void drawVariablesWindow(const ImVec2& canvasPos, const ImVec2& canvasSize, float rounding,
                                 std::span<ParticleGroup> groups);
        /** @copydoc EditorPanel::onGuiRender */
        void onGuiRender(EditorContext& ctx) override;

        ~ParticleEditor() {
            if (m_NodeEditorContext) {
                ed::DestroyEditor(m_NodeEditorContext);
                m_NodeEditorContext = nullptr;
            }
        }

        private:
        std::vector<ImGuiTextFilter> m_VariableFilters;
        ECS::Entity m_ShownEntity = ECS::INVALID_ENTITY_ID; ///< Entity whose graph was drawn last frame.
        ImGuiTextFilter m_AddNodeFilter;
    };
} 