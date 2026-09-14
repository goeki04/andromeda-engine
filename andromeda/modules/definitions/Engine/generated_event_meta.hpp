#pragma once

// ============================================================================
//  AUTO-GENERATED FILE - DO NOT EDIT BY HAND
//  Produced by metaData/gen_struct_meta.py
//  Sources:
//      modules/definitions/Engine/a_EventTypes.hpp
//  Changes to the source headers are picked up on the next build
//  (target: generate_ecs_metadata).
// ============================================================================

// FieldInfo, StructInfo and forEachField live in the hand-written core header, so that
// several generated files can coexist in one translation unit without redefining them.
#include "a_meta_core.hpp"

#include "a_EventTypes.hpp"

namespace Andromeda::Meta {


    // ------------------------------------------------------------------------
    // Andromeda::OnSensorMessageReceived (1 fields) from modules/definitions/Engine/a_EventTypes.hpp
    // ------------------------------------------------------------------------
    template <>
    struct StructInfo<::Andromeda::OnSensorMessageReceived> {
        using type = ::Andromeda::OnSensorMessageReceived;

        static constexpr bool reflected = true;
        static constexpr std::string_view name = "OnSensorMessageReceived";
        static constexpr std::string_view qualifiedName = "Andromeda::OnSensorMessageReceived";
        static constexpr std::string_view header = "modules/definitions/Engine/a_EventTypes.hpp";
        static constexpr std::string_view doc = "";

        static constexpr auto fields = std::make_tuple(
            makeField("message", "std::string", "The raw message received from the sensor.", "", &type::message)
        );

        static constexpr std::array<std::string_view, 1> fieldNames = {"message"};
        static constexpr std::size_t fieldCount = std::tuple_size_v<decltype(fields)>;
    };

    /** @brief Every struct this header carries metadata for. */
    using ReflectedEvents = std::tuple<
        ::Andromeda::OnSensorMessageReceived
    >;

    /** @brief The same types as ReflectedEvents, as display names in declaration order. */
    inline constexpr std::array<std::string_view, 1> ReflectedEventsNames = {"OnSensorMessageReceived"};

} // namespace Andromeda::Meta
