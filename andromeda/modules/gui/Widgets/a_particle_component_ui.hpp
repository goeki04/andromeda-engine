#pragma once
#include "a_components.hpp"
#include "a_particle_group.hpp"
#include <type_traits>
#include "a_Reflector.hpp"
namespace Andromeda::Gui::Component{

   void drawAddParticleButton(ECS::Component::ParticleSystem& particleComp) {
       float s = ImGui::GetFrameHeight(); // GetFrameHeight = FontSize +style.FramePadding.y * 2
       if (ImGui::Button("+", ImVec2(s, s))) {
           particleComp.addParticleGroup();
       }
   }

   void drawParticleGroupProperties(std::span<Andromeda::ParticleGroup> group, u32 index) {
       ImGui::Text("Count");
       ImGui::SameLine(100);
       ImGui::DragInt("##ParticleCount", &group[index].particleCount);

       ImGui::Text("Size");
       ImGui::SameLine(100);
       ImGui::DragFloat("##Size", &group[index].size);

       ImGui::Text("Min lifetime");
       ImGui::SameLine(100);
       ImGui::DragFloat("##Minlifetime", &group[index].minLifeTime);

       ImGui::Text("Velocity");
       ImGui::SameLine(100);
       ImGui::DragFloat3("##Velocity", &group[index].velocity.x);

       ImGui::Text("Color");
       ImGui::SameLine(100);
       ImGui::DragFloat3("##ParticleColor", &group[index].particleColor.x);
   }
    
   void drawParticleGroups(ECS::Component::ParticleSystem& particleComp) {
        drawAddParticleButton(particleComp);

        i32 indexToRemove = -1;
        if (ImGui::BeginChild("ParticleGroups", ImVec2(0, 120),ImGuiChildFlags_ResizeY)) {
            auto groupes = particleComp.getParticleGroups();
            u32 groupCount = static_cast<u32>(groupes.size());
            for (u32 i = 0; i < groupCount; ++i) {
                ImGui::PushID(static_cast<int>(i));
                bool groupOpen = true;
                const auto& groupLabel = groupes[i].groupName;

                bool headerExpanded = ImGui::CollapsingHeader(groupLabel.c_str(), &groupOpen, ImGuiTreeNodeFlags_DefaultOpen);

                if (!groupOpen && groupCount > 1) {
                    indexToRemove = static_cast<int>(i);
                }

                if (ImGui::BeginPopupContextItem()) {

                    static char nameBuffer[64] = "";
                    if (ImGui::IsWindowAppearing()) {
                        strncpy(nameBuffer, groupes[i].groupName.c_str(), sizeof(nameBuffer) - 1);
                        nameBuffer[sizeof(nameBuffer) - 1] = '\0';
                    }
                    ImGui::TextDisabled("Rename Group:");
                    if (ImGui::InputText("##NameInput", nameBuffer, sizeof(nameBuffer),
                                         ImGuiInputTextFlags_EnterReturnsTrue)) {
                        groupes[i].groupName = nameBuffer;
                        ImGui::CloseCurrentPopup();
                    }

                    if (groupCount > 1) {
                        bool canDelete = groupCount > 1;
                        if (ImGui::MenuItem("Delete", "", false, canDelete)) {
                            indexToRemove = static_cast<int>(i);
                        }
                    }
                    ImGui::EndPopup();
                }

                if (headerExpanded) {
                    drawParticleGroupProperties(groupes, i);
                }

                ImGui::PopID();
            }
            particleComp.removeParticleGroup(indexToRemove);
        }
        ImGui::EndChild();
   }


   /** @brief Draws the event binding window for the particle system.
        Event bindings let users connect particle system properties to custom callbacks or events. */
   void drawEventBindingWindow() {
       if (ImGui::CollapsingHeader("Event Bindings")) {
           float s = ImGui::GetFrameHeight();
           if (ImGui::Button("+", ImVec2(s,s))) {

           }
           if (ImGui::BeginChild("EventBindings", ImVec2(0, 150),ImGuiChildFlags_ResizeY)) {
                
           }
       }
   }
}