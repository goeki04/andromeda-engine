#include "a_GroupsOverlay.hpp"

#include <span>
#include "imgui.h"

namespace Andromeda::Gui::Overlay {

    void drawParticleGroupList(ECS::Component::ParticleSystem& system, u32 shownGroupId, u32& selectedGroupId,
                               GroupsOverlayState& state) {
        // Requested here, carried out after the loop: duplicating or removing a group edits the vector
        // the loop is walking, which would invalidate the span.
        u32 duplicateId = 0;
        u32 removeId = 0;
        const std::span<ParticleGroup> groups = system.getParticleGroups();
        const bool canRemove = groups.size() > 1; // there is always at least one group

        for (ParticleGroup& group : groups) {
            // Two groups may share a name; the ID keeps their ImGui IDs apart.
            ImGui::PushID(static_cast<int>(group.id));

            if (state.renamingGroupId == group.id) {
                // Renaming: the row becomes an input field. Enter or clicking elsewhere commits,
                // Escape lets ImGui restore the old text, which is then committed unchanged.
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                if (state.renameFocusPending) {
                    ImGui::SetKeyboardFocusHere(); // only in the first frame, or typing never ends
                    state.renameFocusPending = false;
                }
                const bool entered = ImGui::InputText("##rename", state.renameBuffer, sizeof(state.renameBuffer),
                                                      ImGuiInputTextFlags_EnterReturnsTrue |
                                                          ImGuiInputTextFlags_AutoSelectAll);
                if (entered || ImGui::IsItemDeactivated()) {
                    // An empty name would leave an unclickable row, so it is simply rejected.
                    if (state.renameBuffer[0] != '\0' && group.groupName != state.renameBuffer)
                        group.groupName = system.makeUniqueGroupName(state.renameBuffer);
                    state.renamingGroupId = 0;
                }
            } else {
                // The highlight follows the group actually shown, so a stale selection that fell back to
                // the first group highlights that one.
                if (ImGui::Selectable(group.groupName.c_str(), group.id == shownGroupId))
                    selectedGroupId = group.id;

                if (ImGui::BeginPopupContextItem("##groupMenu")) {
                    if (ImGui::MenuItem("Rename")) {
                        const size_t length = group.groupName.copy(state.renameBuffer, sizeof(state.renameBuffer) - 1);
                        state.renameBuffer[length] = '\0';
                        state.renamingGroupId = group.id;
                        state.renameFocusPending = true;
                    }
                    if (ImGui::MenuItem("Duplicate"))
                        duplicateId = group.id;
                    if (ImGui::MenuItem("Delete", nullptr, false, canRemove))
                        removeId = group.id;
                    ImGui::EndPopup();
                }
            }
            ImGui::PopID();
        }

        // Right-click on free space below the rows; NoOpenOverItems keeps it off the group rows,
        // which have their own menu.
        if (ImGui::BeginPopupContextWindow("##groupsMenu",
                                           ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
            if (ImGui::MenuItem("Add Particle Group"))
                system.addParticleGroup();
            ImGui::EndPopup();
        }

        if (duplicateId != 0) {
            if (const ParticleGroup* copy = system.duplicateParticleGroup(duplicateId))
                selectedGroupId = copy->id; // show the copy, like any editor that duplicates something
        }
        if (removeId != 0) {
            system.removeParticleGroup(removeId);
            if (state.renamingGroupId == removeId)
                state.renamingGroupId = 0;
            // The shown group is looked up by ID every frame, so a removed one falls back to the first.
        }
    }
}
