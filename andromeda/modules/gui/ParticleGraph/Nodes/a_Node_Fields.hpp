#pragma once

/**
 * @file a_Node_Fields.hpp
 * @brief The small drawing bits a node is made of: value widgets, labels and the pin dot.
 */

#include <string_view>
#include "imgui.h"
#include "a_primitives.hpp"

constexpr ImU32 PIN_ORANGE = IM_COL32(255, 152, 0, 255);

namespace Andromeda::Gui::Node {

    inline constexpr float kParamWidth = 80.0f; ///< Width of a param widget in pixels.

    // One overload per type a pin can carry; the template catches everything else and draws nothing,
    // so a node may hold plain data (a variableId, say) without a widget for it.
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

    /** @brief The dot a link attaches to. Drawn inside ed::BeginPin()/EndPin(), see NodeBuilder. */
    inline void drawPinIcon() {
        const float size = ImGui::GetFrameHeight();
        const ImVec2 pos = ImGui::GetCursorScreenPos();
        ImGui::Dummy(ImVec2(size, size));
        ImGui::GetWindowDrawList()->AddCircleFilled(ImVec2(pos.x + size * 0.5f, pos.y + size * 0.5f), size * 0.25f,
                                                    PIN_ORANGE);
    }
}
