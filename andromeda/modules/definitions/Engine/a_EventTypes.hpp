#pragma once

#include "a_Keycodes.hpp"
#include "a_file_event.hpp"
#include <string>
#include <any>
namespace Andromeda {

    /**
     * @brief Defines all supported event types within the Andromeda Engine.
     */
    enum class EventType {
        OnKeyDown, OnKeyUp,
        OnMouseBtnDown, OnMouseBtnUp, OnMouseWheelScroll,
        OnMouseMoved, OnSceneObjectSelected, OnEnableWireframe,
        OnPushUndoTransform, OnSensorMessageReceived,
        OnFileChanged, OnMeshReloaded
    };

    /**
     * @brief Base interface for all events.
     * Includes a virtual destructor to ensure proper cleanup of derived event types.
     */
    struct IEvent {
        virtual ~IEvent() = default;
    };

    /**
     * @brief Triggered once per watched file change, after the watcher thread handed the
     *        event to the main thread.
     *
     * @details Dispatched from OS_Manager::update(), so listeners run on the main thread
     *          and may touch the graphics context. A Rescan action means notifications
     *          were dropped and the path is the watched directory, not a single file.
     */
    struct FileChanged : IEvent {
        FileAction action;             ///< What happened to the entry.
        std::filesystem::path path;    ///< Absolute path of the affected entry.

        FileChanged(const FileAction _action, std::filesystem::path _path)
            : action(_action), path(std::move(_path)) {}
        static constexpr EventType GetStaticType() { return EventType::OnFileChanged; }
    };

    /**
     * @brief Triggered once a mesh's CPU and GPU data are current again.
     *
     * @details Sent by the ResourceManager at the end of an import, for a freshly added mesh
     *          as well as for one that was re-imported after its file changed. Listeners can
     *          assume the mesh id is valid and its GPU buffers are the new ones, so anything
     *          derived from the mesh - thumbnails, bounding volumes, colliders - is stale and
     *          has to be rebuilt.
     *
     *          Deliberately separate from FileChanged: a consumer that only cares about the
     *          result should not have to know about file paths, extensions or whether the
     *          import succeeded, and it must not race the ResourceManager for who handles the
     *          file event first.
     */
    struct MeshReloaded : IEvent {
        u32 meshID; ///< The mesh whose data was just (re)imported.

        explicit MeshReloaded(const u32 _meshID) : meshID(_meshID) {}
        static constexpr EventType GetStaticType() { return EventType::OnMeshReloaded; }
    };

    /**
     * @brief Triggered when a keyboard key is pressed.
     */
    struct KeyDown : IEvent {
        Keycode keycode; ///< The unique code of the pressed key.

        explicit KeyDown(const Keycode _code) : keycode(_code) {}
        static constexpr EventType GetStaticType() { return EventType::OnKeyDown; }
    };

    /**
     * @brief Triggered when a keyboard key is released.
     */
    struct KeyUp : IEvent {
        Keycode keycode; ///< The unique code of the released key.

        explicit KeyUp(const Keycode _code) : keycode(_code) {}
        static constexpr EventType GetStaticType() { return EventType::OnKeyUp; }
    };

    /**
     * @brief Triggered when the mouse is moved across the window.
     */
    struct MouseMoved : IEvent {
        float x, y; ///< The new absolute window coordinates of the mouse.
        float xrel, yrel; ///< Relative movement since the last frame 
        MouseMoved(const float _x, const float _y, const float _xrel, const float _yrel) : x(_x), y(_y), xrel(_xrel), yrel(_yrel) {}
        static constexpr EventType GetStaticType() { return EventType::OnMouseMoved; }
    };

    /**
     * @brief Triggered by the mouse wheel or touchpad scrolling.
     */
    struct MouseWheelScroll : IEvent {
        float mouseWheelX; ///< Horizontal scroll delta.
        float mouseWheelY; ///< Vertical scroll delta (common scroll wheel).

        MouseWheelScroll(float _x, float _y) : mouseWheelX(_x), mouseWheelY(_y) {}
        static constexpr EventType GetStaticType() { return EventType::OnMouseWheelScroll; }
    };

    /**
     * @brief Triggered when a mouse button is pressed.
     */
    struct MouseBtnDown : IEvent {
        MouseCode m_MouseCode; ///< The specific mouse button (Left, Right, Middle, etc.).

        explicit MouseBtnDown(const MouseCode _code) : m_MouseCode(_code) {}
        static constexpr EventType GetStaticType() { return EventType::OnMouseBtnDown; }
    };

    /**
     * @brief Triggered when a mouse button is released.
     */
    struct MouseBtnUp : IEvent {
        MouseCode m_MouseCode; ///< The specific mouse button that was released.

        explicit MouseBtnUp(const MouseCode _code) : m_MouseCode(_code) {}
        static constexpr EventType GetStaticType() { return EventType::OnMouseBtnUp; }
    };
    /**
     * @brief Dispatches if the user selects an object in the viewport
     */
    struct SceneObjectSelected : IEvent {
        ECS::Entity entity; ///< The ID of the selected entity.

        explicit SceneObjectSelected(ECS::Entity e) : entity(e) {}
        static constexpr EventType GetStaticType() { return EventType::OnSceneObjectSelected; }
    };

    /**
         * @brief Triggered when the wireframe rendering mode is toggled.
         */
    struct OnEnableWireFrame : IEvent {
        bool state; ///< True to enable wireframe, false for solid rendering.

        explicit OnEnableWireFrame(bool _state) : state(_state) {}
        static constexpr EventType GetStaticType() { return EventType::OnEnableWireframe; }
    };

    struct PushUndoTransformEvent : IEvent {
        ECS::Entity entity;
        std::any oldState;

        PushUndoTransformEvent(ECS::Entity e, std::any state)
            : entity(e), oldState(std::move(state)) {
        }

        static constexpr EventType GetStaticType() { return EventType::OnPushUndoTransform; }
    };

}