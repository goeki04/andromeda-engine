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
#include "a_node_graph.hpp"
#include "imgui_internal.h"
#include <cstring>
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

        /** @brief Drag-and-drop payload type of a variable dragged from the list onto the canvas. */
        constexpr const char* kVariablePayload = "GRAPH_VARIABLE";

        /** @brief Search field and "+" button, drawn inside a menu bar. */
        void drawVariableToolbar(ECS::Component::ParticleSystem& system, ImGuiTextFilter& filter) {
            const float buttonSize = ImGui::GetFrameHeight();

            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + kBarPadding);

            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - buttonSize - ImGui::GetStyle().ItemSpacing.x);
            if (ImGui::InputTextWithHint("##search", ICON_LC_SEARCH " Search...", filter.InputBuf,
                                         IM_ARRAYSIZE(filter.InputBuf)))
                filter.Build();

            if (ImGui::Button(ICON_LC_PLUS, ImVec2(buttonSize, buttonSize)))
                system.graphVariables.push_back(
                    {system.nextVariableId++, makeUniqueVariableName(system.graphVariables), 0.0f});
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Add variable");
        }

        constexpr float kRowPadding = 8.0f;  ///< Space between a variable row's background and its widgets.
        constexpr float kRowRounding = 6.0f; ///< Corner rounding of a variable row's background.

        /** @brief @p color with its RGB scaled by @p factor; below 1 darkens, alpha stays. */
        ImU32 scaledColor(ImVec4 color, float factor) {
            return ImGui::GetColorU32(ImVec4(color.x * factor, color.y * factor, color.z * factor, color.w));
        }

        /**
         * @brief One variable as a rounded, draggable card: remove button, name, type, value, grip icon.
         * @details An invisible button spans the whole card *behind* the widgets. Clicking a widget uses
         *          the widget; clicking anywhere else on the card and dragging starts a drag onto the canvas.
         *          SetNextItemAllowOverlap is what lets the widgets on top still receive the mouse.
         *          The grip sits on the right, next to the canvas, so the way to drag is short.
         *          The payload is the variable's ID - a copied value, never a pointer into the vector.
         * @return true when the remove button was pressed. The caller erases after its loop.
         */
        bool drawVariableRow(GraphVariable& variable) {
            const ImGuiStyle& style = ImGui::GetStyle();
            const float frameHeight = ImGui::GetFrameHeight();
            const float rowWidth = ImGui::GetContentRegionAvail().x;
            const float rowHeight = frameHeight + kRowPadding * 2.0f;
            const ImVec2 rowMin = ImGui::GetCursorScreenPos();
            const ImVec2 rowMax(rowMin.x + rowWidth, rowMin.y + rowHeight);

            // Drag area behind everything else in the row.
            ImGui::SetNextItemAllowOverlap();
            ImGui::InvisibleButton("##row", ImVec2(rowWidth, rowHeight));
            const bool rowHovered = ImGui::IsItemHovered();
            const bool rowHeld = ImGui::IsItemActive();
            if (ImGui::BeginDragDropSource()) {
                ImGui::SetDragDropPayload(kVariablePayload, &variable.id, sizeof(variable.id));
                ImGui::Text(ICON_LC_VARIABLE " %s", variable.name.c_str());
                ImGui::EndDragDropSource();
            }

            const ImVec4 background = style.Colors[ImGuiCol_WindowBg];
            const float shade = rowHeld ? 0.55f : (rowHovered ? 0.8f : 0.7f);
            ImGui::GetWindowDrawList()->AddRectFilled(rowMin, rowMax, scaledColor(background, shade), kRowRounding);

            // Widgets on top of the drag area, in one line.
            const float innerWidth = rowWidth - kRowPadding * 2.0f;
            const float fixedWidth = frameHeight * 2.0f + style.ItemSpacing.x * 4.0f; // remove button + grip + gaps
            const float flexibleWidth = std::max(innerWidth - fixedWidth, 0.0f);
            ImGui::SetCursorScreenPos(ImVec2(rowMin.x + kRowPadding, rowMin.y + kRowPadding));

            const bool removePressed = ImGui::Button(ICON_LC_X, ImVec2(frameHeight, frameHeight));

            ImGui::SameLine();
            char nameBuffer[64];
            const size_t length = variable.name.copy(nameBuffer, sizeof(nameBuffer) - 1);
            nameBuffer[length] = '\0';
            ImGui::SetNextItemWidth(flexibleWidth * 0.4f);
            if (ImGui::InputText("##name", nameBuffer, sizeof(nameBuffer)))
                variable.name = nameBuffer;

            ImGui::SameLine();
            const int currentType = static_cast<int>(variable.value.index());
            int selectedType = currentType;
            ImGui::SetNextItemWidth(flexibleWidth * 0.25f);
            if (ImGui::Combo("##type", &selectedType, kGraphValueTypeNames.data(),
                             static_cast<int>(kGraphValueTypeNames.size())) &&
                selectedType != currentType)
                variable.value = makeDefaultGraphValue(static_cast<size_t>(selectedType));

            ImGui::SameLine();
            ImGui::SetNextItemWidth(flexibleWidth * 0.35f);
            std::visit([](auto& value) { Node::drawField(value); }, variable.value);

            // Right-aligned grip. Plain text has no ID, so clicks on it reach the drag area underneath.
            ImGui::SameLine();
            ImGui::SetCursorScreenPos(ImVec2(rowMax.x - kRowPadding - frameHeight, rowMin.y + kRowPadding));
            ImGui::AlignTextToFramePadding();
            ImGui::TextDisabled(ICON_LC_GRIP_VERTICAL);

            // Continue below the card. The Dummy is a real item, so the manual cursor moves above do not
            // count as extending the window (ImGui asserts on SetCursorPos without a following item).
            ImGui::SetCursorScreenPos(ImVec2(rowMin.x, rowMax.y));
            ImGui::Dummy(ImVec2(0.0f, 0.0f));

            return removePressed;
        }

        /** @brief The variable a node is bound to, or 0 for unbound and non-variable nodes. */
        u32 boundVariableId(const NodeInstance& node) {
            return std::visit([](const auto& data) -> u32 {
                if constexpr (requires { data.variableId; })
                    return data.variableId;
                else
                    return 0;
            }, node.data);
        }

        /**
         * @brief Deletes every node bound to @p variableId, links included, from the graphs of all groups.
         * @details All groups, not just the one shown: the variable is shared, so any graph can use it.
         *          IDs are collected first because removeNode erases from the vector being searched.
         */
        void removeNodesBoundTo(ECS::Component::ParticleSystem& system, u32 variableId) {
            for (ParticleGroup& group : system.allParticleGroups()) {
                std::vector<u32> boundNodes;
                for (const NodeInstance& node : group.graph.nodes) {
                    if (boundVariableId(node) == variableId)
                        boundNodes.push_back(node.id);
                }
                for (const u32 nodeId : boundNodes)
                    Node::removeNode(group.graph, nodeId);
            }
        }

        /** @brief The variable list of a ParticleSystem: menu bar with search and "+", then the table. */
        void drawVariableList(ECS::Component::ParticleSystem& system, ImGuiTextFilter& filter) {
            std::vector<GraphVariable>& variables = system.graphVariables;
            const ImVec2 framePadding = ImGui::GetStyle().FramePadding;
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(framePadding.x, framePadding.y + kBarPadding));
            const bool open =
                ImGui::BeginChild("##variables", ImVec2(0.0f, 0.0f),
                                  ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_AlwaysUseWindowPadding,
                                  ImGuiWindowFlags_MenuBar);
            ImGui::PopStyleVar();

            if (open) {
                if (ImGui::BeginMenuBar()) {
                    drawVariableToolbar(system, filter);
                    ImGui::EndMenuBar();
                }

                if (variables.empty()) {
                    ImGui::TextDisabled("No variables  press " ICON_LC_PLUS " to add one");
                } else {
                    i32 indexToRemove = -1;
                    for (i32 i = 0; i < static_cast<i32>(variables.size()); ++i) {
                        GraphVariable& variable = variables[static_cast<size_t>(i)];
                        if (!filter.PassFilter(variable.name.c_str()))
                            continue;

                        ImGui::PushID(i);
                        if (drawVariableRow(variable))
                            indexToRemove = i;
                        ImGui::PopID();
                    }

                    if (indexToRemove >= 0) {
                        const u32 removedId = variables[static_cast<size_t>(indexToRemove)].id;
                        variables.erase(variables.begin() + indexToRemove);
                        removeNodesBoundTo(system, removedId);
                    }
                }
            }
            ImGui::EndChild();
        }
    } // namespace

    void ParticleEditor::drawVariablesWindow(const ImVec2& canvasPos, const ImVec2& canvasSize, float rounding,
                                             ECS::Component::ParticleSystem& system) {
        ImGui::SetCursorScreenPos(ImVec2(canvasPos.x + kOverlayMargin, canvasPos.y + kOverlayMargin));

        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, rounding);
        const bool open = ImGui::BeginChild("##GraphVariables", canvasSize,
                                            ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX | ImGuiChildFlags_ResizeY);
        ImGui::PopStyleVar();

        if (open) {
            ImGui::SeparatorText("Variables");
            drawVariableList(system, m_VariableFilter);
        }
        ImGui::EndChild();
    }

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

    ImVec2 toImVec2(vec2 v) {
        return ImVec2(v.x, v.y);
    }

    vec2 toVec2(ImVec2 v) {
        return vec2(v.x, v.y);
    }

    /** @brief The variable node type matching the variable's type, bound to it and holding its current value. */
    NodeData makeNodeForVariable(const GraphVariable& variable) {
        return std::visit([&variable](const auto& value) -> NodeData {
            using T = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<T, i32>) {
                return Node::IntVariable{.value = value, .variableId = variable.id};
            } else if constexpr (std::is_same_v<T, float>) {
                return Node::FloatVariable{.value = value, .variableId = variable.id};
            } else {
                static_assert(std::is_same_v<T, bool>, "New GraphValue type: add its node here");
                return Node::BoolVariable{.value = value, .variableId = variable.id};
            }
        }, variable.value);
    }

    /** @brief True for node types that can be bound to a GraphVariable (they have a variableId). */
    template<typename NodeT>
    constexpr bool isVariableNode = requires(NodeT& node) { node.variableId; };

    /**
     * @brief Brings a node that is bound to a variable in line with it. Call right before drawing the node.
     * @details - Variable missing: normally impossible, deleting a variable removes its nodes (see
     *            removeNodesBoundTo). If a scene still has such a node, it is unbound and keeps its last
     *            value as its own constant - it cannot be removed here, the caller is iterating the nodes.
     *          - Variable changed its type: the node is replaced by the matching variable node type. ID and
     *            position stay; pin IDs stay too, since all variable nodes share the field layout.
     *          - Otherwise the node's value is set from the variable.
     * @return The bound variable, or nullptr for unbound and non-variable nodes. The pointer points into
     *         @p variables and is only valid until that vector changes.
     */
    GraphVariable* syncFromVariable(NodeInstance& node, std::vector<GraphVariable>& variables) {
        GraphVariable* bound = nullptr;
        bool typeChanged = false;

        std::visit([&](auto& data) {
            using NodeT = std::decay_t<decltype(data)>;
            if constexpr (isVariableNode<NodeT>) {
                if (data.variableId == 0)
                    return;
                const auto it = std::find_if(variables.begin(), variables.end(),
                                             [&data](const GraphVariable& v) { return v.id == data.variableId; });
                if (it == variables.end()) {
                    data.variableId = 0;
                    return;
                }
                bound = &*it;

                using ValueT = decltype(data.value.value);
                if (std::holds_alternative<ValueT>(it->value))
                    data.value.value = std::get<ValueT>(it->value);
                else
                    typeChanged = true;
            }
        }, node.data);

        // Outside the visit: replacing the variant's content while visiting it would destroy the object
        // the lambda still refers to.
        if (typeChanged)
            node.data = makeNodeForVariable(*bound);
        return bound;
    }

    /**
     * @brief Writes an edit made in a bound node back to its variable. Call right after drawing the node.
     * @details syncFromVariable made node and variable equal before drawing, so any difference now is an
     *          edit in this node. Checking for a difference (instead of always copying) matters when several
     *          nodes share one variable: an untouched node must not write the old value back over the edit.
     */
    void writeBackToVariable(const NodeInstance& node, GraphVariable& variable) {
        std::visit([&variable](const auto& data) {
            using NodeT = std::decay_t<decltype(data)>;
            if constexpr (isVariableNode<NodeT>) {
                using ValueT = decltype(data.value.value);
                if (std::holds_alternative<ValueT>(variable.value) && std::get<ValueT>(variable.value) != data.value.value)
                    variable.value = data.value.value;
            }
        }, node.data);
    }

    /**
     * @brief Accepts a variable dropped onto the canvas and creates a matching node under the mouse.
     * @param canvasRect The whole canvas in screen space.
     * @note Call between ed::Suspend() and ed::Resume(): screen coordinates, but still inside the editor
     *       so ed::ScreenToCanvas works.
     */
    void acceptVariableDrop(const ImRect& canvasRect, const std::vector<GraphVariable>& variables,
                            ParticleGraph& graph) {
        // The variables overlay is a child window, so hovering it does not count as hovering this one:
        // a variable dropped back onto the list does not create a node. The drag keeps an item active,
        // hence AllowWhenBlockedByActiveItem.
        if (!ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem))
            return;
        if (!ImGui::BeginDragDropTargetCustom(canvasRect, ImGui::GetID("##variableDrop")))
            return;

        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kVariablePayload)) {
            u32 variableId = 0;
            std::memcpy(&variableId, payload->Data, sizeof(variableId));

            const auto it = std::find_if(variables.begin(), variables.end(),
                                         [variableId](const GraphVariable& v) { return v.id == variableId; });
            if (it != variables.end()) {
                const ImVec2 position = ed::ScreenToCanvas(ImGui::GetMousePos());
                NodeInstance& node = addNode(graph, makeNodeForVariable(*it), toVec2(position));
                ed::SetNodePosition(ed::NodeId(node.id), position);
            }
        }
        ImGui::EndDragDropTarget();
    }
    } // namespace

    ImVec2 getCanvasSize() {
        return ImVec2(ImGui::GetContentRegionAvail().x * 0.25f, ImGui::GetContentRegionAvail().y * 0.5f);
    }

    void ParticleEditor::onGuiRender(EditorContext& ctx) {
        ImGui::SetNextWindowSizeConstraints(ImVec2(200, 100), ImVec2(FLT_MAX, FLT_MAX));

        if (ImGui::Begin(m_Name, &m_IsOpen)) {
            ECS::Entity selectedEntity = ctx.selection->getSelectedEntity();

            if (selectedEntity != ECS::INVALID_ENTITY_ID) {
                ECS::EntityHandle handle = {selectedEntity, ctx.registry};

                if (handle.has<ECS::Component::ParticleSystem>()) {
                    auto& particleSystem = handle.get<ECS::Component::ParticleSystem>();
                    const ImVec2 canvasSize = getCanvasSize();
                    const ImVec2 canvasPos = ImGui::GetCursorScreenPos();
                    const ImVec2 fullCanvasSize = ImGui::GetContentRegionAvail(); // the canvas fills the rest
                    const ImRect canvasRect(canvasPos,
                                            ImVec2(canvasPos.x + fullCanvasSize.x, canvasPos.y + fullCanvasSize.y));

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
                        m_PendingLinkPin = ed::PinId(); // belonged to the previous entity's graph
                    }

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
                    acceptVariableDrop(canvasRect, particleSystem.graphVariables, graph);
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

                    // Popup closed without picking (click outside, Escape): forget the dragged-from pin, or
                    // the next node added the normal way would connect to it. Checked after the pick above,
                    // since picking closes the popup in the same frame.
                    if (m_PendingLinkPin && !ImGui::IsPopupOpen("Add Node"))
                        m_PendingLinkPin = ed::PinId();

                    for (NodeInstance& node : graph.nodes) {
                        // Bound variable nodes: variable -> node before drawing, edits node -> variable after.
                        // Nodes later in this loop already see an edit made in an earlier one.
                        GraphVariable* variable = syncFromVariable(node, particleSystem.graphVariables);
                        const std::string title = variable ? std::string(ICON_LC_VARIABLE " ") + variable->name
                                                           : std::string();
                        std::visit([&](auto& data) { Node::drawNode(data, node.id, title); }, node.data);
                        if (variable)
                            writeBackToVariable(node, *variable);
                        // The editor owns dragging; mirror its position so the graph data stays current.
                        node.position = toVec2(ed::GetNodePosition(ed::NodeId(node.id)));
                    }

                    // After all nodes: the editor needs their pins before it can draw, connect or
                    // delete links. Each runs once per frame, not once per node.
                    Node::drawLink(graph);
                    if (const ed::PinId droppedFrom = Node::createLink(graph)) {
                        // Link released over empty canvas: offer the node menu there. The popup appears next
                        // frame; the pin is remembered until a node is picked or the popup is closed.
                        m_PendingLinkPin = droppedFrom;
                        ed::Suspend(); // popups open in screen space, like the ones above
                        ImGui::OpenPopup("Add Node");
                        ed::Resume();
                    }
                    Node::deleteSelection(graph);
                    ed::End();
                    const float nodeRounding = ed::GetStyle().NodeRounding;
                    ed::SetCurrentEditor(nullptr);
                    drawVariablesWindow(canvasPos, canvasSize, nodeRounding, particleSystem);
                }
            }
        }
        ImGui::End();
    }
}
