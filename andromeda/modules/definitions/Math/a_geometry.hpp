#pragma once
#include <vector>
#include <glm/glm.hpp>
#include "a_components.hpp"
#include "a_primitives.hpp"
#include <stdexcept>

namespace Andromeda {

    /**
     * @brief Represents a single point in 3D space with associated data.
     */
    struct Vertex {
        vec3 pos    = vec3(0.0f); ///< 3D position of the vertex
        vec3 normal = vec3(0.0f); ///< Normal vector for lighting calculations
        vec2 uv     = vec2(0.0f); ///< Texture coordinates (u, v)
        vec3 color  = vec3(1.0f); ///< Diffuse color of the vertex (default: white)
    };

    /**
     * @brief Axis-Aligned Bounding Box for collision and culling.
     */
    struct AABB {
        vec3 min = vec3(1e34f);  ///< Initialized to infinity to ensure correct min-clamping
        vec3 max = vec3(-1e34f); ///< Initialized to negative infinity to ensure correct max-clamping
    };

    /**
     * @brief CPU-side representation of a 3D model's geometry.
     * Manages vertex and index data and provides utility for bounds calculation.
     */
    struct Mesh {
        std::vector<Vertex> vertexbuffer; ///< Storage for all vertices in the mesh
        std::vector<u32> indexBuffer;     ///< Storage for the rendering order (indices)

        /**
         * @brief Move constructor for efficient data transfer.
         * Prevents deep copying of large vertex/index arrays.
         */
        Mesh(std::vector<Vertex>&& vertexPositions, std::vector<u32>&& vertexIndices)
            : vertexbuffer(std::move(vertexPositions)),
            indexBuffer(std::move(vertexIndices)) {
        }

        Mesh() = default;

        /**
         * @brief Calculates the Axis-Aligned Bounding Box (AABB) for this mesh.
         * Iterates through all vertices to find the min/max bounds.
         * @return Calculated AABB component.
         * @throws std::runtime_error if the vertex buffer is empty.
         */
        [[nodiscard]] ECS::Component::AABB getAABB() const
        {
            if (vertexbuffer.empty())
                throw std::runtime_error("Mesh has no vertices");

            const auto& vb = vertexbuffer;
            vec3 min = vb[0].pos;
            vec3 max = min;

            for (const auto& v : vb) {
                const vec3 p = v.pos;
                min = glm::min(min, p);
                max = glm::max(max, p);
            }

            ECS::Component::AABB aabb;
            aabb.min = min;
            aabb.max = max;
            aabb.center = (min + max) * 0.5f;

            return aabb;
        }

        Mesh(const Mesh&) = delete;
        Mesh& operator=(const Mesh&) = delete;
        Mesh(Mesh&& other) noexcept = default;
        Mesh& operator=(Mesh&& other) noexcept = default;

        ~Mesh() = default;
    };
}