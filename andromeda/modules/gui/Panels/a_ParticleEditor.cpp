#include "a_ParticleEditor.hpp"
#include "imgui.h"
#include "a_SelectionContext.hpp"
#include "a_registry.hpp"
#include "a_components.hpp"
#include "a_Node_Renderer.hpp"
#include "a_Nodes.hpp"
#include <algorithm>
#include <array>
#include <string>
#include <variant>
#include "IconsLucide.h"
#include "a_node_graph.hpp"
namespace Andromeda::Gui {

    namespace {
        constexpr float kOverlayMargin = 5.0f; ///< Gap between the canvas corner and the variables overlay.
        constexpr float kBarPadding = 4.0f;    ///< Extra space above and below the widgets in a menu bar.

        /** @brief Type dropdown labels, in the order of the GraphValue alternatives (value.index()). */
        constexpr std::array<const char*, 3> kGraphValueTypeNames = {"Int", "Float", "Bool"};
        static_assert(kGraphValueTypeNames.size() == std::variant_size_v<GraphValue>,
                      "kGraphValueTypeNames and makeDefaultGraphValue must list every GraphValue alternative");

        /** @brief A default value of the GraphValue alternative at @p index. */
        GraphValue makeDefaultGraphValue(size_t index) {
            switch (index) {
            case 0:
                return i32{0};
            case 1:
                return 0.0f;
            default:
                return false;
            }
        }

        /** @brief "NewVariable", or "NewVariable1", "NewVariable2", ... if that name is taken. */
        std::string makeUniqueVariableName(const std::vector<GraphVariable>& variables) {
            for (u32 suffix = 0;; ++suffix) {
                std::string name = suffix == 0 ? "NewVariable" : "NewVariable" + std::to_string(suffix);
                const bool taken = std::any_of(variables.begin(), variables.end(),
                                               [&name](const GraphVariable& v) { return v.name == name; });
                if (!taken)
                    return name;
            }
        }

        /** @brief Search field and "+" button, drawn inside a menu bar. */
        void drawVariableToolbar(ParticleGroup& group, ImGuiTextFilter& filter) {
            const float buttonSize = ImGui::GetFrameHeight();

            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + kBarPadding);

            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - buttonSize - ImGui::GetStyle().ItemSpacing.x);
            if (ImGui::InputTextWithHint("##search", ICON_LC_SEARCH " Search...", filter.InputBuf,
                                         IM_ARRAYSIZE(filter.InputBuf)))
                filter.Build();

            if (ImGui::Button(ICON_LC_PLUS, ImVec2(buttonSize, buttonSize)))
                group.graphVariables.push_back({makeUniqueVariableName(group.graphVariables), 0.0f});
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Add variable");
        }

        /**
         * @brief One table row: name, type, value and a remove button.
         * @return true when the remove button was pressed. The caller erases after its loop.
         */
        bool drawVariableRow(GraphVariable& variable) {
            ImGui::TableNextRow();

            ImGui::TableNextColumn();
            char nameBuffer[64];
            const size_t length = variable.name.copy(nameBuffer, sizeof(nameBuffer) - 1);
            nameBuffer[length] = '\0';
            ImGui::SetNextItemWidth(-FLT_MIN);
            if (ImGui::InputText("##name", nameBuffer, sizeof(nameBuffer)))
                variable.name = nameBuffer;

            ImGui::TableNextColumn();
            const int currentType = static_cast<int>(variable.value.index());
            int selectedType = currentType;
            ImGui::SetNextItemWidth(-FLT_MIN);
            if (ImGui::Combo("##type", &selectedType, kGraphValueTypeNames.data(),
                             static_cast<int>(kGraphValueTypeNames.size())) &&
                selectedType != currentType)
                variable.value = makeDefaultGraphValue(static_cast<size_t>(selectedType));

            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth(-FLT_MIN);
            std::visit([](auto& value) { Node::drawField(value); }, variable.value);

            ImGui::TableNextColumn();
            return ImGui::Button(ICON_LC_X);
        }

        void drawGroupVariables(ParticleGroup& group, ImGuiTextFilter& filter) {
            const ImVec2 framePadding = ImGui::GetStyle().FramePadding;
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(framePadding.x, framePadding.y + kBarPadding));
            const bool open =
                ImGui::BeginChild("##variables", ImVec2(0.0f, 0.0f),
                                  ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_AlwaysUseWindowPadding,
                                  ImGuiWindowFlags_MenuBar);
            ImGui::PopStyleVar();

            if (open) {
                if (ImGui::BeginMenuBar()) {
                    drawVariableToolbar(group, filter);
                    ImGui::EndMenuBar();
                }

                if (group.graphVariables.empty()) {
                    ImGui::TextDisabled("No variables  press " ICON_LC_PLUS " to add one");
                } else if (ImGui::BeginTable("##variableTable", 4, ImGuiTableFlags_SizingStretchProp)) {
                    ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 1.5f);
                    ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthStretch, 1.0f);
                    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch, 1.5f);
                    ImGui::TableSetupColumn("Remove", ImGuiTableColumnFlags_WidthFixed);

                    i32 indexToRemove = -1;
                    for (i32 i = 0; i < static_cast<i32>(group.graphVariables.size()); ++i) {
                        GraphVariable& variable = group.graphVariables[static_cast<size_t>(i)];
                        if (!filter.PassFilter(variable.name.c_str()))
                            continue;

                        ImGui::PushID(i);
                        if (drawVariableRow(variable))
                            indexToRemove = i;
                        ImGui::PopID();
                    }
                    ImGui::EndTable();

                    if (indexToRemove >= 0)
                        group.graphVariables.erase(group.graphVariables.begin() + indexToRemove);
                }
            }
            ImGui::EndChild();
        }
    } // namespace

    void ParticleEditor::drawVariablesWindow(const ImVec2& canvasPos, const ImVec2& canvasSize, float rounding,
                                             std::span<ParticleGroup> groups) {
        ImGui::SetCursorScreenPos(ImVec2(canvasPos.x + kOverlayMargin, canvasPos.y + kOverlayMargin));

        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, rounding);
        const bool open = ImGui::BeginChild("##GraphVariables", canvasSize,
                                            ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX | ImGuiChildFlags_ResizeY);
        ImGui::PopStyleVar();

        if (open) {
            ImGui::SeparatorText("Variables");
            m_VariableFilters.resize(groups.size());
            for (size_t i = 0; i < groups.size(); ++i) {
                ParticleGroup& group = groups[i];
                ImGui::PushID(static_cast<int>(i));
                if (ImGui::CollapsingHeader(group.groupName.c_str()))
                    drawGroupVariables(group, m_VariableFilters[i]);
                ImGui::PopID();
            }
        }
        ImGui::EndChild();
    }

    namespace {
    /** @brief "AddNode" -> "Add": the menu shows node names without the redundant suffix. */
    std::string_view nodeDisplayName(std::string_view typeName) {
        constexpr std::string_view suffix = "Node";
        if (typeName.size() > suffix.size() && typeName.ends_with(suffix))
            typeName.remove_suffix(suffix.size());
        return typeName;
    }

    /**
     * @brief Searchable "Add Node" popup.
     * @param openedAt Receives the screen position the popup was opened at, where the new node goes.
     * @return Index into Meta::ReflectedNodesNames of the picked node, or -1.
     * @note Call between ed::Suspend() and ed::Resume().
     */
    i32 drawAddNodePopup(ImGuiTextFilter& filter, float rounding, ImVec2& openedAt) {
        ImGui::SetNextWindowSizeConstraints(ImVec2(220.0f, 0.0f), ImVec2(FLT_MAX, 320.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, rounding);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 8.0f));
        const bool open = ImGui::BeginPopup("Add Node");
        ImGui::PopStyleVar(2);
        if (!open)
            return -1;

        // Only valid inside the popup, so it has to be read here rather than by the caller.
        openedAt = ImGui::GetMousePosOnOpeningCurrentPopup();

        if (ImGui::IsWindowAppearing()) {
            filter.Clear();
            ImGui::SetKeyboardFocusHere();
        }
        ImGui::SetNextItemWidth(-FLT_MIN);
        const bool enterPressed =
            ImGui::InputTextWithHint("##nodeSearch", ICON_LC_SEARCH " Search nodes...", filter.InputBuf,
                                     IM_ARRAYSIZE(filter.InputBuf), ImGuiInputTextFlags_EnterReturnsTrue);
        filter.Build();
        ImGui::Separator();

        i32 picked = -1;
        i32 firstMatch = -1;
        for (i32 i = 0; i < static_cast<i32>(Meta::ReflectedNodesNames.size()); ++i) {
            const std::string label(nodeDisplayName(Meta::ReflectedNodesNames[static_cast<size_t>(i)]));
            if (!filter.PassFilter(label.c_str()))
                continue;
            if (firstMatch < 0)
                firstMatch = i;
            ImGui::PushID(i);
            if (ImGui::Selectable(label.c_str()))
                picked = i;
            ImGui::PopID();
        }
        if (firstMatch < 0)
            ImGui::TextDisabled("No matching nodes");

        if (enterPressed && firstMatch >= 0)
            picked = firstMatch;
        if (picked >= 0)
            ImGui::CloseCurrentPopup();

        ImGui::EndPopup();
        return picked;
    }

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

    ImVec2 toImVec2(vec2 v) {
        return ImVec2(v.x, v.y);
    }

    vec2 toVec2(ImVec2 v) {
        return vec2(v.x, v.y);
    }
    } // namespace

    void ParticleEditor::onGuiRender(EditorContext& ctx) {
        ImGui::SetNextWindowSizeConstraints(ImVec2(200, 100), ImVec2(FLT_MAX, FLT_MAX));

        if (ImGui::Begin(m_Name, &m_IsOpen)) {
            ECS::Entity selectedEntity = ctx.selection->getSelectedEntity();

            if (selectedEntity != ECS::INVALID_ENTITY_ID) {
                ECS::EntityHandle handle = {selectedEntity, ctx.registry};

                if (handle.has<ECS::Component::ParticleSystem>()) {
                    auto& particleSystem = handle.get<ECS::Component::ParticleSystem>();
                    const ImVec2 canvasPos = ImGui::GetCursorScreenPos();
                    ImVec2 canvasSize = ImGui::GetContentRegionAvail();
                    canvasSize.x *= 0.25f;
                    canvasSize.y *= 0.5f;
                    auto group = particleSystem.getParticleGroups();
                    ParticleGraph& graph = group[0].graph; // later: the group selected in the editor
                    ed::SetCurrentEditor(m_NodeEditorContext);
                    ed::Begin("Particle Graph");

                    // Every graph numbers its nodes from 1, but all graphs share this one editor
                    // context, which keeps positions per node ID. After switching entities, node 1 of
                    // the new graph would sit where node 1 of the old one was - so push the stored
                    // positions back into the editor whenever the shown graph changes.
                    if (selectedEntity != m_ShownEntity) {
                        ed::ClearSelection();
                        for (const NodeInstance& node : graph.nodes)
                            ed::SetNodePosition(ed::NodeId(node.id), toImVec2(node.position));
                        m_ShownEntity = selectedEntity;
                    }

                    ed::Suspend();
                    const bool shiftA = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
                                        !ImGui::GetIO().WantTextInput &&
                                        ImGui::IsKeyChordPressed(ImGuiMod_Shift | ImGuiKey_A);
                    if (shiftA || isBackgroundContextClick())
                        ImGui::OpenPopup("Add Node");

                    ImVec2 openedAt;
                    const i32 pickedNode = drawAddNodePopup(m_AddNodeFilter, ed::GetStyle().NodeRounding, openedAt);
                    ed::Resume();

                    if (pickedNode >= 0) {
                        const ImVec2 position = ed::ScreenToCanvas(openedAt);
                        NodeInstance& node = addNode(graph, static_cast<size_t>(pickedNode), toVec2(position));
                        ed::SetNodePosition(ed::NodeId(node.id), position);
                    }

                    for (NodeInstance& node : graph.nodes) {
                        std::visit([&node](auto& data) { Node::drawNode(data, node.id); }, node.data);
                        // The editor owns dragging; mirror its position so the graph data stays current.
                        node.position = toVec2(ed::GetNodePosition(ed::NodeId(node.id)));
                    }
                    ed::End();
                    const float nodeRounding = ed::GetStyle().NodeRounding;
                    ed::SetCurrentEditor(nullptr);
                    drawVariablesWindow(canvasPos, canvasSize, nodeRounding, group);
                }
            }
        }
        ImGui::End();
    }
}
