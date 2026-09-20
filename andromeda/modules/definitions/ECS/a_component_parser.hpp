#pragma once

/**
 * @file a_component_parser.hpp
 * @brief JSON (de)serialization bindings for all ECS components, used by the scene serializer.
 */
#include "a_particle_group.hpp"
#include <nlohmann/json.hpp>
#include "a_glm_json_parser.hpp"
#include "a_components.hpp"
#include "a_model_record.hpp"
#include <variant>
#include <type_traits>
#include <string>
 /**
  * @namespace Andromeda::ECS::Component
  * @brief Contains all ECS component structures and their associated serialization logic.
  */
namespace Andromeda {

    inline void to_json(nlohmann::json& j, const ParticleGroup& p) {
        j = nlohmann::json{
            {"groupName", p.groupName}, {"particleCount", p.particleCount}, {"size", p.size},
            {"velocity", p.velocity},   {"particleColor", p.particleColor}, {"minLifeTime", p.minLifeTime}};
    }

    inline void from_json(const nlohmann::json& j, ParticleGroup& p) {
        j.at("groupName").get_to(p.groupName);
        j.at("particleCount").get_to(p.particleCount);
        j.at("size").get_to(p.size);
        j.at("velocity").get_to(p.velocity);
        j.at("particleColor").get_to(p.particleColor);
        j.at("minLifeTime").get_to(p.minLifeTime);
    }
} // namespace Andromeda
namespace Andromeda::ECS::Component {

    inline void to_json(nlohmann::json& json, const deviceType& e) {
        json = e;
    }

    inline void from_json(const nlohmann::json& json, deviceType& e) {
        e = static_cast<deviceType>(json.get<int>());
    }

    /** @brief Serializes the ParticleSystem component (particle groups, ID counter, enabled flag). */
    inline void to_json(nlohmann::json& j, const ParticleSystem& ps) {
        j["particleGroups"] = ps.particleGroups;
        j["nextParticleGroupID"] = ps.nextParticleGroupID;
        j["useParticleGroups"] = ps.useParticleGroups;
    }

    inline void from_json(const nlohmann::json& j, ParticleSystem& ps) {
        j.at("particleGroups").get_to(ps.particleGroups);
        j.at("nextParticleGroupID").get_to(ps.nextParticleGroupID);
        j.at("useParticleGroups").get_to(ps.useParticleGroups);
    }
    /** @brief Serializes the Transform component (Position, Rotation, Scale). */
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Transform, position, rotation, scale)
    /** @brief Serializes the Axis-Aligned Bounding Box (AABB) component. */
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(AABB, min, max, center)

    /** @brief Serializes the Tag component used for entity identification. */
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Tag, name)

    /** @brief Serializes the Device component. */
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Device, type)
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(MeshRenderer, meshID);

    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Material, materialName)
}