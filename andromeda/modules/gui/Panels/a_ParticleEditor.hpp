#pragma once
#include "imgui_node_editor.h"
#include "imgui_internal.h" // ImRect
#include "a_EditorPanel.hpp"
#include "a_EditorContext.hpp"
#include "a_particle_group.hpp"
#include <span>
#include <string>
#include <vector>
#include "a_Primitives.hpp"
#include "a_Nodes.hpp"
#include "a_GroupsOverlay.hpp"
#include "a_node_graph.hpp"

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
            registerSettingsHandler();
        }
        /**
         * @brief Floating overlay on the canvas listing the system's graph variables; drag its title to move it.
         * @details The variables belong to the ParticleSystem and are shared by the graphs of all its groups.
         *          Each row has a drag handle; dropping it on the canvas creates a matching node.
         * @param canvas The whole canvas in screen space, captured before ed::Begin().
         * @param defaultSize Default overlay size; after the first frame the user-resized size from imgui.ini wins.
         * @param rounding Corner rounding of the overlay, matched to the nodes.
         * @param system The selected ParticleSystem.
         * @note Call after ed::End(), still inside the panel's ImGui::Begin()/End().
         */
        void drawVariablesWindow(const ImRect& canvas, const ImVec2& defaultSize, float rounding,
                                 ECS::Component::ParticleSystem& system);

        /**
         * @brief Overlay listing the system's particle groups; clicking one shows its graph. Movable like the
         *        variables overlay, and first placed below it.
         * @param system The selected ParticleSystem.
         * @param canvas The whole canvas in screen space, captured before ed::Begin().
         * @param defaultSize Default overlay size, like the variables overlay.
         * @param rounding Corner rounding of the overlay, matched to the nodes.
         * @note Call after ed::End(), still inside the panel's ImGui::Begin()/End().
         */
        void drawParticleGroupsWindow(ECS::Component::ParticleSystem& system, const ImRect& canvas,
                                      const ImVec2& defaultSize, float rounding);
        /** @copydoc EditorPanel::onGuiRender */
        void onGuiRender(EditorContext& ctx) override;

        ~ParticleEditor() {
            // The handler points at this panel; if the ImGui context outlives it, it must not call back.
            if (ImGui::GetCurrentContext())
                ImGui::RemoveSettingsHandler(kSettingsTypeName);
            if (m_NodeEditorContext) {
                ed::DestroyEditor(m_NodeEditorContext);
                m_NodeEditorContext = nullptr;
            }
        }

        private:
        static constexpr const char* kSettingsTypeName = "ParticleEditor"; ///< Section name in imgui.ini.

        /**
         * @brief Stores the overlay positions in imgui.ini, next to the window and overlay sizes ImGui keeps
         *        there, as a [ParticleEditor][Overlays] section.
         * @note Needs the ImGui context, which GuiRenderer::init() creates before the panels. ImGui reads
         *       imgui.ini lazily in the first NewFrame(), so registering here is early enough.
         */
        void registerSettingsHandler();

        /** @brief The group whose graph is edited: the selected one, or the first if that one is gone. */
        ParticleGroup& activeGroup(ECS::Component::ParticleSystem& system, ECS::Entity entity);

        /** @brief Pushes the stored node positions into the shared editor context after a graph change. */
        void syncEditorToGraph(const ParticleGraph& graph, ECS::Entity entity, u32 groupId);

        /** @brief Shift+A and right-click menu, the dropped variables, and the node a pick creates. */
        void handleAddNodePopup(ParticleGraph& graph, const ImRect& canvasRect,
                                std::vector<GraphVariable>& variables);

        /** @brief Draws every node, keeping bound nodes and their variables equal. */
        void drawNodes(ParticleGraph& graph, std::vector<GraphVariable>& variables);

        /** @brief Draws the links and handles connecting and deleting. Call after drawNodes(). */
        void drawLinks(ParticleGraph& graph);

        ImGuiTextFilter m_VariableFilter; ///< Search text of the variables overlay.
        ECS::Entity m_ShownEntity = ECS::INVALID_ENTITY_ID; ///< Entity whose graph was drawn last frame.
        u32 m_SelectedGroupId = 0; ///< Group picked in the groups overlay; 0 or a deleted ID falls back to the first group.
        u32 m_ShownGroupId = 0;    ///< Group whose graph was drawn last frame.
        Overlay::GroupsOverlayState m_GroupsState; ///< Rename state of the groups overlay.
        ImVec2 m_VariablesOffset{5.0f, 5.0f}; ///< Variables overlay position relative to the canvas corner.
        ImVec2 m_GroupsOffset{5.0f, -1.0f};   ///< Groups overlay position; y < 0 until first placed below the variables.
        ImGuiTextFilter m_AddNodeFilter;  ///< Search text of the "Add Node" popup.
        ed::PinId m_PendingLinkPin;       ///< Pin a link was dragged from into empty space; the next node from the popup connects to it.
    };
} 