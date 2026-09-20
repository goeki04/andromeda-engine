#pragma once
#include "imgui.h"
#include "imgui_node_editor.h"
#include "a_particle_group.hpp"
#include "a_bindable_fields.hpp"
#include <string_view>
#include <array>
#include "a_primitives.hpp"
#include "generated_particle_group_meta.hpp"
#include "generated_node_meta.hpp"
#include "a_meta_core.hpp"

inline constexpr auto g_BindableFieldNames = Andromeda::makeBindableFieldNames<Andromeda::ParticleGroup>();
inline constexpr auto g_BindableFieldChannels = Andromeda::makeBindableFieldChannels<Andromeda::ParticleGroup>();
constexpr ImU32 PIN_ORANGE = IM_COL32(255, 152, 0, 255);

namespace Andromeda::Gui::Node {
    namespace ed = ax::NodeEditor;


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

    inline ed::PinId makePinId(u32 nodeId, u32 fieldIndex) {
        return ed::PinId((static_cast<u64>(nodeId) << 32) | (static_cast<u64>(fieldIndex) + 1));
    }

    inline void drawPinIcon() {
        const float size = ImGui::GetFrameHeight();
        const ImVec2 pos = ImGui::GetCursorScreenPos();
        ImGui::Dummy(ImVec2(size, size));
        ImGui::GetWindowDrawList()->AddCircleFilled(ImVec2(pos.x + size * 0.5f, pos.y + size * 0.5f), size * 0.25f,
                                                    PIN_ORANGE);
    }


    template<typename NodeT>
    void drawNode(NodeT& node, u32 nodeID) {
        ed::BeginNode(ed::NodeId(nodeID));
        drawLabel(Meta::StructInfo<NodeT>::name);

        ImGui::BeginGroup();
        u32 fieldIndex = 0;

        Meta::forEachField(node, [&](auto const& field, auto& member) { 
            using V = std::decay_t<decltype(field)>;
            const u32 index = fieldIndex++;
            if constexpr (pinRole<V> == PinRole::Input) {
                ed::BeginPin(makePinId(nodeID, index), ed::PinKind::Input);
                ed::PinPivotAlignment(ImVec2(0.0f, 0.5f));
                ed::PinPivotSize(ImVec2(0.0f, 0.0f));
                drawPinIcon();
                ImGui::SameLine();
                drawLabel(field.name);
                ed::EndPin();
            }
        });
        ImGui::EndGroup();
        ImGui::SameLine();

        ImGui::BeginGroup();
        Meta::forEachField(node, [&](auto const& field, auto& member) {
            using V = std::decay_t<decltype(member)>;
            if constexpr (pinRole<V> == PinRole::Param) {
                ImGui::PushID(field.name.data(), field.name.data() + field.name.size());
                drawLabel(field.name);
                ImGui::SameLine();
                ImGui::SetNextItemWidth(80.0f);
                drawField(member.value);
                ImGui::PopID();
            }
        });
        ImGui::EndGroup();
        ImGui::SameLine();

        ImGui::BeginGroup();
        fieldIndex = 0;
        Meta::forEachField(node, [&](auto const& field, auto& member) {
            using V = std::decay_t<decltype(member)>;
            const u32 index = fieldIndex++;
            if constexpr (pinRole<V> == PinRole::Output) {
                ed::BeginPin(makePinId(nodeID, index), ed::PinKind::Output);
                ed::PinPivotAlignment(ImVec2(1.0f, 0.5f));
                ed::PinPivotSize(ImVec2(0.0f, 0.0f));
                drawLabel(field.name);
                ImGui::SameLine();
                drawPinIcon();
                ed::EndPin();
            }
        });
        ImGui::EndGroup();

        ed::EndNode();
    }


}