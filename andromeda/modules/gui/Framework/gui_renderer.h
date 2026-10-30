#pragma once
#include "a_graphics_base.hpp"
#include <vector>
#include <string>
#include "a_Docking.hpp"
#include "a_EditorContext.hpp"
#include "a_registry.hpp"
#include "a_guiTypes.hpp"
#include "a_IPanelController.hpp"
#include "a_EditorPanel.hpp"
class IDeviceProvider;
class SceneManager;
namespace Andromeda::amath {
    struct CameraData;
}

namespace Andromeda::Gui {

    /**
     * @class GuiRenderer
     * @brief Owns, initializes and renders all editor panels each frame; the heart of the editor UI.
     *
     * @details Maintains a polymorphic list of @c EditorPanel instances and drives their
     *          lifecycle (init -> per-frame render). It implements @c IPanelController so that
     *          other UI elements (e.g. the menu bar) can toggle panel visibility by name. It
     *          also handles ImGui frame setup/teardown, font loading and the dockspace host
     *          window.
     */
    class GuiRenderer : public IPanelController {
    public:
        /**
         * @brief Initializes the ImGui context, style, fonts and all registered panels.
         * @param editorContext The shared editor state/dependencies passed to each panel's @c initPanel().
         */
        void init(EditorContext& editorContext);

        /**
         * @brief Renders one frame of the entire editor UI (dockspace, menu bar and all open panels).
         * @param editorContext The shared editor state forwarded to each panel's @c onGuiRender().
         */
        void update(EditorContext& editorContext);

        /**
         * @brief Finalizes the ImGui draw data and presents it, handling multi-viewport platform windows.
         * @param window The main SDL window to present to.
         */
        static void EndFrame(SDL_Window* window);

        /** @brief Shuts down ImGui and releases panel/UI resources. */
        void destroy();

    private:
        /**
         * @brief Registers the imgui.ini settings handler that persists panel visibility.
         * @details Called at the end of init(), before the first frame reads the ini.
         */
        void registerPanelSettings();

    public:

        std::vector<std::unique_ptr<EditorPanel>> m_Panels; ///< All registered editor panels, owned by the renderer.

        /** @copydoc IPanelController::isPanelOpen */
        bool isPanelOpen(std::string_view name) const override;
        /** @copydoc IPanelController::setPanelOpen */
        void setPanelOpen(std::string_view name, bool open) override;
        /** @copydoc IPanelController::getPanelNames */
        std::vector<std::string_view> getPanelNames() const override;

        /** @brief Loads and registers the editor's custom font(s) with ImGui. */
        static void loadFont();

        std::vector<SDL_Surface> m_DeviceIcons; ///< Loaded icon surfaces used for device/tool buttons.

        /**
         * @brief Constructs a panel of type @p T in place and adds it to the render list.
         * @tparam T The concrete @c EditorPanel-derived type to instantiate.
         * @tparam Args Constructor argument types forwarded to @p T.
         * @param args Arguments forwarded to the panel's constructor.
         */
        template<typename T, typename... Args>
        void registerPanel(Args&&... args) {
            m_Panels.push_back(std::make_unique<T>(std::forward<Args>(args)...));
        }

        /** @brief Opens the project folder in the host OS file explorer. */
        static void OpenFolder();

        /**
         * @brief Opens a URL in the user's default web browser.
         * @param url The fully-qualified URL to open.
         */
        static void OpenURL(const std::string& url);

        /** @brief Configures the ImGui IO config flags (docking, multi-viewport, keyboard nav, etc.). */
        static void setFlags();
    };
}