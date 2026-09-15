#pragma once

// ============================================================================
//  AUTO-GENERATED FILE - DO NOT EDIT BY HAND
//  Produced by metaData/gen_struct_meta.py
//  Sources:
//      modules/definitions/Engine/a_EventTypes.hpp
//      modules/data/telemetry/a_sensor_events.hpp
//  Changes to the source headers are picked up on the next build
//  (target: generate_ecs_metadata).
// ============================================================================

// FieldInfo, StructInfo and forEachField live in the hand-written core header, so that
// several generated files can coexist in one translation unit without redefining them.
#include "a_meta_core.hpp"

#include "a_EventTypes.hpp"
#include "a_sensor_events.hpp"

namespace Andromeda::Meta {


    // ------------------------------------------------------------------------
    // Andromeda::OnSensorMessageReceived (2 fields) from modules/data/telemetry/a_sensor_events.hpp
    // ------------------------------------------------------------------------
    template <>
    struct StructInfo<::Andromeda::OnSensorMessageReceived> {
        using type = ::Andromeda::OnSensorMessageReceived;

        static constexpr bool reflected = true;
        static constexpr std::string_view name = "OnSensorMessageReceived";
        static constexpr std::string_view qualifiedName = "Andromeda::OnSensorMessageReceived";
        static constexpr std::string_view header = "modules/data/telemetry/a_sensor_events.hpp";
        static constexpr std::string_view doc = "";

        static constexpr auto fields = std::make_tuple(
            makeField("m_Telemetry", "SensorTelemetry", "The raw message received from the sensor.", "", &type::m_Telemetry),
            makeField("m_HasTelemetry", "bool", "Indicates whether the telemetry data is valid and can be used.", "false", &type::m_HasTelemetry)
        );

        static constexpr std::array<std::string_view, 2> fieldNames = {"m_Telemetry", "m_HasTelemetry"};
        static constexpr std::size_t fieldCount = std::tuple_size_v<decltype(fields)>;
    };

    /** @brief Every struct this header carries metadata for. */
    using ReflectedEvents = std::tuple<
        ::Andromeda::OnSensorMessageReceived
    >;

    /** @brief The same types as ReflectedEvents, as display names in declaration order. */
    inline constexpr std::array<std::string_view, 1> ReflectedEventsNames = {"OnSensorMessageReceived"};

} // namespace Andromeda::Meta
