#pragma once
#include <type_traits>
#include "a_primitives.hpp"

namespace Andromeda {

    /** @brief What a node field is in the graph: a pin on the left, an editable setting, or a pin on the right. */
    enum class PinRole : u8 { None, Input, Param, Output };

    template<PinRole Role, typename T>
    struct Pin {
        T value{};
        constexpr Pin() = default;
        constexpr Pin(T v) : value(v) {} // allows 'Param<float> inMin = 0.0f;'
    };

    template<typename T>
    using Input = Pin<PinRole::Input, T>;
    template<typename T>
    using Param = Pin<PinRole::Param, T>;
    template<typename T>
    using Output = Pin<PinRole::Output, T>;

    /** @brief Role and inner type of a node field; PinRole::None for plain members. */
    template<typename T>
    struct PinTraits {
        static constexpr PinRole role = PinRole::None;
        using value_type = T;
    };

    template<PinRole Role, typename T>
    struct PinTraits<Pin<Role, T>> {
        static constexpr PinRole role = Role;
        using value_type = T;
    };

    template<typename T>
    inline constexpr PinRole pinRole = PinTraits<T>::role;

    // if one of these fails, the build stops here.
    static_assert(pinRole<Input<float>> == PinRole::Input);
    static_assert(pinRole<Param<float>> == PinRole::Param);
    static_assert(pinRole<Output<vec3>> == PinRole::Output);
    static_assert(pinRole<float> == PinRole::None);
    static_assert(std::is_same_v<PinTraits<Output<vec3>>::value_type, vec3>);

    enum class ValueType : u8 { None, Int, Float, Bool, Vec2, Vec3 };

    template<typename T>
    struct ValueTypeOf {
        static constexpr ValueType value = ValueType::None;
    };

    template<>
    struct ValueTypeOf<i32> {
        static constexpr ValueType value = ValueType::Int;
    };

    template<>
    struct ValueTypeOf<float> {
        static constexpr ValueType value = ValueType::Float;
    };

    template<>
    struct ValueTypeOf<bool> {
        static constexpr ValueType value = ValueType::Bool;
    };

    template<>
    struct ValueTypeOf<vec2> {
        static constexpr ValueType value = ValueType::Vec2;
    };

    template<>
    struct ValueTypeOf<vec3> {
        static constexpr ValueType value = ValueType::Vec3;
    };

    template<typename T>
    inline constexpr ValueType valueTypeOf = ValueTypeOf<T>::value;

    static_assert(valueTypeOf<float> == ValueType::Float);
    static_assert(valueTypeOf<i32> == ValueType::Int);
    static_assert(valueTypeOf<bool> == ValueType::Bool);
    static_assert(valueTypeOf<vec2> == ValueType::Vec2);
    static_assert(valueTypeOf<vec3> == ValueType::Vec3);
}