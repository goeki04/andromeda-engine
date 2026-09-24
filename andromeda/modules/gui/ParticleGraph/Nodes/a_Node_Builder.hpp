#pragma once

/**
 * @file a_Node_Builder.hpp
 * @brief Laying out and drawing one node: three aligned columns built from its reflected fields.
 */

#include <algorithm>
#include <string_view>
#include <type_traits>
#include "imgui.h"
#include "imgui_node_editor.h"
#include "a_Node_Fields.hpp"
#include "a_Node_PinQuery.hpp"
#include "a_meta_core.hpp"
#include "generated_node_meta.hpp"

namespace Andromeda::Gui::Node {

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
