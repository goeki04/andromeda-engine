#pragma once

// ============================================================================
//  AUTO-GENERATED FILE - DO NOT EDIT BY HAND
//  Produced by metaData/gen_struct_meta.py
//  Sources:
//      modules/definitions/renderer/a_particle_group.hpp
//  Changes to the source headers are picked up on the next build
//  (target: generate_ecs_metadata).
// ============================================================================

#include <array>
#include <cstddef>
#include <string_view>
#include <tuple>
#include <utility>

#include "a_particle_group.hpp"

namespace Andromeda::Meta {

    /** @brief Description of a single data member of a reflected struct. */
    template <typename Owner, typename Member>
    struct FieldInfo {
        using owner_type = Owner;
        using member_type = Member;

        std::string_view name;           ///< Member name, exactly as written in the header.
        std::string_view typeName;       ///< Type as source text, e.g. "vec3".
        std::string_view doc;            ///< Doxygen comment of the member ("" if none).
        std::string_view defaultLiteral; ///< Default initializer as text ("" if none).
        Member Owner::* pointer;         ///< Pointer-to-member for generic access.

        constexpr const Member& get(const Owner& owner) const noexcept { return owner.*pointer; }
        constexpr Member& get(Owner& owner) const noexcept { return owner.*pointer; }
    };

    template <typename Owner, typename Member>
    constexpr FieldInfo<Owner, Member> makeField(std::string_view name,
                                                 std::string_view typeName,
                                                 std::string_view doc,
                                                 std::string_view defaultLiteral,
                                                 Member Owner::* pointer) noexcept {
        return FieldInfo<Owner, Member>{name, typeName, doc, defaultLiteral, pointer};
    }

    /** @brief Primary template - specialized below for every scanned struct. */
    template <typename T>
    struct StructInfo {
        static constexpr bool reflected = false;
    };

    /** @brief True when metadata was generated for T. */
    template <typename T>
    inline constexpr bool isReflected = StructInfo<T>::reflected;

    /** @brief Calls fn(field) for every field of T, in declaration order. */
    template <typename T, typename Fn>
    constexpr void forEachField(Fn&& fn) {
        std::apply([&fn](auto const&... field) { (fn(field), ...); }, StructInfo<T>::fields);
    }

    /** @brief Calls fn(field, value) for every field of a concrete instance. */
    template <typename T, typename Fn>
    constexpr void forEachField(T& instance, Fn&& fn) {
        std::apply([&](auto const&... field) { (fn(field, instance.*(field.pointer)), ...); },
                   StructInfo<T>::fields);
    }

    template <typename T, typename Fn>
    constexpr void forEachField(const T& instance, Fn&& fn) {
        std::apply([&](auto const&... field) { (fn(field, instance.*(field.pointer)), ...); },
                   StructInfo<T>::fields);
    }


    // ------------------------------------------------------------------------
    // Andromeda::EventBinding (1 fields) from modules/definitions/renderer/a_particle_group.hpp
    // ------------------------------------------------------------------------
    template <>
    struct StructInfo<::Andromeda::EventBinding> {
        using type = ::Andromeda::EventBinding;

        static constexpr bool reflected = true;
        static constexpr std::string_view name = "EventBinding";
        static constexpr std::string_view qualifiedName = "Andromeda::EventBinding";
        static constexpr std::string_view header = "modules/definitions/renderer/a_particle_group.hpp";
        static constexpr std::string_view doc = "";

        static constexpr auto fields = std::make_tuple(
            makeField("event", "IEvent*", "Type of the event to bind to.", "", &type::event)
        );

        static constexpr std::array<std::string_view, 1> fieldNames = {"event"};
        static constexpr std::size_t fieldCount = std::tuple_size_v<decltype(fields)>;
    };

    // ------------------------------------------------------------------------
    // Andromeda::ParticleGroup (7 fields) from modules/definitions/renderer/a_particle_group.hpp
    // ------------------------------------------------------------------------
    template <>
    struct StructInfo<::Andromeda::ParticleGroup> {
        using type = ::Andromeda::ParticleGroup;

        static constexpr bool reflected = true;
        static constexpr std::string_view name = "ParticleGroup";
        static constexpr std::string_view qualifiedName = "Andromeda::ParticleGroup";
        static constexpr std::string_view header = "modules/definitions/renderer/a_particle_group.hpp";
        static constexpr std::string_view doc = "";

        static constexpr auto fields = std::make_tuple(
            makeField("groupName", "std::string", "Name of the particle group for identification.", "\"ParticleGroup_1\"", &type::groupName),
            makeField("particleCount", "i32", "Maximum number of particles allowed in this group.", "1000", &type::particleCount),
            makeField("size", "float", "Size of each particle in this group.", "1.0f", &type::size),
            makeField("velocity", "vec3", "Initial velocity of particles in this group.", "{1.0f, 1.0f, 1.0f}", &type::velocity),
            makeField("particleColor", "vec3", "Color of the particles (RGB).", "{1.0f, 1.0f, 1.0f}", &type::particleColor),
            makeField("minLifeTime", "float", "Minimum lifetime of the particles in seconds.", "0.0f", &type::minLifeTime),
            makeField("eventBindings", "std::vector<EventBinding>", "List of event bindings for this particle group.", "", &type::eventBindings)
        );

        static constexpr std::array<std::string_view, 7> fieldNames = {"groupName", "particleCount", "size", "velocity", "particleColor", "minLifeTime", "eventBindings"};
        static constexpr std::size_t fieldCount = std::tuple_size_v<decltype(fields)>;
    };

    /** @brief Every struct this header carries metadata for. */
    using ReflectedStructs = std::tuple<
        ::Andromeda::EventBinding,
        ::Andromeda::ParticleGroup
    >;

} // namespace Andromeda::Meta
