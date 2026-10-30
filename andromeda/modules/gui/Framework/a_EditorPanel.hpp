#pragma once
#include "a_guiTypes.hpp"
#include <string_view>
struct ImGuiTextBuffer;
namespace Andromeda::Gui
{
    struct EditorContext;
    /**
         * @brief Abstract base class for all editor UI components.
         * * This interface defines the lifecycle of a UI panel. By inheriting from
         * EditorPanel, any component can be managed and rendered by the GuiRenderer
         * without the renderer needing to know the panel's specific implementation.
         */
    class EditorPanel {

    public:
        /**
         * @brief Virtual destructor to ensure proper cleanup of derived classes.
         */
        virtual ~EditorPanel() = default;

        /**
         * @brief Pure virtual function to handle ImGui rendering logic.
         * * This must be implemented by every panel. It contains the
         * ImGui::Begin() and ImGui::End() calls and all widgets in between.
         */
        virtual void onGuiRender(EditorContext& ctx) = 0;

        /**
         * @brief Optional one-time initialization hook, called before the first render.
         * @details Override to cache resources (icons, textures) or subscribe to events.
         *          The base implementation does nothing.
         * @param ctx The shared editor context.
         */
        virtual void initPanel(EditorContext& ctx) {}

        virtual void writeSettings(ImGuiTextBuffer& buffer) const {}

        virtual void readSettings(const char* line) {}

        /** @brief Returns the panel's display name (also used as the ImGui window title). */
        const char* getName() const { return m_Name; }

        /**
         * @brief Constructs a panel with the given window title.
         * @param name The display name of the panel; must outlive the panel (typically a string literal).
         */
        explicit EditorPanel(const char* name) : m_Name(name){}

        bool m_IsOpen = true; ///< Controls the visibility of the panel.
        EditorWindowFlags m_WindowFlags = 0; ///< Extra ImGui window flags applied when this panel begins.
    protected:
        const char* m_Name = nullptr; ///< The title of the ImGui window.
    };

}
