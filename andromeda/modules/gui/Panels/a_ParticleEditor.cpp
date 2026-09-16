#include "a_ParticleEditor.hpp"
#include "imgui.h"
namespace Andromeda::Gui{
	
    void ParticleEditor::onGuiRender(EditorContext& ctx) {
        ImGui::SetNextWindowSizeConstraints(ImVec2(200, 100), ImVec2(FLT_MAX, FLT_MAX));
        if (ImGui::Begin(m_Name, &m_IsOpen)) {
        }
        ImGui::End();
    }

}