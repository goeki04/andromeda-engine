#pragma once

/**
 * @file a_Node_Menu.hpp
 * @brief The searchable "Add Node" popup, built from the reflected node list.
 */

#include <string>
#include <string_view>
#include "imgui.h"
#include "IconsLucide.h"
#include "a_primitives.hpp"
#include "generated_node_meta.hpp"

namespace Andromeda::Gui::Node {

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
}
