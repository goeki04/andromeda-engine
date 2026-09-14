#pragma once

/**
 * @file a_Dropdown_Button.hpp
 * @brief Custom dropdown button with a search filter, drawn by hand (rounded frame + marker).
 */

#include <algorithm>
#include <cctype>
#include <cstring>
#include <string>
#include <vector>
#include <span>
#include "imgui.h"
#include "imgui_internal.h"

namespace Andromeda::Gui::Component
{
    /**
     * @brief Draws a dropdown button that opens a searchable option list.
     *
     * @details The widget is stateless: the caller owns @p selectedIndex, so the selection lives
     *          as long as the data it belongs to and two dropdowns never share a selection.
     *
     * @param id            Unique label within the current ID stack.
     * @param options       The selectable entries; constant across frames, not owned.
     * @param selectedIndex In/out index into @p options, or -1 for "nothing selected".
     * @param placeholder   Text drawn on the button while @p selectedIndex is -1.
     * @return true on the frame an entry was picked.
     */
    inline bool drawDropdownButton(const char* id, std::span < const std::string_view> options,
                                   int& selectedIndex,
                                   const char* placeholder = "Select...")
    {
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if (window->SkipItems)
            return false;

        ImGuiContext& g = *GImGui;
        const ImGuiID widgetId = window->GetID(id);

        const float width    = ImGui::GetContentRegionAvail().x;
        const float height   = ImGui::GetFrameHeight();
        const float rounding = 10.0f;

        const ImVec2 pos = window->DC.CursorPos;
        const ImRect bb(pos, ImVec2(pos.x + width, pos.y + height));

        ImGui::ItemSize(bb, g.Style.FramePadding.y);
        if (!ImGui::ItemAdd(bb, widgetId))
            return false;

        bool hovered = false;
        bool held    = false;
        const bool pressed = ImGui::ButtonBehavior(bb, widgetId, &hovered, &held, ImGuiButtonFlags_None);

        const std::string popupId = std::string("##DropdownPopup") + id;

        // Only one popup can be open at a time, so a single buffer is enough - but it must be
        // cleared on open, otherwise the previous dropdown's query is still in it.
        static char searchQuery[64] = "";
        if (pressed) {
            searchQuery[0] = '\0';
            ImGui::OpenPopup(popupId.c_str());
        }

        ImGuiCol fill = ImGuiCol_Button;
        if (ImGui::IsPopupOpen(popupId.c_str()))
            fill = ImGuiCol_ButtonActive;
        else if (hovered)
            fill = ImGuiCol_ButtonHovered;

        bool changed = false;

        // Fixed width (0 height = auto): popups are AlwaysAutoResize, so without this the popup
        // grows to fit its content and ends up wider than the button.

        float popupWidth = width;
        for (const std::string_view option : options)
            popupWidth = std::max(popupWidth, ImGui::CalcTextSize(option.data(), option.data() + option.size()).x);
        popupWidth += g.Style.WindowPadding.x * 2.0f + g.Style.FramePadding.x * 2.0f;

        ImGui::SetNextWindowPos(ImVec2(bb.Min.x, bb.Max.y + g.Style.ItemSpacing.y));
        ImGui::SetNextWindowSize(ImVec2(popupWidth, 0.0f));
        if (ImGui::BeginPopup(popupId.c_str())) {
            ImGui::PushItemWidth(-FLT_MIN);
            ImGui::InputTextWithHint("##search", "Search...", searchQuery, IM_ARRAYSIZE(searchQuery));
            ImGui::PopItemWidth();

            const size_t queryLength = std::strlen(searchQuery);
            for (int i = 0; i < static_cast<int>(options.size()); ++i) {
                const std::string_view option = options[i];

                if (queryLength != 0) {
                    const auto match = std::search(option.begin(), option.end(),
                                                   searchQuery, searchQuery + queryLength,
                                                   [](char a, char b) {
                                                       return std::tolower(static_cast<unsigned char>(a)) ==
                                                              std::tolower(static_cast<unsigned char>(b));
                                                   });
                    if (match == option.end())
                        continue;
                }

                if (ImGui::Selectable(option.data(), i == selectedIndex)) {
                    selectedIndex = i;
                    changed = true;
                    ImGui::CloseCurrentPopup();
                }
            }
            ImGui::EndPopup();
        }

        window->DrawList->AddRectFilled(bb.Min, bb.Max, ImGui::GetColorU32(fill), rounding);
        window->DrawList->AddRect(bb.Min, bb.Max, ImGui::GetColorU32(ImGuiCol_Border), rounding, 0, 1.0f);

        // Marker radii are derived from the font size so they follow the DPI/style scale, and
        // num_segments stays 0 so ImGui tessellates against style.CircleTessellationMaxError.
        // A hard-coded segment count is what made these render as visible polygons.
        const float markerOuter = g.FontSize * 0.40f;
        const float markerInner = g.FontSize * 0.28f;
        const ImVec2 markerCenter(bb.Max.x - g.Style.FramePadding.x - markerOuter,
                                  bb.Min.y + height * 0.5f);
        window->DrawList->AddCircleFilled(markerCenter, markerInner, IM_COL32_WHITE, 0);
        window->DrawList->AddCircle(markerCenter, markerOuter, IM_COL32_WHITE, 0, 1.0f);

        const char* label;
        if (selectedIndex >= 0 && selectedIndex < static_cast<int>(options.size())) {
            label = options[selectedIndex].data();
        } else {
            label = placeholder;
        }
        const ImVec2 textMin(bb.Min.x + g.Style.FramePadding.x, bb.Min.y);
        const ImVec2 textMax(markerCenter.x - markerOuter - g.Style.ItemInnerSpacing.x, bb.Max.y);
        const float available = textMax.x - textMin.x;

        const ImVec2 labelSize = ImGui::CalcTextSize(label);
        const char* remaining = nullptr;
        g.Font->CalcTextSizeA(g.FontSize, available, 0.0f, label, nullptr, &remaining);
        const bool truncated = (remaining && *remaining != '\0');

        if (!truncated) {
            ImGui::RenderTextClipped(textMin, textMax, label, nullptr, &labelSize, ImVec2(0.0f, 0.5f), &bb);
        } else {
            static constexpr char kEllipsis[] = "...";
            const float ellipsisWidth = ImGui::CalcTextSize(kEllipsis).x;

            const char* cut = nullptr;
            g.Font->CalcTextSizeA(g.FontSize, ImMax(available - ellipsisWidth, 0.0f), 0.0f, label, nullptr, &cut);
            if (!cut)
                cut = label;

            char buffer[128];
            const int keep = ImMin(static_cast<int>(cut - label),
                                   static_cast<int>(sizeof(buffer)) - static_cast<int>(sizeof(kEllipsis)));
            std::memcpy(buffer, label, static_cast<size_t>(keep));
            std::memcpy(buffer + keep, kEllipsis, sizeof(kEllipsis));

            ImGui::RenderTextClipped(textMin, textMax, buffer, nullptr, nullptr, ImVec2(0.0f, 0.5f), &bb);
        }


        if (truncated && hovered)
            ImGui::SetTooltip("%s", label);

        return changed;
    }
}
