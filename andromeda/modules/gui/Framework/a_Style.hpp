#pragma once

/**
 * @file a_Style.hpp
 * @brief Applies the Andromeda editor's custom ImGui visual theme (dark, low-contrast).
 */

#include "imgui.h"
// SDL is used by setStyle() for the DPI scale. Included here so this header is
// self-contained - it previously relied on gui_renderer.cpp including SDL first.
#include <SDL3/SDL_video.h>
namespace Andromeda::Gui{
    /**
     * @brief Configures the global ImGui style: rounding, spacing and the engine's dark color palette.
     *
     * @details Should be called once after the ImGui context is created. Sizes are scaled by the
     *          primary display's content scale (DPI awareness) via @c ScaleAllSizes(), and an
     *          accent blue (~RGB 0, 0.45, 0.84) is used for active/selected widget states.
     */
    /**
     * @brief The editor's accent color - single source of truth.
     *
     * @details Every "active / selected / on" state in the editor must derive from this,
     *          including hand-drawn widgets that bypass ImGui's style system
     *          (see BrowserPanel tile selection). Do not re-type the literal elsewhere.
     */
    inline constexpr ImVec4 AccentColor{0.00f, 0.45f, 0.84f, 1.00f};

    /** @brief Brighter accent variant, used for the "pressed" step of accented widgets. */
    inline constexpr ImVec4 AccentColorBright{0.00f, 0.55f, 0.95f, 1.00f};

    /**
     * @brief Background of a selected row in a list (Hierarchy, and any future list panel).
     *
     * @details Deliberately neutral rather than accented: ImGui shares ImGuiCol_Header between
     *          selected Selectables and CollapsingHeader/TreeNode bars, and the editor's section
     *          bars must stay grey. Panels push this locally for selected rows only.
     *          At 0.34 it sits ~1.9:1 against an unselected row, well above the ~1.26:1 of the
     *          plain hover step, so selection stays readable once the mouse moves away.
     */
    inline constexpr ImVec4 SelectionBg{0.34f, 0.34f, 0.34f, 1.00f};

    /** @brief Selected row that is additionally hovered. */
    inline constexpr ImVec4 SelectionBgHovered{0.40f, 0.40f, 0.40f, 1.00f};

    /** @brief Returns @p color with its alpha replaced by @p alpha. */
    inline constexpr ImVec4 withAlpha(const ImVec4& color, float alpha) {
        return ImVec4(color.x, color.y, color.z, alpha);
    }

    inline void setStyle() {
        ImGuiStyle& style = ImGui::GetStyle();

        style.WindowRounding = 2.0f;
        style.ChildRounding = 2.0f;
        style.FrameRounding = 2.0f;
        style.PopupRounding = 2.0f;
        style.ScrollbarRounding = 2.0f;
        style.TabRounding = 2.0f;
        style.WindowBorderSize = 1.0f;
        style.FrameBorderSize = 1.0f;
        style.ItemSpacing = ImVec2(8.0f, 4.0f);
        style.FramePadding = ImVec2(6.0f, 4.0f);

        // Default 0.30f: at small radii that error budget is visible as flat facets.
        style.CircleTessellationMaxError = 0.10f;

        // ScaleAllSizes() must run AFTER every size above, otherwise these literals overwrite
        // the scaled values and the editor stays at 1x on a high-DPI display.
        float mainScale = SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay());
        if (mainScale <= 0.0f) {
            mainScale = 1.0f; // SDL returns 0.0f on error.
        }
        style.ScaleAllSizes(mainScale);
        style.FontScaleDpi = mainScale; // imgui 1.92: rasterize the font at the DPI, don't upscale it.

        ImVec4* colors = style.Colors;
        colors[ImGuiCol_WindowBg] = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
        colors[ImGuiCol_ChildBg] = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
        colors[ImGuiCol_PopupBg] = ImVec4(0.08f, 0.08f, 0.08f, 0.94f);

        // 0.32 rather than 0.20: with FrameBorderSize/WindowBorderSize enabled, 0.20 sat at
        // 1.08:1 against the button fill - the borders were drawn but effectively invisible.
        colors[ImGuiCol_Border] = ImVec4(0.32f, 0.32f, 0.32f, 1.00f);
        colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);

        colors[ImGuiCol_MenuBarBg] = ImVec4(0.08f, 0.08f, 0.08f, 1.00f);
        colors[ImGuiCol_TitleBg] = ImVec4(0.08f, 0.08f, 0.08f, 1.00f);
        colors[ImGuiCol_TitleBgActive] = ImVec4(0.08f, 0.08f, 0.08f, 1.00f);
        colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.08f, 0.08f, 0.08f, 1.00f);

        colors[ImGuiCol_FrameBg] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
        colors[ImGuiCol_FrameBgHovered] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
        colors[ImGuiCol_FrameBgActive] = ImVec4(0.28f, 0.28f, 0.28f, 1.00f);

        colors[ImGuiCol_Tab] = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
        colors[ImGuiCol_TabHovered] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
        colors[ImGuiCol_TabActive] = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
        colors[ImGuiCol_TabUnfocused] = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
        colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);

        // The pressed step continues the grey ramp (0.18 -> 0.24 -> 0.30) rather than flashing
        // the accent: the accent marks what IS selected, not what is momentarily under the mouse.
        colors[ImGuiCol_Button] = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
        colors[ImGuiCol_ButtonHovered] = ImVec4(0.24f, 0.24f, 0.24f, 1.00f);
        colors[ImGuiCol_ButtonActive] = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);

        // Header stays neutral grey on purpose, including its active step: ImGui uses this slot
        // for CollapsingHeader and TreeNode bars, and those must not pick up an accent tint.
        // List selection therefore does NOT ride on the global Header color - panels push
        // SelectionBg locally instead (see HierarchyPanel::drawNormalSelectable).
        colors[ImGuiCol_Header] = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
        colors[ImGuiCol_HeaderHovered] = ImVec4(0.24f, 0.24f, 0.24f, 1.00f);
        colors[ImGuiCol_HeaderActive] = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);

        colors[ImGuiCol_ScrollbarBg] = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
        colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
        colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.35f, 0.35f, 0.35f, 1.00f);
        colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.45f, 0.45f, 0.45f, 1.00f);

        colors[ImGuiCol_CheckMark] = AccentColor;
        colors[ImGuiCol_SliderGrab] = AccentColor;
        colors[ImGuiCol_SliderGrabActive] = AccentColorBright;

        colors[ImGuiCol_Text] = ImVec4(0.90f, 0.90f, 0.90f, 1.00f);
        colors[ImGuiCol_TextDisabled] = ImVec4(0.40f, 0.40f, 0.40f, 1.00f);
    }
}