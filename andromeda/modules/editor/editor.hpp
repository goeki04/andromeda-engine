#pragma once

#include "a_ISubsystem.hpp"
#include "gui_renderer.h"
#include "scene.hpp"
#include "a_math.hpp"
#include "a_EditorContext.hpp"
#include "a_SelectionContext.hpp"
#include "a_undo_buffer.hpp"
#include "a_event_manager.hpp"

namespace Andromeda {
    class Renderer;
}

namespace Andromeda::Editor {

    /**
     * @class Editor
     * @brief The core subsystem managing the editor state, viewport camera, and scene interaction.
     * * This subsystem acts as the bridge between the underlying game engine (ECS, Renderer, SceneManager)
     * and the graphical user interface (ImGui). It handles viewport rendering logic, 3D object picking
     * via raycasting, free-flight camera movement, and the global undo/redo command buffer.
     */
    class Editor : public ISubsystem {
        Renderer* m_Renderer;
        SceneManager* m_SceneManager;
        Gui::GuiRenderer m_GuiRenderer;
        SelectionContext m_Selection;
        Gui::EditorContext m_EditorContext;
        Gui::ViewportDrawInfo m_VpDrawInfo;

        Undo::UndoBuffer m_UndoBuffer;
        EventListenerID m_UndoListenerId;
        EventListenerID m_PushUndoListenerId;
        EventListenerID m_OnSensorMsgReceivedId;
        EventListenerID m_MouseMotionListenerId;

    public:
        /**
         * @brief Default constructor for the Editor subsystem.
         */
        Editor()
            : m_Renderer(nullptr),
            m_SceneManager(nullptr)
        {
        }

        /**
         * @brief Initializes the shared editor context.
         * * Maps critical engine subsystems (Renderer, SceneManager, ResourceManager)
         * and the active OpenGL window to the EditorContext struct so that UI panels
         * can access global engine data safely.
         */
        void initEditorContext();

        /**
         * @brief Starts the editor subsystem.
         * * Overrides ISubsystem::start. Sets up the GUI renderer and registers core
         * event listeners for camera mouse motion and the undo/redo buffer system.
         */
        void start() override;

        /**
         * @brief Main per-frame update loop for the editor.
         * * Overrides ISubsystem::update. Calculates camera matrices, processes 3D object
         * picking rays, and prepares the ViewportDrawInfo before submitting the UI
         * data to the GuiRenderer.
         */
        void update() override;

        /**
         * @brief Intercepts raw SDL events for the UI backend.
         * @param event Pointer to the raw SDL_Event polled from the window manager.
         */
        void updateEvent(SDL_Event* event) override;

        /**
         * @brief Cleans up UI resources and deregisters global event listeners.
         */
        void destroy() override;

        /**
         * @brief Calculates intersection between a 3D ray and an Oriented Bounding Box (OBB).
         * * Transforms the camera's picking ray into the local space of the entity using
         * the inverse of its model matrix, then performs a standard AABB intersection test.
         * * @param cam The camera data containing the origin and direction of the ray.
         * @param aabb The Axis-Aligned Bounding Box component of the target entity.
         * @param modelMatrix The world-space transform matrix of the entity.
         * @return True if the ray intersects the bounding box; otherwise, false.
         */
        static bool RayIntersectAABB(const amath::CameraData& cam, const ECS::Component::AABB& aabb, const mat4& modelMatrix);

        /**
         * @brief Evaluates mouse clicks in the viewport to select entities.
         * * Iterates over all entities with AABB and Transform components, checking for
         * ray intersections. Dispatches an OnSceneObjectSelected event if a new entity is clicked.
         * * @param cam Pointer to the active editor camera data.
         */
        void editorPicking(const amath::CameraData* cam);

        /**
         * @brief Gets the static compile-time string identifier of the subsystem.
         * @return A string_view containing "Editor".
         */
        static constexpr std::string_view GetStaticName() { return "Editor"; }

        /**
         * @brief Gets the runtime string identifier of the subsystem.
         * @return A C-string containing the subsystem's name.
         */
        [[nodiscard]] const char* getSubsystemName() const override {
            return GetStaticName().data();
        }

        /**
         * @brief Updates the active picking ray based on the current ImGui mouse coordinates.
         */
        void updateEditorCameraRay() const;

        /**
         * @brief Processes WASD keyboard and mouse input for free-flight camera navigation.
         * * Handles relative mouse locking via SDL and calculates the forward/right vectors
         * to update the camera's position and view matrix.
         * * @param cameraData The camera state to update.
         */
        void cameraMovement(amath::CameraData& cameraData);

        /**
         * @brief Extracts the world-space position of the camera.
         * @param cam The camera state to query.
         * @return A 3D vector representing the camera's position.
         */
        [[nodiscard]] static vec3 getCameraPos(const amath::CameraData& cam);

        /**
         * @brief Extracts the view matrix of the camera.
         * @param cam The camera state to query.
         * @return The 4x4 view matrix.
         */
        [[nodiscard]] static mat4 getViewMatrix(const amath::CameraData& cam);

        /**
         * @brief Translates screen-space mouse coordinates into a viewport-local picking ray.
         * @param cam The camera data containing viewport bounds and mouse state.
         */
        static void updatePickingRay(amath::CameraData& cam);

        /**
         * @brief Adjusts the camera's Field of View (FOV) based on scroll wheel input.
         * @param cam The camera state to modify.
         * @param event The SDL scroll wheel event.
         */
        static void zoom(amath::CameraData& cam, const SDL_Event* event);

        /**
         * @brief Recalculates the perspective projection matrix based on viewport dimensions.
         * @param cam The camera state to modify.
         * @param viewPortSizeX The absolute width of the viewport image in pixels.
         * @param viewportSizeY The absolute height of the viewport image in pixels.
         */
        static void setProjectionMatrix(amath::CameraData& cam, float viewPortSizeX, float viewportSizeY);

        /**
         * @brief Retrieves the active projection matrix.
         * @param cam The camera state to query.
         * @return The 4x4 perspective projection matrix.
         */
        static mat4 getProjectionMatrix(const amath::CameraData& cam);

        /**
         * @brief Calculates a hardcoded orbital view matrix focused on the world origin.
         * @param cam The camera state to modify.
         * @return The resulting 4x4 look-at view matrix.
         */
        static mat4 calculateCameraOrbit(amath::CameraData& cam);

        /**
         * @brief Unprojects a 2D viewport cursor coordinate into a 3D world-space ray.
         * * Uses the inverse projection and inverse view matrices to map a point from
         * Normalized Device Coordinates (NDC) back into world space.
         * * @param cam The camera state containing projection data and mouse coordinates.
         * @return An amath::Ray structure containing the unprojected origin and normalized direction.
         */
        [[nodiscard]] static amath::Ray cursorToWorldRay(const amath::CameraData& cam);
    };
}