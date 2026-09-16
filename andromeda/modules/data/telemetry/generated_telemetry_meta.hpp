#pragma once

// ============================================================================
//  AUTO-GENERATED FILE - DO NOT EDIT BY HAND
//  Produced by metaData/gen_struct_meta.py
//  Sources:
//      modules/data/telemetry/bmv080_telemetry.hpp
//  Changes to the source headers are picked up on the next build
//  (target: generate_ecs_metadata).
// ============================================================================

// FieldInfo, StructInfo and forEachField live in the hand-written core header, so that
// several generated files can coexist in one translation unit without redefining them.
#include "a_meta_core.hpp"

#include "bmv080_telemetry.hpp"

namespace Andromeda::Meta {


    // ------------------------------------------------------------------------
    // Andromeda::BMV080Telemetry (4 fields) from modules/data/telemetry/bmv080_telemetry.hpp
    // ------------------------------------------------------------------------
    template <>
    struct StructInfo<::Andromeda::BMV080Telemetry> {
        using type = ::Andromeda::BMV080Telemetry;

        static constexpr bool reflected = true;
        static constexpr std::string_view name = "BMV080Telemetry";
        static constexpr std::string_view qualifiedName = "Andromeda::BMV080Telemetry";
        static constexpr std::string_view header = "modules/data/telemetry/bmv080_telemetry.hpp";
        static constexpr std::string_view doc = "";

        static constexpr auto fields = std::make_tuple(
            makeField("pm1_0", "int", "", "{0}", &type::pm1_0),
            makeField("pm2_5", "int", "", "{0}", &type::pm2_5),
            makeField("pm10_0", "int", "", "{0}", &type::pm10_0),
            makeField("is_obstructed", "bool", "", "{false}", &type::is_obstructed)
        );

        static constexpr std::array<std::string_view, 4> fieldNames = {"pm1_0", "pm2_5", "pm10_0", "is_obstructed"};
        static constexpr std::size_t fieldCount = std::tuple_size_v<decltype(fields)>;
    };

    /** @brief Every struct this header carries metadata for. */
    using ReflectedTelemetry = std::tuple<
        ::Andromeda::BMV080Telemetry
    >;

    /** @brief The same types as ReflectedTelemetry, as display names in declaration order. */
    inline constexpr std::array<std::string_view, 1> ReflectedTelemetryNames = {"BMV080Telemetry"};

} // namespace Andromeda::Meta
