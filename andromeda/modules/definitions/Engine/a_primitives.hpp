#pragma once

/**
 * @file a_primitives.hpp
 * @brief Fundamental type aliases used throughout the Andromeda engine.
 *
 * @details This header is the lowest-level building block of the `definitions`
 *          module. It re-exports GLM's vector/matrix types and the fixed-width
 *          integer types under short, engine-wide names (e.g. @c vec3, @c u32),
 *          so that the rest of the codebase does not need to depend on GLM or
 *          `<cstdint>` directly. Almost every other header in the project
 *          includes this file, either directly or transitively.
 */

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/quaternion.hpp>
#include <cstdint>
namespace Andromeda {
    /** @brief 2-component single-precision floating point vector (alias for @c glm::vec2). */
    using vec2 = glm::vec2;
    /** @brief 3-component single-precision floating point vector (alias for @c glm::vec3). */
    using vec3 = glm::vec3;
    /** @brief 4-component single-precision floating point vector (alias for @c glm::vec4). */
    using vec4 = glm::vec4;
    /** @brief 2-component signed integer vector (alias for @c glm::ivec2). */
    using ivec2 = glm::ivec2;
    /** @brief 3-component signed integer vector (alias for @c glm::ivec3). */
    using ivec3 = glm::ivec3;
    /** @brief 4-component signed integer vector (alias for @c glm::ivec4). */
    using ivec4 = glm::ivec4;
    /** @brief 4x4 single-precision floating point matrix (alias for @c glm::mat4). Used for transforms, view and projection matrices. */
    using mat4 = glm::mat4;
    /** @brief Single-precision quaternion (alias for @c glm::quat). Used for rotations to avoid gimbal lock. */
    using quat = glm::quat;
    /** @brief 3x3 single-precision floating point matrix (alias for @c glm::mat3). Typically used for normal/rotation-only transforms. */
    using mat3 = glm::mat3;

    /** @brief 8-bit unsigned integer (alias for @c std::uint8_t). */
    using u8 = std::uint8_t;
    /** @brief 16-bit unsigned integer (alias for @c std::uint16_t). */
    using u16 = std::uint16_t;
    /** @brief 32-bit unsigned integer (alias for @c std::uint32_t). Commonly used for handles, IDs and sizes. */
    using u32 = std::uint32_t;
    /** @brief 64-bit unsigned integer (alias for @c std::uint64_t). */
    using u64 = std::uint64_t;

    /** @brief 8-bit signed integer (alias for @c std::int8_t). */
    using i8 = std::int8_t;
    /** @brief 16-bit signed integer (alias for @c std::int16_t). */
    using i16 = std::int16_t;
    /** @brief 32-bit signed integer (alias for @c std::int32_t). */
    using i32 = std::int32_t;
    /** @brief 64-bit signed integer (alias for @c std::int64_t). */
    using i64 = std::int64_t;

    template<typename T>
    struct ChannelTraits {
        static constexpr u32 count = 0; 
    };

    template<>
    struct ChannelTraits<vec2> {
        static constexpr u32 count = 2;
    };
    template<>

    struct ChannelTraits<vec3> {
        static constexpr u32 count = 3;
    };
    template<>

    struct ChannelTraits<vec4> {
        static constexpr u32 count = 4;
    };

    template<>
    struct ChannelTraits<ivec2> {
        static constexpr u32 count = 2;
    };

    template<>
    struct ChannelTraits<ivec3> {
        static constexpr u32 count = 3;
    };

    template<>
    struct ChannelTraits<ivec4> {
        static constexpr u32 count = 4;
    };

    template<>
    struct ChannelTraits<float> {
        static constexpr u32 count = 1;
    };

    template<>
    struct ChannelTraits<i32> {
        static constexpr u32 count = 1;
    };

    template<>
    struct ChannelTraits<bool> {
        static constexpr u32 count = 1;
    };

    /**
     * @namespace Andromeda::ECS
     * @brief Entity-Component-System primitives shared across the engine.
     */
    namespace ECS{
        /** @brief Type alias for Entity identifiers. Entities are plain numeric handles into the registry, not objects. */
        using Entity = uint32_t;

        /** @brief Sentinel value representing "no entity" / an invalid handle. */
        constexpr Entity INVALID_ENTITY_ID = static_cast<Entity>(-1);
    }
}
