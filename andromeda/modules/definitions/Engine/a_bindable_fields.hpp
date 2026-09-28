#pragma once
#include <array>
#include <string_view>
#include <type_traits>
#include "a_primitives.hpp"
#include "a_meta_core.hpp"
namespace Andromeda {
    template<typename T>
    constexpr std::size_t countBindableFields() {
        std::size_t n = 0;
        Andromeda::Meta::forEachField<T>([&](auto const& f) {
            using V = typename std::decay_t<decltype(f)>::member_type;
            if constexpr (ChannelTraits<V>::count > 0)
                ++n;
        });
        return n;
    }

    template<typename T>
    constexpr auto makeBindableFieldNames() {
        std::array<std::string_view, countBindableFields<T>()> out{};
        std::size_t i = 0;
        Andromeda::Meta::forEachField<T>([&](auto const& f) {
            using V = typename std::decay_t<decltype(f)>::member_type;
            if constexpr (ChannelTraits<V>::count > 0)
                out[i++] = f.name;
        });
        return out;
    }

    /** @brief Counts the fields of T whose type is exactly @c Wanted; sizes the arrays below. */
    template<typename T, typename Wanted>
    constexpr std::size_t countFieldsOfType() {
        std::size_t n = 0;
        Andromeda::Meta::forEachField<T>([&](auto const& f) {
            using V = typename std::decay_t<decltype(f)>::member_type;
            if constexpr (std::is_same_v<V, Wanted>)
                ++n;
        });
        return n;
    }

    /** @brief The names of T's fields that carry a number, so everything bindable except the flags. */
    template<typename T>
    constexpr auto makeNumericFieldNames() {
        std::array<std::string_view, countBindableFields<T>() - countFieldsOfType<T, bool>()> out{};
        std::size_t i = 0;
        Andromeda::Meta::forEachField<T>([&](auto const& f) {
            using V = typename std::decay_t<decltype(f)>::member_type;
            if constexpr (ChannelTraits<V>::count > 0 && !std::is_same_v<V, bool>)
                out[i++] = f.name;
        });
        return out;
    }

    template<typename T>
    constexpr auto makeBindableFieldChannels() {
        std::array<u32, countBindableFields<T>()> out{};
        std::size_t i = 0;
        Andromeda::Meta::forEachField<T>([&](auto const& f) {
            using V = typename std::decay_t<decltype(f)>::member_type;
            if constexpr (ChannelTraits<V>::count > 0) {
                out[i++] = ChannelTraits<V>::count;
            }
        });
        return out;
    }
}