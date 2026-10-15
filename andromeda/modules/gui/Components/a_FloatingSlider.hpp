#pragma once

/**
 * @file a_FloatingSlider.hpp
 * @brief A minimal slider drawn as a thin track with a round grab.
 */

#include <algorithm>

#include "imgui.h"

namespace Andromeda::Gui::Component{

    /**
     * @brief Draws a slider as a line with a circular handle and edits @p value in place.
     *
     * @details Built from an InvisibleButton plus two draw-list calls rather than a styled
     *          ImGui::SliderFloat, because the built-in slider always fills its frame and its
     *          grab is clamped to the frame height - a thin track with a larger round knob is
     *          not reachable through style alone.
     *
     *          The widget keeps no state: the caller owns the value, and the handle position
     *          is derived from it every frame.
     *
     * @param id    Unique within the window; pass distinct ids (or use PushID) inside loops,
     *              otherwise all instances share one active state.
     * @param value Edited in place, clamped to [@p min, @p max].
     * @param min   Lower end of the track.
     * @param max   Upper end of the track.
     * @return true in the frames where @p value actually changed.
     *
     * @note Returns true in every frame the drag moves the handle. For one undo entry per
     *       drag rather than per frame, remember the value when the drag starts and commit it
     *       when @c ImGui::IsItemDeactivated() goes true. (@c IsItemDeactivatedAfterEdit()
     *       does not work here: it relies on ImGui's internal edit flag, which only the
     *       built-in widgets set.)
     */
    inline bool drawFloatingSlider(const char* id, float& value, const float min, const float max, float width) {
        constexpr float circleRadius = 10.0f;
        const float height = ImGui::GetFrameHeight();
        constexpr float thickness = 3.0f;

        const ImVec2 origin = ImGui::GetCursorScreenPos();
        bool changed = false;

        ImGui::InvisibleButton(id, ImVec2(width, height));

        const float trackStart = origin.x + circleRadius;
        const float trackWidth = width - 2.0f * circleRadius;

        // A zero-width range or a track too narrow for the knob would both divide by zero
        // below. Degenerate rather than undefined: park the handle at the left end and ignore
        // input, so a mis-parameterised slider looks wrong instead of producing NaN - which
        // would spread into whatever the value feeds.
        const float range = max - min;
        const bool usable = range > 0.0f && trackWidth > 0.0f;

        float t = usable ? std::clamp((value - min) / range, 0.0f, 1.0f) : 0.0f;

        const bool isHovered = ImGui::IsItemHovered();
        const bool isActive = ImGui::IsItemActive();

        ImColor color = ImColor(80, 80, 80, 255);
        if (isHovered) {
            color = ImColor(120, 120, 120, 255);
        }
        if (isActive) {
            color = ImColor(150, 150, 150, 255);
        }

        if (isActive && usable) {
            t = std::clamp((ImGui::GetIO().MousePos.x - trackStart) / trackWidth, 0.0f, 1.0f);
            if (const float next = min + t * range; next != value) {
                value = next;
                changed = true;
            }
        }

        ImDrawList* dl = ImGui::GetWindowDrawList();
        const float centerY = origin.y + height * 0.5f;

        dl->AddRectFilled(ImVec2(origin.x, centerY - thickness * 0.5f),
                          ImVec2(origin.x + width, centerY + thickness * 0.5f), IM_COL32(255, 255, 255, 255),
                          thickness * 0.5f);
        dl->AddCircleFilled(ImVec2(trackStart + t * trackWidth, centerY), circleRadius, color);
        return changed;
    }
}
