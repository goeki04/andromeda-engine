#pragma once

#include "a_Keycodes.hpp"
#include "a_meta_attributes.hpp"
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
        OnPushUndoTransform, OnSensorMessageReceived
    };

    /**
     * @brief Base interface for all events.
     * Includes a virtual destructor to ensure proper cleanup of derived event types.
     */
    struct IEvent {
        virtual ~IEvent() = default;
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

    /**
     * @brief Triggered when a raw message arrives from the sensor.
     * A_EVENT makes this event appear in the editor's event-binding dropdown.
     */
    struct A_EVENT OnSensorMessageReceived : IEvent {
        std::string message; ///< The raw message received from the sensor.
        explicit OnSensorMessageReceived(const std::string& msg) : message(msg) {}
        static constexpr EventType GetStaticType() { return EventType::OnSensorMessageReceived; }
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