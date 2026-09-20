#pragma once
#include "imgui_node_editor.h"
#include "a_EditorPanel.hpp"
#include "a_EditorContext.hpp"
#include "a_particle_group.hpp"
#include <string>
#include "a_Primitives.hpp"
#include "a_Nodes.hpp"
namespace Andromeda::Gui {
    namespace ed = ax::NodeEditor;

    class ParticleEditor : public EditorPanel {
    
        public:
        ed::EditorContext* m_NodeEditorContext = nullptr;
        explicit ParticleEditor(const char* name) : EditorPanel(name) { 
            m_IsOpen = false; 

            ed::Config config;
            config.CustomZoomLevels.push_back(0.25f);
            config.CustomZoomLevels.push_back(0.5f);
            config.CustomZoomLevels.push_back(0.75f);
            config.CustomZoomLevels.push_back(1.0f);
            config.SettingsFile = "ParticleEditor.json";
            config.NavigateButtonIndex = 2; // mouse button index for navigation (0 = left, 1 = right, 2 = middle)
            m_NodeEditorContext = ed::CreateEditor(&config);
        }

        /** @copydoc EditorPanel::onGuiRender */
        void onGuiRender(EditorContext& ctx) override;

        ~ParticleEditor() {
            if (m_NodeEditorContext) {
                ed::DestroyEditor(m_NodeEditorContext);
                m_NodeEditorContext = nullptr;
            }
        }

        private:
        Node::AddNode m_AddNode;
    };
} 