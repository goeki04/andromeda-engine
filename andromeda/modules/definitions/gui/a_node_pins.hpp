#pragma once
#include <type_traits>
#include <variant>
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

        /** @brief Node and field a pin belongs to - the two halves of a pin ID. */
    struct PinAddress {
        u32 nodeId = 0;
        u32 fieldIndex = 0;
    };

    /**
     * @brief Pin ID = node ID in the upper 32 bits, field index + 1 in the lower (so it is never 0).
     * @details Plain numbers on purpose: loading a graph happens in this data module, which must not
     *          depend on the node editor. The GUI wraps the result in an ed::PinId.
     */
    inline u64 encodePinId(u32 nodeId, u32 fieldIndex) {
        return (static_cast<u64>(nodeId) << 32) | (static_cast<u64>(fieldIndex) + 1);
    }

    /** @brief Inverse of encodePinId. */
    inline PinAddress decodePinId(u64 pinId) {
        return PinAddress{static_cast<u32>(pinId >> 32), static_cast<u32>(pinId & 0xFFFFFFFF) - 1};
    }
    
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

    /** @brief A value travelling along a link; one alternative per ValueType except None. */
    using NodeValue = std::variant<i32, float, bool, vec2, vec3>;

    // ---------------------------------------------------------------------------------------------
    // What may be plugged into what. The editor asks canConnect() before it accepts a link, evaluation
    // asks readAs() when it moves a value along that link. Both answer from the same rules below, so a
    // link the editor allows always carries its value through - a rule added to one without the other
    // means either a link that cannot be drawn or one that silently delivers nothing.
    // ---------------------------------------------------------------------------------------------

    /** @brief True when an output of type @p from may feed an input of type @p to. */
    inline constexpr bool canConnect(ValueType from, ValueType to) {
        if (from == ValueType::None || to == ValueType::None)
            return false;
        if (from == ValueType::Int && to == ValueType::Float)
            return true; // widening, no loss; see readAs()
        return from == to;
    }

    /**
     * @brief Reads @p value as ValueT, applying the conversions canConnect() allows.
     * @param out Only written when the read succeeds, so a mismatch leaves the pin on its own value.
     * @return false when a ValueT cannot be made from @p value.
     */
    template<typename ValueT>
    bool readAs(const NodeValue& value, ValueT& out) {
        if (const ValueT* exact = std::get_if<ValueT>(&value)) {
            out = *exact;
            return true;
        }
        if constexpr (std::is_same_v<ValueT, float>) {
            if (const i32* whole = std::get_if<i32>(&value)) {
                out = static_cast<float>(*whole);
                return true;
            }
        }
        return false;
    }
}