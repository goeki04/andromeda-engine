#pragma once

/**
 * @file a_GraphOverlay.hpp
 * @brief The floating panels that sit on top of the node canvas: how they are opened, moved and clamped.
 */

#include <algorithm>
#include "imgui.h"
#include "imgui_internal.h" // ImRect

namespace Andromeda::Gui::Overlay {

    constexpr float kOverlayMargin = 5.0f; ///< Gap between the canvas corner and an overlay.
    constexpr float kBarPadding = 4.0f;    ///< Extra space above and below the widgets in a menu bar.

    /**
     * @brief Opens an overlay child at canvas.Min + @p offset whose title can be dragged to move it.
     * @details A child window has no title bar of its own, so the header is a SeparatorText with an
     *          invisible button laid over it. While that button is held, the mouse movement is added to
     *          @p offset. The offset is relative to the canvas corner, so the overlay stays in place when
     *          the panel moves, and it is clamped so the overlay never leaves the canvas.
     * @param offset Position relative to the canvas corner; updated while dragging.
     * @return The result of BeginChild. Call ImGui::EndChild() either way.
     */
    inline bool beginMovableOverlay(const char* id, const char* title, const ImRect& canvas,
                                    const ImVec2& defaultSize, float rounding, ImVec2& offset) {
        ImGui::SetCursorScreenPos(ImVec2(canvas.Min.x + offset.x, canvas.Min.y + offset.y));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, rounding);
        const bool open = ImGui::BeginChild(
            id, defaultSize, ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX | ImGuiChildFlags_ResizeY);
        ImGui::PopStyleVar();

        if (open) {
            const ImVec2 headerPos = ImGui::GetCursorScreenPos();
            ImGui::SeparatorText(title);
            const float headerHeight = ImGui::GetItemRectSize().y;

            // Same position and height as the title, so the cursor ends up where the title left it.
            // InvisibleButton asserts on a zero size, hence the 1 px minimum for a squeezed overlay.
            ImGui::SetCursorScreenPos(headerPos);
            ImGui::InvisibleButton("##move", ImVec2(std::max(1.0f, ImGui::GetContentRegionAvail().x),
                                                    std::max(1.0f, headerHeight)));
            if (ImGui::IsItemHovered() || ImGui::IsItemActive())
                ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
            if (ImGui::IsItemActive()) {
                const ImVec2 delta = ImGui::GetIO().MouseDelta;
                offset.x += delta.x;
                offset.y += delta.y;
                // ImGui writes imgui.ini only when something is marked dirty; our offsets are not its own data.
                if (delta.x != 0.0f || delta.y != 0.0f)
                    ImGui::MarkIniSettingsDirty();
            }
        }

        // Clamped every frame, not only while dragging: shrinking the panel pushes the overlay back inside.
        const ImVec2 size = ImGui::GetWindowSize();
        offset.x = std::clamp(offset.x, 0.0f, std::max(0.0f, canvas.GetWidth() - size.x));
        offset.y = std::clamp(offset.y, 0.0f, std::max(0.0f, canvas.GetHeight() - size.y));
        return open;
    }
}
