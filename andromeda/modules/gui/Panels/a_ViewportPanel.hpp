#pragma once

#include "a_EditorPanel.hpp"
#include "imgui.h"
#include "ImGuizmo.h"
#include "a_registry.hpp"
#include <array>
#include <optional>
#include <any>

/**
 * @namespace Andromeda::Gui
 * @brief Contains all editor panels, overlay tools, and UI rendering logic.
 */
namespace Andromeda::Gui {

    struct EditorContext;
    struct ViewportDrawInfo; // Assuming this is forward-declared or included via EditorContext

    /**
     * @struct TransformIcons
     * @brief Manages the texture handles and logic mappings for the viewport's transform toolbar.
     */
    struct TransformIcons {
        /** @brief Defines the available transformation tools. */
        enum Type { Translate, Scale, Rotate, Select, Count };

        /** @brief Cached OpenGL texture IDs for the tool icons, ready for ImGui. */
        std::array<ImTextureID, Count> handles;

        /** @brief Internal string identifiers for the tools, used for UI IDs. */
        static constexpr const char* toolNames[Count] = {
            "translate", "scale", "rotate", "select"
        };

        /**
         * @brief Maps the internal tool type to the corresponding ImGuizmo operation.
         * @param tool The selected tool type.
         * @return An optional ImGuizmo::OPERATION. Returns std::nullopt if the "Select" tool is active (no gizmo).
         */
        static constexpr std::optional<ImGuizmo::OPERATION> getImGuizmoTool(const Type tool) {
            switch (tool) {
            case Translate: return ImGuizmo::TRANSLATE;
            case Scale:     return ImGuizmo::SCALE;
            case Rotate:    return ImGuizmo::ROTATE;
            default:
                return std::nullopt;
            }
        }
    };

    /**
     * @struct ViewportDimension
     * @brief Represents the spatial boundaries and size of the viewport image on the screen.
     */
    struct ViewportDimension {
        ImVec2 min;  /**< Top-left screen coordinate of the viewport. */
        ImVec2 max;  /**< Bottom-right screen coordinate of the viewport. */
        ImVec2 size; /**< Absolute width and height of the viewport. */
    };

    /**
     * @class ViewportPanel
     * @brief The primary 3D rendering window of the editor.
     * * Handles the display of the engine's framebuffer, camera input redirection,
     * entity picking ray calculations, and 3D object manipulation via ImGuizmo.
     */
    class ViewportPanel : public EditorPanel {
    public:
        /** @brief The currently selected manipulation tool (defaults to Select). */
        TransformIcons::Type m_ActiveTool = TransformIcons::Select;

        /**
         * @brief Constructs the ViewportPanel.
         * @param name The title of the ImGui window.
         */
        explicit ViewportPanel(const char* name)
            : EditorPanel(name), m_SelectedEntity({ ECS::INVALID_ENTITY_ID, nullptr }) {
        }

        /**
         * @brief Initializes event listeners and caches required UI texture icons.
         * @param ctx The global EditorContext.
         */
        void initPanel(EditorContext& ctx) override;

        /**
         * @brief Main rendering loop for the viewport window.
         * @param ctx The global EditorContext.
         */
        void onGuiRender(EditorContext& ctx) override;

        /**
         * @brief Renders the ImGuizmo manipulation handles for the currently selected entity.
         * @param ctx The global EditorContext.
         */
        void updateGizmos(EditorContext& ctx);

        /**
         * @brief Configures ImGuizmo drawing boundaries to match the viewport image.
         * @note Uses @c m_ImageRect, measured in drawViewportImage(), which must run first.
         */
        void prepareImGuizmo();

        /**
         * @brief Processes user interaction with the active gizmo and records undo states.
         * @param transform The transform component of the manipulated entity.
         * @param modelMatrix The model matrix after ImGuizmo has manipulated it.
         */
        void handleGizmoInteraction(ECS::Component::Transform& transform, const mat4& modelMatrix);

        /**
         * @brief Writes the manipulated matrix back into the transform component.
         * @details Only the field the active tool actually drives is written. Feeding all
         *          three back would round-trip the rotation through Euler angles every
         *          frame, which drifts and made the gizmo jump mid-drag.
         * @param transform The transform component to apply the values to.
         * @param matrix The final 4x4 matrix produced by ImGuizmo.
         */
        void applyGizmoTransform(ECS::Component::Transform& transform, const mat4& matrix) const;

        /**
         * @brief Finalizes a gizmo drag operation and dispatches an undo event if changes occurred.
         * @param currentTransform The final transform state after the mouse is released.
         */
        void finalizeGizmoInteraction(ECS::Component::Transform& currentTransform);

        /** @brief Pushes transparent, borderless ImGui styles for the viewport overlay tools. */
        static void setOverlayStyle();

        /** * @brief Generates standard window flags for non-intrusive UI overlays.
         * @return A bitmask of ImGuiWindowFlags.
         */
        static ImGuiWindowFlags setOverlayFlags();

        /** @brief Restores standard ImGui styles after overlay rendering is complete. */
        static void resetOverlayStyle();

        /**
         * @brief Renders the pill-shaped toolbar containing translation, rotation, and scale toggles.
         * @param textureHandles The cached collection of tool icon textures.
         * @param flags Window flags for the overlay.
         */
        void drawTransformButtons(const TransformIcons& textureHandles, ImGuiWindowFlags flags);

        /**
         * @brief Renders the wireframe toggle button.
         * @param textureID The cached texture ID for the wireframe icon.
         */
        void drawWireframeControl(const u32& textureID);

        /**
         * @brief Master function that composes and draws the entire 2D overlay on top of the 3D viewport.
         * @param textureID The texture ID for the standalone wireframe button.
         * @param textureHandles The texture collection for the transform tools.
         */
        void drawViewportOverlay(const u32& textureID, const TransformIcons& textureHandles);

        /**
         * @brief Maps screen-space mouse coordinates to viewport-local coordinates for raycasting.
         * @param vpDimension The physical boundaries of the viewport image.
         * @param ctx The global EditorContext.
         * @param drawInfo Contextual rendering data including the camera.
         */
        static void updateImGuiMousePos(const ViewportDimension& vpDimension, EditorContext& ctx, const ViewportDrawInfo& drawInfo);

        /**
         * @brief Renders the final OpenGL framebuffer texture into the ImGui window.
         * @param ctx The global EditorContext.
         * @param drawInfo Render state information containing the framebuffer ID.
         */
        void drawViewportImage(EditorContext& ctx, const ViewportDrawInfo& drawInfo);

        /**
         * @brief Evaluates mouse state to toggle camera rotation logic vs. UI interaction.
         * @param drawInfo Render state information containing the camera.
         */
        static void handleViewportInput(const ViewportDrawInfo& drawInfo);

    private:
        ECS::EntityHandle m_SelectedEntity;      /**< Handle to the currently selected entity in the scene. */
        TransformIcons m_TextureHandles = {};    /**< Cached UI icons for the transform toolbar. */
        bool m_WireframeEnabled = false;         /**< Tracks the current rendering mode (Solid vs. Wireframe). */
        bool m_IsDraggingGizmo = false;          /**< Flag indicating if the user is actively dragging a gizmo axis. */
        ViewportDimension m_ImageRect = {};      /**< Screen rect of the viewport image, measured after it is drawn. ImGuizmo maps the mouse through it, so it must be the real rect, not the framebuffer size. */
        std::any m_ActiveUndoState;              /**< Stores a snapshot of the Transform before a gizmo drag started. */
    };
}