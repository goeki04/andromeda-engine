#pragma once
#include "a_components.hpp"
#include "a_particle_group.hpp"
#include <type_traits>
#include "generated_particle_group_meta.hpp"
#include "a_particle_group.hpp"
namespace Andromeda::Gui::Component{

   void drawAddParticleButton(ECS::Component::ParticleSystem& particleComp) {
       float s = ImGui::GetFrameHeight(); // GetFrameHeight = FontSize +style.FramePadding.y * 2
       if (ImGui::Button("+", ImVec2(s, s))) {
           particleComp.addParticleGroup();
       }
   }

   void drawBindingList(std::vector<std::string>& eventNames, std::vector<std::string>& particleFields) {
       if (ImGui::Button("Event")) {
           ImGui::OpenPopup("EventBindingPopup");
       }
       ImGui::SameLine();
       if (ImGui::Button("Field")) {
           ImGui::OpenPopup("FieldBindingPopup");
       }

       if (ImGui::BeginPopup("EventBindingPopup")) {
           static char searchQuery[64] = "";
           ImGui::InputTextWithHint("##searchEvent", "Search event...", searchQuery, IM_ARRAYSIZE(searchQuery));
           for (auto const& e : eventNames) {
                   auto it = std::search(e.begin(), e.end(), searchQuery, searchQuery + strlen(searchQuery),
                                         [](char a, char b) { return std::tolower(a) == std::tolower(b); });
                   if (it != e.end() || searchQuery[0] == '\0') {
                       if (ImGui::Selectable(e.c_str())) {
                           ImGui::CloseCurrentPopup();
                       }
                   }
           }
           ImGui::EndPopup();
       }
       if (ImGui::BeginPopup("FieldBindingPopup")) {
           static char searchQuery[64] = "";
           ImGui::InputTextWithHint("##searchParticleField", "Search property...", searchQuery, IM_ARRAYSIZE(searchQuery));
           for (auto const& p : particleFields) {
               if (ImGui::Selectable(p.c_str())) {
                   ImGui::CloseCurrentPopup();
               }
           }
           ImGui::EndPopup();
       }
   }

         /** @brief Draws the event binding window for the particle system.
Event bindings let users connect particle system properties to custom callbacks or events. */
   void drawEventBindingWindow(Andromeda::ParticleGroup& group) {
       ImGui::PushID("EventBindings");
       float s = ImGui::GetFrameHeight();
       if (ImGui::TreeNode("Event Bindings")) {
           ImGui::Indent();
           Andromeda::Meta::forEachField(group, [&](auto const& f, auto& value) {
               using V = std::decay_t<decltype(value)>;
               constexpr u32 channelCount = ChannelTraits<V>::count;
           });
           std::vector<std::string> eventNames = {"OnStart", "OnUpdate", "OnEnd"};
           std::vector<std::string> particleFields = {"Position", "Velocity", "Color", "Size", "Lifetime"};
           drawBindingList(eventNames, particleFields);
           ImGui::TreePop();
       }
       ImGui::Unindent();
       ImGui::PopID();
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

       drawEventBindingWindow(group[index]);
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
}