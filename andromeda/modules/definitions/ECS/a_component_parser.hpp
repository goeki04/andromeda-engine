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
#include <type_traits>
namespace Andromeda {

    inline std::string_view graphValueTypeName(const GraphValue& value) {
        return std::visit(
            [](const auto& v) -> std::string_view {
                using T = std::decay_t<decltype(v)>;
                if constexpr (std::is_same_v<T, i32>)
                    return "int";
                if constexpr (std::is_same_v<T, float>)
                    return "float";
                if constexpr (std::is_same_v<T, bool>)
                    return "bool";
            },
            value);
    }

    inline void to_json(nlohmann::json& j, const GraphVariable& v) {
        j["id"] = v.id;
        j["name"] = v.name;
        j["type"] = graphValueTypeName(v.value);
        std::visit([&j](const auto& value) { j["value"] = value; }, v.value);
    }

    inline void from_json(const nlohmann::json& j, GraphVariable& v) {
        // Variables saved before IDs existed have none; the ParticleSystem loader assigns one.
        if (j.contains("id"))
            j.at("id").get_to(v.id);
        j.at("name").get_to(v.name);
        const std::string type = j.at("type").get<std::string>();
        const nlohmann::json& value = j.at("value");
        if (type == "int")
            v.value = value.get<i32>();
        else if (type == "float")
            v.value = value.get<float>();
        else if (type == "bool")
            v.value = value.get<bool>();
        else
            throw std::runtime_error("Unknown GraphVariable type: " + type);
    }

    inline void to_json(nlohmann::json& j, const ParticleGroup& p) {
        j = nlohmann::json{
            {"groupName", p.groupName}, {"particleCount", p.particleCount}, {"size", p.size},
            {"velocity", p.velocity},   {"particleColor", p.particleColor}, {"minLifeTime", p.minLifeTime}
        };
    }

    inline void from_json(const nlohmann::json& j, ParticleGroup& p) {
        j.at("groupName").get_to(p.groupName);
        j.at("particleCount").get_to(p.particleCount);
        j.at("size").get_to(p.size);
        j.at("velocity").get_to(p.velocity);
        j.at("particleColor").get_to(p.particleColor);
        j.at("minLifeTime").get_to(p.minLifeTime);
        // An older "graphVariables" key here is read by the ParticleSystem loader, not by the group.
    }
} // namespace Andromeda

/**
 * @namespace Andromeda::ECS::Component
 * @brief Contains all ECS component structures and their associated serialization logic.
 */
namespace Andromeda::ECS::Component {

    inline void to_json(nlohmann::json& j, const deviceType& e) {
        j = e;
    }

    inline void from_json(const nlohmann::json& j, deviceType& e) {
        e = static_cast<deviceType>(j.get<int>());
    }

    /** @brief Serializes the ParticleSystem component (particle groups, ID counters, enabled flag, variables). */
    inline void to_json(nlohmann::json& j, const ParticleSystem& ps) {
        j["particleGroups"] = ps.particleGroups;
        j["nextParticleGroupID"] = ps.nextParticleGroupID;
        j["useParticleGroups"] = ps.useParticleGroups;
        j["nextVariableId"] = ps.nextVariableId;
        j["graphVariables"] = ps.graphVariables;
    }

    inline void from_json(const nlohmann::json& j, ParticleSystem& ps) {
        j.at("particleGroups").get_to(ps.particleGroups);
        j.at("nextParticleGroupID").get_to(ps.nextParticleGroupID);
        j.at("useParticleGroups").get_to(ps.useParticleGroups);

        if (j.contains("graphVariables")) {
            j.at("graphVariables").get_to(ps.graphVariables);
        } else {
            // Scenes saved while variables still belonged to single groups: collect them all here.
            for (const auto& group : j.at("particleGroups")) {
                if (!group.contains("graphVariables"))
                    continue;
                for (const auto& variable : group.at("graphVariables"))
                    ps.graphVariables.push_back(variable.get<GraphVariable>());
            }
        }

        if (j.contains("nextVariableId"))
            j.at("nextVariableId").get_to(ps.nextVariableId);

        // Older variables were saved without an ID; give each one a fresh, unused ID.
        for (GraphVariable& variable : ps.graphVariables) {
            if (variable.id == 0)
                variable.id = ps.nextVariableId++;
            else if (variable.id >= ps.nextVariableId)
                ps.nextVariableId = variable.id + 1;
        }
    }
    /** @brief Serializes the Transform component (Position, Rotation, Scale). */
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Transform, position, rotation, scale)
    /** @brief Serializes the Axis-Aligned Bounding Box (AABB) component. */
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(AABB, min, max, center)

    /** @brief Serializes the Tag component used for entity identification. */
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Tag, name)

    /** @brief Serializes the Device component. */
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Device, type)

    /** @brief Serializes the MeshRenderer component. */
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(MeshRenderer, meshID)

    /** @brief Serializes the Material component. */
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Material, materialName)
}