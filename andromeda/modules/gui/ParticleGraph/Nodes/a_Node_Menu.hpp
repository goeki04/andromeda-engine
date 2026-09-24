#pragma once

/**
 * @file a_Node_Menu.hpp
 * @brief The searchable "Add Node" popup, built from the reflected node list.
 */

#include <array>
#include <string>
#include <string_view>
#include <utility>
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

    // The menu's sections. Kept here rather than in the node structs because it is a question of
    // presentation, and because the generator does not carry the [[ParticleNode]] marker's text into
    // the metadata. A node that is not listed shows up under kUncategorized, so a new node type is
    // never missing from the menu - it just lands in the last section until it is sorted in here.

    inline constexpr std::string_view kUncategorized = "Other";

    /** @brief Section order of the "Add Node" popup; kUncategorized is always drawn last. */
    inline constexpr std::array<std::string_view, 4> kNodeCategoryOrder = {"Math", "Time", "Variables", "Output"};

    /** @brief Which section a node type belongs to, by type name (not by display name). */
    inline constexpr std::array<std::pair<std::string_view, std::string_view>, 11> kNodeCategories = {{
        {"AddNode", "Math"},
        {"Subtract", "Math"},
        {"Multiply", "Math"},
        {"Divide", "Math"},
        {"Clamp", "Math"},
        {"Lerp", "Math"},
        {"Time", "Time"},
        {"OutputNode", "Output"},
        {"IntVariable", "Variables"},
        {"FloatVariable", "Variables"},
        {"BoolVariable", "Variables"},
    }};

    inline constexpr std::string_view nodeCategory(std::string_view typeName) {
        for (const auto& [node, category] : kNodeCategories) {
            if (node == typeName)
                return category;
        }
        return kUncategorized;
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
        // No plain separator here: the first section heading below already draws one.

        i32 picked = -1;
        i32 firstMatch = -1;

        // One pass per section, so the nodes appear grouped no matter how they are ordered in
        // a_Nodes.hpp. Sections whose nodes are all filtered away draw no heading at all.
        const auto drawSection = [&](std::string_view category) {
            bool headingDrawn = false;
            for (i32 i = 0; i < static_cast<i32>(Meta::ReflectedNodesNames.size()); ++i) {
                const std::string_view typeName = Meta::ReflectedNodesNames[static_cast<size_t>(i)];
                if (nodeCategory(typeName) != category)
                    continue;

                const std::string label(nodeDisplayName(typeName));
                if (!filter.PassFilter(label.c_str()))
                    continue;

                if (!headingDrawn) {
                    ImGui::SeparatorText(std::string(category).c_str());
                    headingDrawn = true;
                }
                if (firstMatch < 0)
                    firstMatch = i;

                ImGui::PushID(i);
                if (ImGui::Selectable(label.c_str()))
                    picked = i;
                ImGui::PopID();
            }
        };

        for (const std::string_view category : kNodeCategoryOrder)
            drawSection(category);
        drawSection(kUncategorized); // new node types land here until they are sorted in

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
