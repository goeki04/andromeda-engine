#pragma once
#include "a_components.hpp"
#include "a_particle_group.hpp"
#include <type_traits>
#include <string>
#include <vector>
#include "generated_particle_group_meta.hpp"
#include "generated_event_meta.hpp"
#include "a_particle_group.hpp"
#include "IconsLucide.h"
#include "a_Dropdown_Button.hpp"
#include "a_logger.hpp"
 namespace Andromeda::Gui::Component{

    template<typename T>
    constexpr std::size_t countBindableFields() {
        std::size_t n = 0;
        Andromeda::Meta::forEachField<T>([&](auto const& f) {
            using V = typename std::decay_t<decltype(f)>::member_type;
            if constexpr (ChannelTraits<V>::count > 0)
                ++n;
        });
        return n;
    }

    template<typename T>
    constexpr auto makeBindableFieldNames() {
        std::array<std::string_view, countBindableFields<T>()> out{};
        std::size_t i = 0;
        Andromeda::Meta::forEachField<T>([&](auto const& f) {
            using V = typename std::decay_t<decltype(f)>::member_type;
            if constexpr (ChannelTraits<V>::count > 0)
                out[i++] = f.name;
        });
        return out;
    }

    template<typename T>
    constexpr auto makeBindableFieldChannels() {
        std::array<u32, countBindableFields<T>()> out{};
        std::size_t i = 0;
        Andromeda::Meta::forEachField<T>([&](auto const& f) {
            using V = typename std::decay_t<decltype(f)>::member_type;
            if constexpr (ChannelTraits<V>::count > 0) {
                out[i++] = ChannelTraits<V>::count;
            }
        });
        return out;
    }

   inline constexpr auto g_BindableFieldNames = makeBindableFieldNames<Andromeda::ParticleGroup>();
   inline constexpr auto g_BindableFieldChannels = makeBindableFieldChannels<Andromeda::ParticleGroup>();
   inline void drawAddParticleButton(ECS::Component::ParticleSystem& particleComp) {
       float s = ImGui::GetFrameHeight(); // GetFrameHeight = FontSize +style.FramePadding.y * 2
       if (ImGui::Button("+", ImVec2(s, s))) {
           particleComp.addParticleGroup();
       }
   }

   /**
    * @brief Event names offered by the binding dropdown.
    * Generated from the events marked [[BindableEvent]] in a_EventTypes.hpp - to add an event to the
    * dropdown, annotate it there; nothing here needs to change.
    */
   inline constexpr auto& g_EventNames = Andromeda::Meta::ReflectedEventsNames;

   inline void parseEventString(const std::string& message) {
       A_INFO("Received sensor message: {}", message);
   }
    template<typename T>
        requires std::is_arithmetic_v<T> 
   inline void parseEvent() {

   }

   /**
    * @brief Draws one event -> field binding row.
    * @param binding The binding to edit; it owns the selection, the widget is stateless.
    * @return true when the row's delete button was pressed. The row must not erase itself:
    *         the caller is still iterating over the vector, so the erase happens after the loop.
    */
   inline bool drawBindingList(EventBinding& binding) {
       bool removeRequested = false;
       if (ImGui::BeginTable("EventBindingTable", 4)) {
           ImGui::TableSetupColumn("Edit", ImGuiTableColumnFlags_WidthFixed);
           ImGui::TableSetupColumn("Event", ImGuiTableColumnFlags_WidthStretch);
           ImGui::TableSetupColumn("Field", ImGuiTableColumnFlags_WidthStretch);
           ImGui::TableSetupColumn("Delete", ImGuiTableColumnFlags_WidthFixed);
           ImGui::TableNextRow();
           ImGui::TableNextColumn();
           ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
           ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
           if (ImGui::Button(ICON_LC_EDIT_2 "##editBinding")) {
           }
           ImGui::PopStyleColor(2);
           ImGui::TableNextColumn();

           drawDropdownButton("event", g_EventNames, binding.eventIndex, "Select event...");

           ImGui::TableNextColumn();
           drawDropdownButton("field", g_BindableFieldNames, binding.fieldIndex, "Select field...");

          
           u32 fieldChannels = g_BindableFieldChannels[binding.fieldIndex];


           ImGui::TableNextColumn();
           ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
           ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
           if (ImGui::Button(ICON_LC_MINUS "##removeBinding")) {
               removeRequested = true;
           }
           ImGui::PopStyleColor(2);

           ImGui::EndTable();
       }
       return removeRequested;
   }

    /** @brief Draws the event binding window for the particle system.
    Event bindings let users connect particle system properties to custom callbacks or events. */
   inline void drawEventBindingWindow(Andromeda::ParticleGroup& group) {
       ImGui::PushID("EventBindings");
       if (ImGui::TreeNode("Event Bindings")) {

           const i32 size = static_cast<i32>(group.eventBindings.size());
           i32 indexToRemove = -1;
           for (i32 i = 0; i < size; ++i) {
               ImGui::PushID(i);
               if (drawBindingList(group.eventBindings[i])) {
                   indexToRemove = i;
               }
               if (i < size - 1) {
                   ImGui::Separator();
               }
               ImGui::PopID();
           }

           if (indexToRemove >= 0) {
               group.eventBindings.erase(group.eventBindings.begin() + static_cast<ptrdiff_t>(indexToRemove));
           }

           if (ImGui::Button("+ Binding")) {
               group.eventBindings.emplace_back();
           }
           ImGui::TreePop();
       }
       ImGui::PopID();
   }

   inline void drawParticleGroupProperties(std::span<Andromeda::ParticleGroup> group, u32 index) {
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
    
   inline void drawParticleGroups(ECS::Component::ParticleSystem& particleComp) {
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