#pragma once

/**
 * @file a_components.hpp
 * @brief Definitions of all ECS component structures used by the Andromeda engine.
 *
 * @details Components in this file are scanned at build time by the Python
 *          metadata pre-processor (`gen_components.py`) to generate the
 *          component tuple and name tables (see @c generated_components.hpp
 *          and @c generated_component_names.hpp). They are intentionally kept
 *          as plain, dependency-light POD-like structs so that the registry
 *          can store, copy and serialize them cheaply.
 */

#include "a_math.hpp"
#include "a_model_record.hpp"
#include <algorithm>
#include <span>
#include <string>
#include <string_view>
#include "a_particle_group.hpp"
namespace Andromeda::ECS::Component {

    /**
     * @brief Resets a component back to its default-constructed state.
     * @tparam T The component type, must be default-constructible.
     * @param component Reference to the component instance to reset in place.
     */
    template<typename T>
    void resetComponent(T& component) {
        component = T();
    }

    /**
    * @brief Marker attribute for the automated Undo/Redo system.
    *
    * @details Components marked with this attribute are parsed by the
    *          Python metadata pre-processor (gen_undo.py). The build
    *          system uses this information to automatically generate:
    *          - Command-specific POD data structures.
    *          - Dispatcher logic for the Data-Oriented Undo buffer.
    *          - Memory-efficient snapshot logic for editor state management.
    *
    * @note Only components intended for manual user editing in the
    *       Inspector should be marked. Volatile or calculated data
    *       (like AABB or Physics state) should be excluded to save memory.
    */

    /**
     * @struct [[Andromeda::Undo]]
     * @brief Spatial representation of an entity.
     * @details Marked with [[Andromeda::Undo]] to track position, scale,
     *          and rotation changes in the editor.
     */
    struct [[Andromeda::Undo]] Transform {
        vec3 position = { 0.0f, 0.0f, 0.0f }; ///< World-space position of the entity.
        vec3 scale = { 1.0f, 1.0f, 1.0f };    ///< Per-axis scale factor of the entity.
        quat rotation = quat(1.0f,0.0f,0.0f,0.0f); ///< Orientation of the entity, stored as a quaternion (identity by default).

        /**
         * @brief Builds the local-to-world transformation matrix from position, rotation and scale.
         * @return The combined TRS (Translate * Rotate * Scale) model matrix.
         */
        [[nodiscard]] mat4 modelMatrix() const {
            mat4 m(1.0f);
            m = amath::translate(m, position);
            m *= glm::mat4_cast(rotation);
            m = amath::scale(m, scale);
            return m;
        }
    };

    /**
     * @brief Associates an entity with a named material used by the renderer.
     * @note Currently stores only a material name; lookup of the actual material data happens elsewhere (resource manager).
     */
    struct Material {
        std::string materialName = "PBRMaterial"; ///< Name of the material to look up/apply when rendering this entity.
    };

    /**
     * @brief Marks an entity as representing a physical hardware device (sensor, controller, etc.) in the scene.
     */
    struct Device
    {
        deviceType type; ///< The kind of device this entity represents (see @c deviceType in a_model_record.hpp).
    };

    /**
     * @brief Links an entity to the mesh that should be drawn for it.
     */
    struct MeshRenderer
    {
        u32 meshID; ///< Identifier of the mesh resource to render, as registered with the resource manager.
    };

    /**
     * @brief Axis-Aligned Bounding Box component, used for picking, culling and gizmo placement.
     * @note This is derived/calculated data (see @c Mesh::getAABB()) rather than user-edited data,
     *       which is why it is not marked with [[Andromeda::Undo]].
     */
    struct AABB {
        vec3 min = { 0.0f,0.0f,0.0f };    ///< Minimum corner of the bounding box in local/world space.
        vec3 max = { 0.0f,0.0f,0.0f };    ///< Maximum corner of the bounding box in local/world space.
        vec3 center = { 0.0f,0.0f,0.0f }; ///< Midpoint between @c min and @c max, cached for convenience.
    };

    /**
     * @brief Component that holds multiple particle groups, each with its own constraints and properties. 
     */
    struct ParticleSystem {
        u32 nextParticleGroupID = 1;               ///< Counter for generating unique IDs for new particle groups.
        bool useParticleGroups = false;            ///< Flag indicating whether to use particle groups or not.
        u32 nextVariableId = 1;                    ///< Next free GraphVariable ID. Never reused.
        std::vector<GraphVariable> graphVariables; ///< Variables shared by the graphs of all particle groups.

        ParticleSystem() {
            // Initialize with a default particle group
            particleGroups.emplace_back().id = nextParticleGroupID++;
        }

        // Defined out-of-line in a_component_parser.hpp (after ParticleGroup's own to_json/from_json),
        // not here: this header is included by many files that never see ParticleGroup's JSON bindings,
        // and those bindings must already be visible when these functions' bodies are compiled.
        friend void to_json(nlohmann::json& j, const ParticleSystem& ps);
        friend void from_json(const nlohmann::json& j, ParticleSystem& ps);

        std::span<ParticleGroup> getParticleGroups() {
            if (useParticleGroups) {
                return particleGroups;
            } else {
                return std::span<ParticleGroup>(&particleGroups[0], 1);
            }
        }

        /** @brief Every group, also while useParticleGroups is off; for work that must reach all graphs. */
        std::span<ParticleGroup> allParticleGroups() {
            return particleGroups;
        }

        void addParticleGroup() {
            auto& group = particleGroups.emplace_back();
            group.id = nextParticleGroupID++; // Assign a unique ID to the new particle group
            group.groupName = "ParticleGroup_" + std::to_string(group.id);
        }

        /**
         * @brief Appends a copy of the group with @p id, graph included, under its own ID and free name.
         * @return The new group, or nullptr if there is no group with @p id.
         */
        ParticleGroup* duplicateParticleGroup(u32 id) {
            const ParticleGroup* source = findParticleGroup(id);
            if (source == nullptr)
                return nullptr;

            // Copied before the push_back: growing the vector would move the source out from under us.
            ParticleGroup copy = *source;
            copy.id = nextParticleGroupID++;
            copy.groupName = makeUniqueGroupName(source->groupName);
            particleGroups.push_back(std::move(copy));
            return &particleGroups.back();
        }

        void removeParticleGroup(i32 indexToRemove) {
            if (indexToRemove >= 0 && indexToRemove < static_cast<i32>(particleGroups.size()) && particleGroups.size() > 1) {
                particleGroups.erase(particleGroups.begin() + indexToRemove);
            }
        }

        /** @brief Removes the group with @p id. The last group is never removed: there is always one. */
        void removeParticleGroup(u32 id) {
            if (particleGroups.size() <= 1)
                return;
            const auto it = std::find_if(particleGroups.begin(), particleGroups.end(),
                                         [id](const ParticleGroup& group) { return group.id == id; });
            if (it != particleGroups.end())
                particleGroups.erase(it);
        }

        /** @brief The group with @p id, or nullptr; ids are unique inside one ParticleSystem. */
        ParticleGroup* findParticleGroup(u32 id) {
            for (ParticleGroup& group : particleGroups) {
                if (group.id == id)
                    return &group;
            }
            return nullptr;
        }

        /** @brief @p wanted, or "wanted (2)", "wanted (3)", ... if that name is already taken. */
        std::string makeUniqueGroupName(std::string_view wanted) const {
            const auto taken = [this](const std::string& name) {
                return std::any_of(particleGroups.begin(), particleGroups.end(),
                                   [&name](const ParticleGroup& group) { return group.groupName == name; });
            };
            std::string name(wanted);
            for (u32 suffix = 2; taken(name); ++suffix)
                name = std::string(wanted) + " (" + std::to_string(suffix) + ")";
            return name;
        }

    private:
        std::vector<ParticleGroup> particleGroups; ///< Groups of particles with specific constraints.
    };

    /**
     * @brief Human-readable identifier for an entity, shown in the editor hierarchy/inspector.
     */
    struct [[Andromeda::Undo]] Tag{
        std::string name = "Unnamed"; ///< Display name of the entity.
    };
}
