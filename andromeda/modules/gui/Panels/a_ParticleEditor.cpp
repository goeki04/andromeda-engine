#include "a_ParticleEditor.hpp"
#include "imgui.h"
#include "a_SelectionContext.hpp"
#include "a_registry.hpp"
#include "a_components.hpp"
#include "a_Node_Renderer.hpp"
#include "a_Nodes.hpp"
namespace Andromeda::Gui {

    void ParticleEditor::onGuiRender(EditorContext& ctx) {
        ImGui::SetNextWindowSizeConstraints(ImVec2(200, 100), ImVec2(FLT_MAX, FLT_MAX));
        
        if (ImGui::Begin(m_Name, &m_IsOpen)) {
            ECS::Entity selectedEntity = ctx.selection->getSelectedEntity();

            if (selectedEntity != ECS::INVALID_ENTITY_ID) {
                ECS::EntityHandle handle = {selectedEntity, ctx.registry};

                if (handle.has<ECS::Component::ParticleSystem>()) {
                    auto& particleSystem = handle.get<ECS::Component::ParticleSystem>();
                    auto group = particleSystem.getParticleGroups();
                    ed::SetCurrentEditor(m_NodeEditorContext);
                    ed::Begin("Particle Graph");

                    if (ImGui::BeginPopup("Add Node")) {
                        ImGui::EndPopup();
                    }

                    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
                        ImGui::IsKeyChordPressed(ImGuiMod_Shift | ImGuiKey_A)) {
                        ImGui::OpenPopup("Add Node");
                    }

                    Node::drawNode(m_AddNode, 1);
                    ed::End();
                    ed::SetCurrentEditor(nullptr);
                }
            }
        }
        ImGui::End();
    }
}