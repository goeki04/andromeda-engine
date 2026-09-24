#include "a_ParticleEditor.hpp"

#include <cstdio>
#include <cstring>
#include <span>
#include <string>
#include <variant>
#include "imgui.h"
#include "imgui_internal.h"
#include "IconsLucide.h"
#include "a_GraphCoords.hpp"
#include "a_GraphOverlay.hpp"
#include "a_GroupsOverlay.hpp"
#include "a_Node_Renderer.hpp"
#include "a_Nodes.hpp"
#include "a_SelectionContext.hpp"
#include "a_VariablesOverlay.hpp"
#include "a_components.hpp"
#include "a_node_graph.hpp"
#include "a_registry.hpp"

namespace Andromeda::Gui {

    namespace {
        /**
         * @brief Right-click on the canvas background: press and release without dragging.
         * @details Replaces ed::ShowBackgroundContextMenu(), which cancels on 1 px of mouse movement and
         *          cannot see the click that closes an already open popup, so reopening needed two clicks.
         */
        bool isBackgroundContextClick() {
            const ImGuiIO& io = ImGui::GetIO();
            const float threshold = io.MouseDragThreshold;
            return ImGui::IsMouseReleased(ImGuiMouseButton_Right) &&
                   io.MouseDragMaxDistanceSqr[ImGuiMouseButton_Right] < threshold * threshold &&
                   ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByPopup) && !ed::GetHoveredNode() &&
                   !ed::GetHoveredPin() && !ed::GetHoveredLink();
        }

        /** @brief Default size of an overlay: a quarter of the canvas wide, half of it high. */
        ImVec2 getOverlaySize() {
            return ImVec2(ImGui::GetContentRegionAvail().x * 0.25f, ImGui::GetContentRegionAvail().y * 0.5f);
        }
    } // namespace

    void ParticleEditor::registerSettingsHandler() {
        ImGuiSettingsHandler handler;
        handler.TypeName = kSettingsTypeName;
        handler.TypeHash = ImHashStr(kSettingsTypeName);
        handler.UserData = this;

        // [ParticleEditor][Overlays]: the one entry; any non-null pointer tells ImGui to feed us its lines.
        handler.ReadOpenFn = [](ImGuiContext*, ImGuiSettingsHandler*, const char* name) -> void* {
            return std::strcmp(name, "Overlays") == 0 ? reinterpret_cast<void*>(1) : nullptr;
        };
        // Lines that do not parse (edited by hand, older format) are skipped and the default stays.
        handler.ReadLineFn = [](ImGuiContext*, ImGuiSettingsHandler* h, void*, const char* line) {
            auto* self = static_cast<ParticleEditor*>(h->UserData);
            float x = 0.0f;
            float y = 0.0f;
            if (std::sscanf(line, "Variables=%f,%f", &x, &y) == 2)
                self->m_VariablesOffset = ImVec2(x, y);
            else if (std::sscanf(line, "Groups=%f,%f", &x, &y) == 2)
                self->m_GroupsOffset = ImVec2(x, y);
        };
        handler.WriteAllFn = [](ImGuiContext*, ImGuiSettingsHandler* h, ImGuiTextBuffer* out) {
            const auto* self = static_cast<const ParticleEditor*>(h->UserData);
            out->appendf("[%s][Overlays]\n", h->TypeName);
            out->appendf("Variables=%.0f,%.0f\n", self->m_VariablesOffset.x, self->m_VariablesOffset.y);
            out->appendf("Groups=%.0f,%.0f\n", self->m_GroupsOffset.x, self->m_GroupsOffset.y);
            out->append("\n");
        };
        ImGui::AddSettingsHandler(&handler);
    }

    void ParticleEditor::drawVariablesWindow(const ImRect& canvas, const ImVec2& defaultSize, float rounding,
                                             ECS::Component::ParticleSystem& system) {
        if (Overlay::beginMovableOverlay("##GraphVariables", "Variables", canvas, defaultSize, rounding,
                                         m_VariablesOffset))
            Overlay::drawVariableList(system, m_VariableFilter);
        ImGui::EndChild();
    }

    void ParticleEditor::drawParticleGroupsWindow(ECS::Component::ParticleSystem& system, const ImRect& canvas,
                                                  const ImVec2& defaultSize, float rounding) {
        // First time shown: start just below the default spot of the variables overlay.
        if (m_GroupsOffset.y < 0.0f)
            m_GroupsOffset =
                ImVec2(m_VariablesOffset.x, m_VariablesOffset.y + defaultSize.y + Overlay::kOverlayMargin);

        if (Overlay::beginMovableOverlay("##GraphParticleGroups", "Particle Groups", canvas, defaultSize, rounding,
                                         m_GroupsOffset))
            Overlay::drawParticleGroupList(system, m_ShownGroupId, m_SelectedGroupId, m_GroupsState);
        ImGui::EndChild();
    }

    ParticleGroup& ParticleEditor::activeGroup(ECS::Component::ParticleSystem& system, ECS::Entity entity) {
        // A new entity starts at its first group; the previous entity's choice means nothing here.
        if (entity != m_ShownEntity)
            m_SelectedGroupId = 0;

        // Looked up by ID every frame instead of keeping an index or pointer: adding or deleting groups
        // moves them in the vector. Not found (0, deleted, groups turned off): fall back to the first
        // group, which always exists.
        const std::span<ParticleGroup> groups = system.getParticleGroups();
        ParticleGroup* active = &groups.front();
        for (ParticleGroup& group : groups)
            if (group.id == m_SelectedGroupId)
                active = &group;
        return *active;
    }

    void ParticleEditor::syncEditorToGraph(const ParticleGraph& graph, ECS::Entity entity, u32 groupId) {
        // Every graph numbers its nodes from 1, but all graphs share this one editor context, which keeps
        // positions per node ID. After switching entity or group, node 1 of the new graph would sit where
        // node 1 of the old one was - so push the stored positions back into the editor.
        if (entity == m_ShownEntity && groupId == m_ShownGroupId)
            return;

        ed::ClearSelection();
        for (const NodeInstance& node : graph.nodes)
            ed::SetNodePosition(ed::NodeId(node.id), toImVec2(node.position));
        m_ShownEntity = entity;
        m_ShownGroupId = groupId;
        m_PendingLinkPin = ed::PinId(); // belonged to the previous graph
    }

    void ParticleEditor::handleAddNodePopup(ParticleGraph& graph, const ImRect& canvasRect,
                                            std::vector<GraphVariable>& variables) {
        ed::Suspend();
        const bool shiftA = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
                            !ImGui::GetIO().WantTextInput &&
                            ImGui::IsKeyChordPressed(ImGuiMod_Shift | ImGuiKey_A);
        if (shiftA || isBackgroundContextClick()) {
            // Opened normally, not from a dragged link: the new node must not connect to anything.
            m_PendingLinkPin = ed::PinId();
            ImGui::OpenPopup("Add Node");
        }

        ImVec2 openedAt;
        const i32 pickedNode = Node::drawAddNodePopup(m_AddNodeFilter, ed::GetStyle().NodeRounding, openedAt);
        Overlay::acceptVariableDrop(canvasRect, variables, graph);
        ed::Resume();

        if (pickedNode >= 0) {
            const ImVec2 position = ed::ScreenToCanvas(openedAt);
            NodeInstance& node = addNode(graph, static_cast<size_t>(pickedNode), toVec2(position));
            ed::SetNodePosition(ed::NodeId(node.id), position);

            // Popup opened by dropping a link on empty canvas: connect the new node to that pin.
            if (m_PendingLinkPin)
                Node::connectToNewNode(graph, m_PendingLinkPin, node);
            m_PendingLinkPin = ed::PinId();
        }

        // Popup closed without picking (click outside, Escape): forget the dragged-from pin, or the next
        // node added the normal way would connect to it. Checked after the pick above, since picking
        // closes the popup in the same frame.
        if (m_PendingLinkPin && !ImGui::IsPopupOpen("Add Node"))
            m_PendingLinkPin = ed::PinId();
    }

    void ParticleEditor::drawNodes(ParticleGraph& graph, std::vector<GraphVariable>& variables) {
        for (NodeInstance& node : graph.nodes) {
            // Bound variable nodes: variable -> node before drawing, edits node -> variable after.
            // Nodes later in this loop already see an edit made in an earlier one.
            GraphVariable* variable = Overlay::syncFromVariable(node, variables);
            const std::string title =
                variable ? std::string(ICON_LC_VARIABLE " ") + variable->name : std::string();
            std::visit([&](auto& data) { Node::drawNode(data, node.id, title); }, node.data);
            if (variable)
                Overlay::writeBackToVariable(node, *variable);
            // The editor owns dragging; mirror its position so the graph data stays current.
            node.position = toVec2(ed::GetNodePosition(ed::NodeId(node.id)));
        }
    }

    void ParticleEditor::drawLinks(ParticleGraph& graph) {
        // After all nodes: the editor needs their pins before it can draw, connect or delete links.
        // Each of these runs once per frame, not once per node.
        Node::drawLink(graph);
        if (const ed::PinId droppedFrom = Node::createLink(graph)) {
            // Link released over empty canvas: offer the node menu there. The popup appears next frame;
            // the pin is remembered until a node is picked or the popup is closed.
            m_PendingLinkPin = droppedFrom;
            ed::Suspend(); // popups open in screen space, like the ones above
            ImGui::OpenPopup("Add Node");
            ed::Resume();
        }
        Node::deleteSelection(graph);
    }

    void ParticleEditor::onGuiRender(EditorContext& ctx) {
        ImGui::SetNextWindowSizeConstraints(ImVec2(200, 100), ImVec2(FLT_MAX, FLT_MAX));

        if (ImGui::Begin(m_Name, &m_IsOpen)) {
            const ECS::Entity selectedEntity = ctx.selection->getSelectedEntity();
            ECS::EntityHandle handle = {selectedEntity, ctx.registry};

            if (selectedEntity != ECS::INVALID_ENTITY_ID && handle.has<ECS::Component::ParticleSystem>()) {
                auto& particleSystem = handle.get<ECS::Component::ParticleSystem>();
                const ImVec2 overlaySize = getOverlaySize();
                const ImVec2 canvasPos = ImGui::GetCursorScreenPos();
                const ImVec2 canvasSize = ImGui::GetContentRegionAvail(); // the canvas fills the rest
                const ImRect canvasRect(canvasPos,
                                        ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y));

                ParticleGroup& group = activeGroup(particleSystem, selectedEntity);
                ParticleGraph& graph = group.graph;

                // The graph itself is run by ParticleGraphSystem, every frame and for every group.
                // The editor only shows the values it computed.
                ed::SetCurrentEditor(m_NodeEditorContext);
                ed::Begin("Particle Graph");

                syncEditorToGraph(graph, selectedEntity, group.id);
                handleAddNodePopup(graph, canvasRect, particleSystem.graphVariables);
                drawNodes(graph, particleSystem.graphVariables);
                drawLinks(graph);

                // Read while the editor is still current: every ed:: call, GetStyle() included,
                // dereferences the current editor, which is null after SetCurrentEditor(nullptr).
                const float nodeRounding = ed::GetStyle().NodeRounding;
                ed::End();
                ed::SetCurrentEditor(nullptr);

                drawVariablesWindow(canvasRect, overlaySize, nodeRounding, particleSystem);
                if (particleSystem.useParticleGroups)
                    drawParticleGroupsWindow(particleSystem, canvasRect, overlaySize, nodeRounding);
            }
        }
        ImGui::End();
    }
}
