#pragma once

/**
 * @file a_FloatingSlider.hpp
 * @brief A minimal slider drawn as a thin track with a round grab.
 */

#include <algorithm>

#include "imgui.h"
#include "a_Style.hpp"

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
        const float height = ImGui::GetFrameHeight();

        // Sized off the row height rather than fixed: a 20px knob next to a 3px line read as a
        // marble on a thread, and it stayed that size when the editor's font did not.
        const float circleRadius = height * 0.3f;
        constexpr float thickness = 4.0f;

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

        if (isActive && usable) {
            t = std::clamp((ImGui::GetIO().MousePos.x - trackStart) / trackWidth, 0.0f, 1.0f);

            // Compared against the old value instead of reporting every held frame: a drag
            // that pauses mid-way would otherwise keep firing, and anything listening - undo,
            // a network sync, a shader rebuild - would do the same work over and over.
            if (const float next = min + t * range; next != value) {
                value = next;
                changed = true;
            }
        }

        // Taken from the style rather than hard-coded, so this widget follows the editor theme
        // the way every built-in control does - the white track and grey knob it used to draw
        // were the only two colours in the editor that appeared nowhere else.
        const ImU32 trackColor = ImGui::GetColorU32(ImGuiCol_FrameBg);
        const ImU32 fillColor = ImGui::GetColorU32(isHovered || isActive ? AccentColorBright : AccentColor);
        const ImU32 knobColor = ImGui::GetColorU32(isActive ? ImGuiCol_SliderGrabActive : ImGuiCol_SliderGrab);

        ImDrawList* dl = ImGui::GetWindowDrawList();
        const float centerY = origin.y + height * 0.5f;
        const float knobX = trackStart + t * trackWidth;

        dl->AddRectFilled(ImVec2(origin.x, centerY - thickness * 0.5f),
                          ImVec2(origin.x + width, centerY + thickness * 0.5f), trackColor,
                          thickness * 0.5f);

        // The filled part is what makes the value readable at a glance; without it the track
        // says nothing and the knob has to be hunted for.
        if (knobX > origin.x) {
            dl->AddRectFilled(ImVec2(origin.x, centerY - thickness * 0.5f),
                              ImVec2(knobX, centerY + thickness * 0.5f), fillColor,
                              thickness * 0.5f);
        }

        dl->AddCircleFilled(ImVec2(knobX, centerY), circleRadius, knobColor);
        return changed;
    }
}
