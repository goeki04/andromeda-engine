#pragma once
#include "a_WindowContext.hpp"
#include "a_primitives.hpp"
#include <string_view>
#include <functional>
namespace Andromeda {
    struct SelectionContext;
    namespace ECS { class ComponentRegistry; }
    namespace amath { struct CameraData; }
    class ResourceManager;
    class IModelProvider;
    class SceneManager;
}

namespace Andromeda::Editor::Undo{
    class UndoBuffer;
}

namespace Andromeda::Gui
{
    struct ViewportDrawInfo;
    class IPanelController;
    /**
     * @brief A centralized dependency and state container for the Editor.
     * * This structure serves as the "Single Source of Truth" for UI panels,
     * bundling access to core engine systems, layout metrics, and interactive states.
     */
    struct EditorContext
    {
        /** @name Core Engine Systems
         * Pointers to persistent engine subsystems.
         * @{ */
        ECS::ComponentRegistry* registry;    /**< Access to entities and components. */
        ResourceManager* resourceManager;    /**< Access to textures, models, and fonts. */
        SelectionContext* selection;         /**< Current selection state across all panels. */
        SceneManager* sceneManager;          /**< Management of the active scene hierarchy. */
        IModelProvider* modelProvider;       /**< Interface for hardware-specific data. */
        amath::CameraData* cameraData;       /**< Viewport camera parameters and controls. */
        ViewportDrawInfo* viewportDrawInfo; /**< Information for rendering the viewport. */
        Andromeda::Editor::Undo::UndoBuffer* undoBuffer;       /**< Undo buffer for managing editor actions. */
        /** @} */
        IPanelController* panelController = nullptr;

        /** @brief Graphics API and OS window handles. */
        WindowContext windowContext;

        /**
         * @brief Encapsulates dynamic UI layout information.
         */
        struct
        {
            /** @brief Current size of the active rendering area in pixels. */
            vec2 viewportSize = vec2(0.0f, 0.0f);
        } layout;

        /**
         * @brief Tracks the interactive state and debug flags of the editor.
         */
        struct
        {
            /** @brief Toggle for displaying engine/version information overlay. */
            bool showVersion = false;

            /** @brief True if the last raycast hit a valid surface in the scene. */
            bool hasLastHitpoint = false;

            /** @brief The world-space coordinates of the last valid interaction point. */
            vec3 lastHitPoint = { 0.0f, 0.0f, 0.0f };

            /** @brief True if the viewport window is currently focused by the user. */
            bool viewportFocused = false;

            /** @brief True if the mouse cursor is currently hovering over the viewport area. */
            bool viewportHovered = false;

            /** @brief Index of the currently selected entity in the device browser. */
            i32 selectedIdx = -1;
        } state;

        /**
         * @brief Initializes the context with null pointers.
         * Subsystems must be assigned before passing the context to UI panels.
         */
        EditorContext()
            : registry(nullptr),
            resourceManager(nullptr),
            selection(nullptr),
            sceneManager(nullptr),
            modelProvider(nullptr),
            cameraData(nullptr),
            viewportDrawInfo(nullptr),
            undoBuffer(nullptr)
        {
        }
    };
}