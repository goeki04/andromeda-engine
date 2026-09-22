#pragma once

// ============================================================================
//  AUTO-GENERATED FILE - DO NOT EDIT BY HAND
//  Produced by metaData/gen_struct_meta.py
//  Sources:
//      modules/definitions/renderer/a_particle_group.hpp
//  Changes to the source headers are picked up on the next build
//  (target: generate_ecs_metadata).
// ============================================================================

// FieldInfo, StructInfo and forEachField live in the hand-written core header, so that
// several generated files can coexist in one translation unit without redefining them.
#include "a_meta_core.hpp"

#include "a_particle_group.hpp"

namespace Andromeda::Meta {


    // ------------------------------------------------------------------------
    // Andromeda::NodeType (0 fields) from modules/definitions/renderer/a_particle_group.hpp
    // ------------------------------------------------------------------------
    template <>
    struct StructInfo<::Andromeda::NodeType> {
        using type = ::Andromeda::NodeType;

        static constexpr bool reflected = true;
        static constexpr std::string_view name = "NodeType";
        static constexpr std::string_view qualifiedName = "Andromeda::NodeType";
        static constexpr std::string_view header = "modules/definitions/renderer/a_particle_group.hpp";
        static constexpr std::string_view doc = "";

        static constexpr auto fields = std::make_tuple();
        static constexpr std::array<std::string_view, 0> fieldNames = {};
        static constexpr std::size_t fieldCount = std::tuple_size_v<decltype(fields)>;
    };

    // ------------------------------------------------------------------------
    // Andromeda::GraphVariable (3 fields) from modules/definitions/renderer/a_particle_group.hpp
    // ------------------------------------------------------------------------
    template <>
    struct StructInfo<::Andromeda::GraphVariable> {
        using type = ::Andromeda::GraphVariable;

        static constexpr bool reflected = true;
        static constexpr std::string_view name = "GraphVariable";
        static constexpr std::string_view qualifiedName = "Andromeda::GraphVariable";
        static constexpr std::string_view header = "modules/definitions/renderer/a_particle_group.hpp";
        static constexpr std::string_view doc = "A named value shared by every particle group graph of one ParticleSystem.";

        static constexpr auto fields = std::make_tuple(
            makeField("id", "u32", "Stable ID nodes refer to; survives renaming and reordering. 0 = unassigned.", "0", &type::id),
            makeField("name", "std::string", "Display name, freely editable.", "", &type::name),
            makeField("value", "GraphValue", "Current value; its alternative is the variable's type.", "", &type::value)
        );

        static constexpr std::array<std::string_view, 3> fieldNames = {"id", "name", "value"};
        static constexpr std::size_t fieldCount = std::tuple_size_v<decltype(fields)>;
    };

    // ------------------------------------------------------------------------
    // Andromeda::EventBinding (4 fields) from modules/definitions/renderer/a_particle_group.hpp
    // ------------------------------------------------------------------------
    template <>
    struct StructInfo<::Andromeda::EventBinding> {
        using type = ::Andromeda::EventBinding;

        static constexpr bool reflected = true;
        static constexpr std::string_view name = "EventBinding";
        static constexpr std::string_view qualifiedName = "Andromeda::EventBinding";
        static constexpr std::string_view header = "modules/definitions/renderer/a_particle_group.hpp";
        static constexpr std::string_view doc = "One \"event drives field\" connection of a particle group, as authored in the editor.";

        static constexpr auto fields = std::make_tuple(
            makeField("showEditWindow", "bool", "", "false", &type::showEditWindow),
            makeField("eventChannel", "i32", "", "-1", &type::eventChannel),
            makeField("eventIndex", "i32", "Index into Meta::ReflectedEventsNames, -1 while no event is selected.", "-1", &type::eventIndex),
            makeField("fieldIndex", "i32", "Index into the bindable fields of ParticleGroup, -1 while unselected.", "-1", &type::fieldIndex)
        );

        static constexpr std::array<std::string_view, 4> fieldNames = {"showEditWindow", "eventChannel", "eventIndex", "fieldIndex"};
        static constexpr std::size_t fieldCount = std::tuple_size_v<decltype(fields)>;
    };

    // ------------------------------------------------------------------------
    // Andromeda::ParticleGroup (8 fields) from modules/definitions/renderer/a_particle_group.hpp
    // ------------------------------------------------------------------------
    template <>
    struct StructInfo<::Andromeda::ParticleGroup> {
        using type = ::Andromeda::ParticleGroup;

        static constexpr bool reflected = true;
        static constexpr std::string_view name = "ParticleGroup";
        static constexpr std::string_view qualifiedName = "Andromeda::ParticleGroup";
        static constexpr std::string_view header = "modules/definitions/renderer/a_particle_group.hpp";
        static constexpr std::string_view doc = "One emitter configuration: how many particles there are, how they look and how they move.";

        static constexpr auto fields = std::make_tuple(
            makeField("groupName", "std::string", "Name of the particle group for identification.", "\"ParticleGroup_1\"", &type::groupName),
            makeField("particleCount", "i32", "Maximum number of particles allowed in this group.", "1000", &type::particleCount),
            makeField("size", "float", "Size of each particle in this group.", "1.0f", &type::size),
            makeField("velocity", "vec3", "Initial velocity of particles in this group.", "{1.0f, 1.0f, 1.0f}", &type::velocity),
            makeField("particleColor", "vec3", "Color of the particles (RGB).", "{1.0f, 1.0f, 1.0f}", &type::particleColor),
            makeField("minLifeTime", "float", "Minimum lifetime of the particles in seconds.", "0.0f", &type::minLifeTime),
            makeField("eventBindings", "std::vector<EventBinding>", "List of event bindings for this particle group.", "", &type::eventBindings),
            makeField("graph", "ParticleGraph", "Node graph edited in the particle editor.", "", &type::graph)
        );

        static constexpr std::array<std::string_view, 8> fieldNames = {"groupName", "particleCount", "size", "velocity", "particleColor", "minLifeTime", "eventBindings", "graph"};
        static constexpr std::size_t fieldCount = std::tuple_size_v<decltype(fields)>;
    };

    /** @brief Every struct this header carries metadata for. */
    using ReflectedStructs = std::tuple<
        ::Andromeda::NodeType,
        ::Andromeda::GraphVariable,
        ::Andromeda::EventBinding,
        ::Andromeda::ParticleGroup
    >;

    /** @brief The same types as ReflectedStructs, as display names in declaration order. */
    inline constexpr std::array<std::string_view, 4> ReflectedStructsNames = {"NodeType", "GraphVariable", "EventBinding", "ParticleGroup"};

} // namespace Andromeda::Meta
