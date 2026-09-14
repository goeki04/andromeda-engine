#pragma once

/**
 * @file a_meta_core.hpp
 * @brief Shared vocabulary of the struct-reflection system: FieldInfo, StructInfo and forEachField.
 *
 * Hand-written on purpose. Every file produced by metaData/gen_struct_meta.py includes this
 * header instead of carrying its own copy of these definitions - otherwise two generated
 * headers in the same translation unit redefine FieldInfo and the build breaks.
 * The generated files only add StructInfo<> specializations on top of what is declared here.
 */

#include <array>
#include <cstddef>
#include <string_view>
#include <tuple>
#include <utility>

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

} // namespace Andromeda::Meta
