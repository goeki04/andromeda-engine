#pragma once
#include "a_EventTypes.hpp"
#include "a_sensor_data.hpp"
namespace Andromeda {
    struct [[BindableEvent]] OnSensorMessageReceived : IEvent {
        SensorTelemetry m_Telemetry; ///< The raw message received from the sensor.
        bool m_HasTelemetry = false; ///< Indicates whether the telemetry data is valid and can be used.
        explicit OnSensorMessageReceived(SensorTelemetry telemetry) : m_Telemetry(telemetry) {}
        static constexpr EventType GetStaticType() { return EventType::OnSensorMessageReceived; }
    };
}