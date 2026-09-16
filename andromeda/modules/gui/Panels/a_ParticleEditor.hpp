#pragma once

#include "a_EditorPanel.hpp"
namespace Andromeda::Gui {
    class ParticleEditor : public EditorPanel {
    
        public:
        explicit ParticleEditor(const char* name) : EditorPanel(name) { m_IsOpen = false; }

        /** @copydoc EditorPanel::onGuiRender */
        void onGuiRender(EditorContext& ctx) override;

    };
} 