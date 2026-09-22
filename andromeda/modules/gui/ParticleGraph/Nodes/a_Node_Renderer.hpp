#pragma once
#include "imgui.h"
#include "imgui_node_editor.h"
#include "a_particle_group.hpp"
#include "a_bindable_fields.hpp"
#include <algorithm>
#include <string_view>
#include <type_traits>
#include <array>
#include "a_primitives.hpp"
#include "generated_particle_group_meta.hpp"
#include "generated_node_meta.hpp"
#include "a_meta_core.hpp"
#include "IconsLucide.h"

inline constexpr auto g_BindableFieldNames = Andromeda::makeBindableFieldNames<Andromeda::ParticleGroup>();
inline constexpr auto g_BindableFieldChannels = Andromeda::makeBindableFieldChannels<Andromeda::ParticleGroup>();
constexpr ImU32 PIN_ORANGE = IM_COL32(255, 152, 0, 255);

namespace Andromeda::Gui::Node {
    namespace ed = ax::NodeEditor;

    inline constexpr float kParamWidth = 80.0f; ///< Width of a param widget in pixels.

    inline void drawField(float& v) {
        ImGui::DragFloat("##v", &v, 0.01f);
    }
    inline void drawField(i32& v) {
        ImGui::DragInt("##v", &v);
    }
    inline void drawField(bool& v) {
        ImGui::Checkbox("##v", &v);
    }
    inline void drawField(vec2& v) {
        ImGui::DragFloat2("##v", &v.x, 0.01f);
    }
    inline void drawField(vec3& v) {
        ImGui::DragFloat3("##v", &v.x, 0.01f);
    }
    inline void drawField(vec4& v) {
        ImGui::DragFloat4("##v", &v.x, 0.01f);
    }
    template<typename T>
    inline void drawField(T&) {}

    inline void drawLabel(std::string_view text) {
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(text.data(), text.data() + text.size());
    }

    struct PinInfo {
        u32 nodeId;
        u32 fieldIndex;
    };

    inline PinInfo getPinInfo(ed::PinId pinId) {
        u64 id = pinId.Get();
        u32 nodeId = static_cast<u32>(id >> 32);
        u32 fieldIndex = static_cast<u32>(id & 0xFFFFFFFF) - 1;
        return PinInfo{nodeId, fieldIndex};
    }
    
    inline NodeInstance* getNodeByPinId(ed::PinId pinId, ParticleGraph &graph) {
        PinInfo info = getPinInfo(pinId);
        auto it = std::find_if(graph.nodes.begin(), graph.nodes.end(), [info](const NodeInstance& node) {
            return node.id == info.nodeId;
        });
        if (it != graph.nodes.end()) {
            return &*it;
        }
        return nullptr;
    }

    inline ValueType getPinValueType(ParticleGraph& graph, ed::PinId pinId) {
        auto node = getNodeByPinId(pinId, graph);
        if (node == nullptr) {
            return ValueType::None;
        }
        const u32 searchedIndex = getPinInfo(pinId).fieldIndex;
        ValueType type = ValueType::None;
        std::visit([&](auto& data) {
                u32 index = 0;
                Meta::forEachField(data, [&](auto const&, auto& member) {
                    if (index == searchedIndex) {
                        using PinT = std::decay_t<decltype(member)>; // e.x. Input<float>, Output<vec3>, Param<i32>
                        using valueT = typename PinTraits<PinT>::value_type; // just the inner type, e.x. float, vec3, i32
                        type = valueTypeOf<valueT>;
                    }
                    ++index;
                });
            }, node->data);
        return type;
    }

    inline bool canConnect(ValueType from, ValueType to) {
        if (from == ValueType::None || to == ValueType::None) {
            return false;
        }

        if (from == ValueType::Int && to == ValueType::Float) {
            return true;
        }
        return from == to;
    }

    inline PinRole getPinRole(ParticleGraph& graph, ed::PinId pinId) {
        auto node = getNodeByPinId(pinId, graph);
        if (node == nullptr) {
            return PinRole::None;
        }
        const u32 searchedIndex = getPinInfo(pinId).fieldIndex;
        PinRole role = PinRole::None;
        std::visit([&](auto& data) {
                u32 index = 0;
                Meta::forEachField(data, [&](auto const&, auto& member) {
                    if (index == searchedIndex) {
                        role = pinRole<std::decay_t<decltype(member)>>;
                    }
                    ++index;
                });
            }, node->data);
        return role;
    }

    inline ed::PinId makePinId(u32 nodeId, u32 fieldIndex) {
        return ed::PinId((static_cast<u64>(nodeId) << 32) | (static_cast<u64>(fieldIndex) + 1));
    }
    
    inline void drawLink(ParticleGraph& graph) {
        for (const auto& link : graph.links) {
            ed::Link(ed::LinkId(link.id),ed::PinId(link.sourceId),ed::PinId(link.targetId));
        }
    }

    /** @brief "AddNode" -> "Add": the menu shows node names without the redundant suffix. */
    inline std::string_view nodeDisplayName(std::string_view typeName) {
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
    inline i32 drawAddNodePopup(ImGuiTextFilter& filter, float rounding, ImVec2& openedAt) {
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
     * @brief Stores a link from output @p source to input @p target.
     * @details An input has at most one link, so an existing link into @p target is replaced - which is
     *          what lets the user re-plug an input by simply dragging a new link onto it.
     */
    inline void addLink(ParticleGraph& graph, ed::PinId source, ed::PinId target) {
        std::erase_if(graph.links, [target](const PinLink& link) { return link.targetId == target.Get(); });
        graph.links.emplace_back(PinLink{graph.nextLinkId, source.Get(), target.Get()});
        graph.nextLinkId++;
    }

    /**
     * @brief Handles dragging links: connects pins, and reports a link dropped on empty canvas.
     * @return The pin a link was dragged from and released over empty space - the caller opens the
     *         "Add Node" popup and connects the new node to it. An invalid PinId otherwise.
     * @note Call between ed::Begin() and ed::End(), after the nodes were drawn.
     */
    inline ed::PinId createLink(ParticleGraph& graph) {
        ed::PinId droppedFrom;

        if (ed::BeginCreate()) {
            ed::PinId source;
            ed::PinId target;

            if (ed::QueryNewLink(&source, &target)) {
                PinRole sourceRole = getPinRole(graph, source);
                PinRole targetRole = getPinRole(graph, target);
                bool isSameNode = getPinInfo(source).nodeId == getPinInfo(target).nodeId;

                if (sourceRole == PinRole::Input && targetRole == PinRole::Output) {
                    std::swap(source, target);
                    std::swap(sourceRole, targetRole);
                }

                bool hasValidRoles = sourceRole == PinRole::Output && targetRole == PinRole::Input;
                bool hasValidTypes = canConnect(getPinValueType(graph, source), getPinValueType(graph, target));
                if (hasValidRoles && hasValidTypes && !isSameNode) {
                    if (ed::AcceptNewItem())
                        addLink(graph, source, target);
                } else {
                    ed::RejectNewItem();
                }
            }

            // Same drag, but released over empty canvas instead of over a pin.
            ed::PinId pin;
            if (ed::QueryNewNode(&pin)) {
                if (ed::AcceptNewItem())
                    droppedFrom = pin;
            }
        }
        ed::EndCreate();

        return droppedFrom;
    }

    /**
     * @brief Field index of the first pin with @p role in @p node, or -1 if it has none.
     * @details Same field walk as getPinRole, searching by role instead of by index.
     */
    inline i32 findFirstPin(ValueType other,const NodeInstance& node, PinRole role) {
        i32 found = -1;
        std::visit([&](const auto& data) {
            i32 index = 0;
            Meta::forEachField(data, [&](auto const&, const auto& member) {
                using PinT = std::decay_t<decltype(member)>;
                using ValueT = typename PinTraits<PinT>::value_type;
                const ValueType memberType = valueTypeOf<ValueT>;
                bool hasWantedRole = pinRole<std::decay_t<decltype(member)>> == role;
                bool hasMatchingType =
                    role == PinRole::Input
                        ? canConnect(other, memberType)
                        : canConnect(memberType, other);
                if (found < 0 && hasWantedRole && hasMatchingType)
                    found = index;
                ++index;
            });
        }, node.data);
        return found;
    }

    /**
     * @brief Links @p fromPin to the first matching pin of @p newNode: an output to its first input,
     *        an input to its first output. Does nothing if the node has no such pin or @p fromPin's
     *        node was deleted in the meantime.
     */
    inline void connectToNewNode(ParticleGraph& graph, ed::PinId fromPin, const NodeInstance& newNode) {
        const PinRole fromRole = getPinRole(graph, fromPin);
        if (fromRole != PinRole::Input && fromRole != PinRole::Output)
            return;

        const PinRole wantedRole = fromRole == PinRole::Output ? PinRole::Input : PinRole::Output;
        const i32 fieldIndex = findFirstPin(getPinValueType(graph, fromPin), newNode, wantedRole);
        if (fieldIndex < 0)
            return;

        const ed::PinId newPin = makePinId(newNode.id, static_cast<u32>(fieldIndex));
        if (fromRole == PinRole::Output)
            addLink(graph, fromPin, newPin);
        else
            addLink(graph, newPin, fromPin);
    }

    /** @brief Removes a node and every link attached to it from the graph data. */
    inline void removeNode(ParticleGraph& graph, u32 nodeId) {
        std::erase_if(graph.nodes, [nodeId](const NodeInstance& node) { return node.id == nodeId; });

        // The editor reports a deleted node's links as well, but only the ones it drew. The graph data
        // is what gets saved and evaluated, so it must never keep a link to a node that is gone.
        std::erase_if(graph.links, [nodeId](const PinLink& link) {
            return getPinInfo(ed::PinId(link.sourceId)).nodeId == nodeId ||
                   getPinInfo(ed::PinId(link.targetId)).nodeId == nodeId;
        });
    }

    /**
     * @brief Handles the editor's delete action (Delete key on a selection) for nodes and links.
     * @details Nodes are queried first: accepting a node makes the editor append that node's links to
     *          the items still to delete, so the link loop afterwards reports them too. Querying links
     *          first would miss them.
     * @note Call between ed::Begin() and ed::End(), after the nodes and links were drawn.
     */
    inline void deleteSelection(ParticleGraph& graph) {
        if (ed::BeginDelete()) {
            ed::NodeId deletedNodeId;
            while (ed::QueryDeletedNode(&deletedNodeId)) {
                if (ed::AcceptDeletedItem())
                    removeNode(graph, static_cast<u32>(deletedNodeId.Get()));
            }

            ed::LinkId deletedLinkId;
            while (ed::QueryDeletedLink(&deletedLinkId)) {
                if (ed::AcceptDeletedItem()) {
                    const u32 linkId = static_cast<u32>(deletedLinkId.Get());
                    std::erase_if(graph.links, [linkId](const PinLink& link) { return link.id == linkId; });
                }
            }
        }
        ed::EndDelete();
    }

    inline void drawPinIcon() {
        const float size = ImGui::GetFrameHeight();
        const ImVec2 pos = ImGui::GetCursorScreenPos();
        ImGui::Dummy(ImVec2(size, size));
        ImGui::GetWindowDrawList()->AddCircleFilled(ImVec2(pos.x + size * 0.5f, pos.y + size * 0.5f), size * 0.25f,
                                                    PIN_ORANGE);
    }

    /**
     * @brief Lays out the fields of one node in rows with three aligned columns:
     *        inputs left, params in the middle, outputs right (right-aligned).
     *
     * @details A node can describe its own layout with an overload found next to the node struct:
     * @code
     * inline void drawLayout(RemapNode& n, auto& b) {
     *     b.row(n.value, n.result); // the role of each field decides its column
     *     b.separator();
     *     b.row(n.inMin);
     * }
     * @endcode
     *          Without such an overload, defaultLayout() pairs the n-th input, param and output
     *          into row n.
     *
     *          Column widths are measured every frame and applied on the next one, the usual ImGui
     *          way of aligning content whose size is only known after drawing it. They live in the
     *          window's ImGuiStorage under the node's ID, so the builder itself is a throwaway object.
     *          On the very first frame the columns therefore overlap once and settle immediately.
     *
     * @tparam NodeT A reflected node struct (StructInfo<NodeT> must exist).
     */
    template<typename NodeT>
    class NodeBuilder {
    public:
        /** @brief Must be constructed inside the node, after its ImGui ID has been pushed. */
        NodeBuilder(NodeT& node, u32 nodeId) : m_Node(node), m_NodeId(nodeId), m_Origin(ImGui::GetCursorScreenPos()) {
            ImGuiStorage* storage = ImGui::GetStateStorage();
            m_LeftWidth = storage->GetFloat(ImGui::GetID("##leftWidth"));
            m_CenterWidth = storage->GetFloat(ImGui::GetID("##centerWidth"));
            m_RightWidth = storage->GetFloat(ImGui::GetID("##rightWidth"));
        }

        /**
         * @brief Draws one row. Pass fields of the node; each lands in the column of its role,
         *        several fields of the same role sit side by side.
         */
        template<typename... Fields>
        void row(Fields&... fields) {
            static_assert(((pinRole<std::decay_t<Fields>> != PinRole::None) && ...),
                          "NodeBuilder::row() only takes Input<>, Param<> or Output<> fields");

            constexpr bool hasLeft = ((pinRole<std::decay_t<Fields>> == PinRole::Input) || ...);
            constexpr bool hasCenter = ((pinRole<std::decay_t<Fields>> == PinRole::Param) || ...);
            constexpr bool hasRight = ((pinRole<std::decay_t<Fields>> == PinRole::Output) || ...);

            layoutRow(hasLeft, hasCenter, hasRight,
                      [&] { (drawIfRole<PinRole::Input>(fields), ...); },
                      [&] { (drawIfRole<PinRole::Param>(fields), ...); },
                      [&] { (drawIfRole<PinRole::Output>(fields), ...); });
        }

        /** @brief A horizontal line across the node. ImGui::Separator() would span the whole canvas. */
        void separator() {
            const ImGuiStyle& style = ImGui::GetStyle();
            const float height = style.ItemSpacing.y;
            const ImVec2 pos(m_Origin.x, ImGui::GetCursorScreenPos().y);
            const float width = std::max(contentWidth(), ImGui::GetFrameHeight());

            ImGui::SetCursorScreenPos(pos);
            ImGui::Dummy(ImVec2(width, height));
            ImGui::GetWindowDrawList()->AddLine(ImVec2(pos.x, pos.y + height * 0.5f),
                                                ImVec2(pos.x + width, pos.y + height * 0.5f),
                                                ImGui::GetColorU32(ImGuiCol_Separator));
        }

        /** @brief Empty vertical space of one item spacing. */
        void spacing() {
            ImGui::Dummy(ImVec2(0.0f, ImGui::GetStyle().ItemSpacing.y));
        }

        /** @brief Layout used when the node has no drawLayout(): row n = n-th input, param and output. */
        void defaultLayout() {
            constexpr u32 inputs = countRole<PinRole::Input>();
            constexpr u32 params = countRole<PinRole::Param>();
            constexpr u32 outputs = countRole<PinRole::Output>();
            constexpr u32 rows = std::max({inputs, params, outputs});

            for (u32 k = 0; k < rows; ++k) {
                layoutRow(k < inputs, k < params, k < outputs,
                          [&] { drawNth<PinRole::Input>(k); },
                          [&] { drawNth<PinRole::Param>(k); },
                          [&] { drawNth<PinRole::Output>(k); });
            }
        }

        /** @brief Stores this frame's column widths for the next frame. Call once, after the layout. */
        void finish() {
            ImGuiStorage* storage = ImGui::GetStateStorage();
            storage->SetFloat(ImGui::GetID("##leftWidth"), m_NextLeftWidth);
            storage->SetFloat(ImGui::GetID("##centerWidth"), m_NextCenterWidth);
            storage->SetFloat(ImGui::GetID("##rightWidth"), m_NextRightWidth);
        }

    private:
        /** @brief Number of fields of NodeT with role R, known at compile time. */
        template<PinRole R>
        static constexpr u32 countRole() {
            u32 count = 0;
            Meta::forEachField<NodeT>([&](auto const& field) {
                using V = typename std::decay_t<decltype(field)>::member_type;
                if constexpr (pinRole<V> == R)
                    ++count;
            });
            return count;
        }

        /** @brief Gap between two non-empty columns. */
        static float columnGap() {
            return ImGui::GetStyle().ItemSpacing.x * 3.0f;
        }

        float centerX() const {
            return m_Origin.x + (m_LeftWidth > 0.0f ? m_LeftWidth + columnGap() : 0.0f);
        }

        float rightX() const {
            const float x = centerX();
            return x + (m_CenterWidth > 0.0f ? m_CenterWidth + columnGap() : 0.0f);
        }

        /** @brief Width of all columns together, as measured last frame. */
        float contentWidth() const {
            return std::max({m_LeftWidth, centerX() - m_Origin.x + m_CenterWidth, rightX() - m_Origin.x + m_RightWidth});
        }

        /**
         * @brief Places the three columns of one row. Each draw callback is only called when its
         *        column has content, so an empty column never widens the node.
         */
        template<typename DrawLeft, typename DrawCenter, typename DrawRight>
        void layoutRow(bool hasLeft, bool hasCenter, bool hasRight, DrawLeft&& drawLeft, DrawCenter&& drawCenter,
                       DrawRight&& drawRight) {
            const float rowY = ImGui::GetCursorScreenPos().y;
            float rowHeight = 0.0f;

            ImGui::PushID(static_cast<int>(m_RowIndex++));

            if (hasLeft)
                drawColumn(ImVec2(m_Origin.x, rowY), drawLeft, m_NextLeftWidth, rowHeight);
            if (hasCenter)
                drawColumn(ImVec2(centerX(), rowY), drawCenter, m_NextCenterWidth, rowHeight);
            if (hasRight) {
                ImGuiStorage* storage = ImGui::GetStateStorage();
                const ImGuiID rowWidthKey = ImGui::GetID("##rowRightWidth");
                const float lastRowWidth = storage->GetFloat(rowWidthKey);
                const float x = rightX() + std::max(0.0f, m_RightWidth - lastRowWidth);
                const float width = drawColumn(ImVec2(x, rowY), drawRight, m_NextRightWidth, rowHeight);
                storage->SetFloat(rowWidthKey, width);
            }

            ImGui::PopID();

            // The columns were placed with SetCursorScreenPos; finish the row with a real item so
            // the cursor moves to the next line and ImGui does not flag the manual cursor moves.
            ImGui::SetCursorScreenPos(ImVec2(m_Origin.x, rowY));
            ImGui::Dummy(ImVec2(0.0f, rowHeight));
        }

        /** @brief Draws one column as a group at @p pos and returns its width. */
        template<typename Draw>
        float drawColumn(ImVec2 pos, Draw&& draw, float& maxWidth, float& rowHeight) {
            ImGui::SetCursorScreenPos(pos);
            ImGui::BeginGroup();
            m_ColumnHasItem = false;
            draw();
            ImGui::EndGroup();

            const ImVec2 size = ImGui::GetItemRectSize();
            maxWidth = std::max(maxWidth, size.x);
            rowHeight = std::max(rowHeight, size.y);
            return size.x;
        }

        /** @brief Draws @p member if its role is R; used by row() to fill one column. */
        template<PinRole R, typename V>
        void drawIfRole(V& member) {
            if constexpr (pinRole<V> == R) {
                std::string_view name;
                u32 index = 0;
                if (findField(member, name, index))
                    drawSlot(member, name, index);
            }
        }

        /** @brief Draws the k-th field with role R; used by defaultLayout(). */
        template<PinRole R>
        void drawNth(u32 k) {
            u32 fieldIndex = 0;
            u32 roleIndex = 0;
            Meta::forEachField(m_Node, [&](auto const& field, auto& member) {
                using V = std::decay_t<decltype(member)>;
                const u32 index = fieldIndex++;
                if constexpr (pinRole<V> == R) {
                    if (roleIndex++ == k)
                        drawSlot(member, field.name, index);
                }
            });
        }

        /**
         * @brief Finds the field @p member refers to by comparing addresses.
         * @return false if @p member is not a field of this node (e.g. a field of another node).
         */
        template<typename V>
        bool findField(const V& member, std::string_view& name, u32& index) const {
            bool found = false;
            u32 fieldIndex = 0;
            Meta::forEachField(m_Node, [&](auto const& field, auto& candidate) {
                if (!found && static_cast<const void*>(&candidate) == static_cast<const void*>(&member)) {
                    name = field.name;
                    index = fieldIndex;
                    found = true;
                }
                ++fieldIndex;
            });
            IM_ASSERT(found && "NodeBuilder: field does not belong to this node");
            return found;
        }

        /** @brief Draws one field according to its role: input pin, param widget or output pin. */
        template<typename V>
        void drawSlot(V& member, std::string_view name, u32 fieldIndex) {
            if (m_ColumnHasItem)
                ImGui::SameLine();
            m_ColumnHasItem = true;

            if constexpr (pinRole<V> == PinRole::Input) {
                ed::BeginPin(makePinId(m_NodeId, fieldIndex), ed::PinKind::Input);
                ed::PinPivotAlignment(ImVec2(0.0f, 0.5f));
                ed::PinPivotSize(ImVec2(0.0f, 0.0f));
                drawPinIcon();
                ed::EndPin();
                ImGui::SameLine();
                drawLabel(name);
            } else if constexpr (pinRole<V> == PinRole::Param) {
                ImGui::PushID(name.data(), name.data() + name.size());
                drawLabel(name);
                ImGui::SameLine();
                ImGui::SetNextItemWidth(kParamWidth);
                drawField(member.value);
                ImGui::PopID();
            } else if constexpr (pinRole<V> == PinRole::Output) {
                drawLabel(name);
                ImGui::SameLine();
                ed::BeginPin(makePinId(m_NodeId, fieldIndex), ed::PinKind::Output);
                ed::PinPivotAlignment(ImVec2(1.0f, 0.5f));
                ed::PinPivotSize(ImVec2(0.0f, 0.0f));
                drawPinIcon();
                ed::EndPin();
            }
        }

        NodeT& m_Node;
        u32 m_NodeId;
        ImVec2 m_Origin;           ///< Top-left of the node content, below the title.
        u32 m_RowIndex = 0;        ///< Gives every row its own ImGui ID scope.
        bool m_ColumnHasItem = false;

        float m_LeftWidth = 0.0f;  ///< Column widths measured last frame, used for placement now.
        float m_CenterWidth = 0.0f;
        float m_RightWidth = 0.0f;

        float m_NextLeftWidth = 0.0f; ///< Column widths measured this frame, stored by finish().
        float m_NextCenterWidth = 0.0f;
        float m_NextRightWidth = 0.0f;
    };

    /**
     * @brief Draws any reflected node. Uses the node's drawLayout() overload if one exists,
     *        otherwise NodeBuilder::defaultLayout().
     * @param title Header text; empty uses the node type's name (e.g. a bound variable node shows the variable).
     */
    template<typename NodeT>
    void drawNode(NodeT& node, u32 nodeID, std::string_view title = {}) {
        static_assert(Meta::isReflected<NodeT>,
                      "No StructInfo for this node - is generated_node_meta.hpp included and the generator run?");

        ed::BeginNode(ed::NodeId(nodeID));
        ImGui::PushID(static_cast<int>(nodeID));

        drawLabel(title.empty() ? Meta::StructInfo<NodeT>::name : title);

        NodeBuilder<NodeT> builder(node, nodeID);
        if constexpr (requires { drawLayout(node, builder); })
            drawLayout(node, builder);
        else
            builder.defaultLayout();
        builder.finish();

        ImGui::PopID();
        ed::EndNode();
    }
}
