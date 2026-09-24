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
#include "a_logger.hpp"
#include <algorithm>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>
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

    // ------------------------------------------------------------------------
    // Particle graph
    //
    // Everything is saved by name, never by index: the node type as its struct name, fields as a JSON
    // object keyed by field name, links as node ID + pin name. That keeps saved graphs loadable when node
    // types are added or removed, and when fields are added, removed or reordered - a field missing from
    // the file keeps its default, an unknown one is ignored.
    // ------------------------------------------------------------------------

    /** @brief Format version written with every graph; bump it when the layout below changes incompatibly. */
    inline constexpr i32 kParticleGraphVersion = 1;

    namespace Detail {
        /**
         * @brief The fields of one node worth saving. Inputs (their unconnected fallback) and params are
         *        saved by value, plain data such as variableId as is. Outputs are skipped: evaluation
         *        computes them.
         */
        inline nlohmann::json nodeFieldsToJson(const NodeData& data) {
            nlohmann::json fields = nlohmann::json::object();
            std::visit([&fields](const auto& node) {
                Meta::forEachField(node, [&fields](auto const& field, const auto& member) {
                    using V = std::decay_t<decltype(member)>;
                    const std::string name(field.name);
                    if constexpr (pinRole<V> == PinRole::Input || pinRole<V> == PinRole::Param)
                        fields[name] = member.value;
                    else if constexpr (pinRole<V> == PinRole::None)
                        fields[name] = member;
                });
            }, data);
            return fields;
        }

        /**
         * @brief Reads saved fields back into @p data by name. A field that is missing, or whose saved value
         *        no longer fits its type, keeps its default and logs a warning instead of failing the load.
         */
        inline void nodeFieldsFromJson(const nlohmann::json& fields, NodeData& data, u32 nodeId) {
            std::visit([&fields, nodeId](auto& node) {
                Meta::forEachField(node, [&fields, nodeId](auto const& field, auto& member) {
                    using V = std::decay_t<decltype(member)>;
                    const std::string name(field.name);
                    if (!fields.contains(name))
                        return;
                    try {
                        if constexpr (pinRole<V> == PinRole::Input || pinRole<V> == PinRole::Param)
                            fields.at(name).get_to(member.value);
                        else if constexpr (pinRole<V> == PinRole::None)
                            fields.at(name).get_to(member);
                    } catch (const nlohmann::json::exception& e) {
                        A_WARN("Graph node {}: field '{}' could not be read ({}), keeping its default",
                               nodeId, name, e.what());
                    }
                });
            }, data);
        }

        /**
         * @brief Reads one node. @return false if its type no longer exists; the caller skips the node
         *        rather than failing the whole scene.
         */
        inline bool nodeFromJson(const nlohmann::json& j, NodeInstance& node) {
            const std::string typeName = j.at("type").get<std::string>();
            const i32 typeIndex = nodeTypeIndex(typeName);
            j.at("id").get_to(node.id);
            if (typeIndex < 0) {
                A_WARN("Graph node {}: unknown node type '{}', node skipped", node.id, typeName);
                return false;
            }
            if (j.contains("position"))
                j.at("position").get_to(node.position);
            // Qualified: inside Detail, the unqualified name would find the internal Detail::makeNodeData.
            node.data = Andromeda::makeNodeData(static_cast<size_t>(typeIndex));
            if (j.contains("fields"))
                nodeFieldsFromJson(j.at("fields"), node.data, node.id);
            return true;
        }
    } // namespace Detail

    inline void to_json(nlohmann::json& j, const NodeInstance& node) {
        j["id"] = node.id;
        j["position"] = node.position;
        j["type"] = nodeTypeName(node.data);
        j["fields"] = Detail::nodeFieldsToJson(node.data);
    }

    inline void to_json(nlohmann::json& j, const ParticleGraph& graph) {
        j["version"] = kParticleGraphVersion;
        j["nextNodeId"] = graph.nextNodeId;
        j["nextLinkId"] = graph.nextLinkId;
        j["nodes"] = graph.nodes;

        nlohmann::json links = nlohmann::json::array();
        for (const PinLink& link : graph.links) {
            const PinAddress source = decodePinId(link.sourceId);
            const PinAddress target = decodePinId(link.targetId);
            const NodeInstance* sourceNode = findNode(graph, source.nodeId);
            const NodeInstance* targetNode = findNode(graph, target.nodeId);
            if (!sourceNode || !targetNode)
                continue; // a dangling link has nothing to save
            links.push_back({
                {"id", link.id},
                {"sourceNode", source.nodeId},
                {"sourcePin", fieldNameAt(sourceNode->data, source.fieldIndex)},
                {"targetNode", target.nodeId},
                {"targetPin", fieldNameAt(targetNode->data, target.fieldIndex)},
            });
        }
        j["links"] = links;
    }

    inline void from_json(const nlohmann::json& j, ParticleGraph& graph) {
        graph = ParticleGraph{};

        const i32 version = j.value("version", kParticleGraphVersion);
        if (version > kParticleGraphVersion)
            A_WARN("Particle graph was saved with format version {}, this build knows up to {} - "
                   "unknown data is ignored", version, kParticleGraphVersion);

        graph.nextNodeId = j.value("nextNodeId", 1u);
        graph.nextLinkId = j.value("nextLinkId", 1u);

        if (j.contains("nodes")) {
            for (const nlohmann::json& nodeJson : j.at("nodes")) {
                NodeInstance node;
                if (Detail::nodeFromJson(nodeJson, node))
                    graph.nodes.push_back(std::move(node));
            }
        }

        // Links after the nodes: pin names are turned back into pin IDs via the loaded nodes' fields.
        if (j.contains("links")) {
            for (const nlohmann::json& linkJson : j.at("links")) {
                const u32 sourceNodeId = linkJson.at("sourceNode").get<u32>();
                const u32 targetNodeId = linkJson.at("targetNode").get<u32>();
                const NodeInstance* sourceNode = findNode(graph, sourceNodeId);
                const NodeInstance* targetNode = findNode(graph, targetNodeId);
                if (!sourceNode || !targetNode) {
                    A_WARN("Graph link {}: node {} or {} does not exist, link skipped",
                           linkJson.value("id", 0u), sourceNodeId, targetNodeId);
                    continue;
                }

                const std::string sourcePin = linkJson.at("sourcePin").get<std::string>();
                const std::string targetPin = linkJson.at("targetPin").get<std::string>();
                const i32 sourceField = fieldIndexOf(sourceNode->data, sourcePin);
                const i32 targetField = fieldIndexOf(targetNode->data, targetPin);
                if (sourceField < 0 || targetField < 0) {
                    A_WARN("Graph link {}: pin '{}' or '{}' no longer exists, link skipped",
                           linkJson.value("id", 0u), sourcePin, targetPin);
                    continue;
                }

                graph.links.push_back(PinLink{
                    linkJson.at("id").get<u32>(),
                    encodePinId(sourceNodeId, static_cast<u32>(sourceField)),
                    encodePinId(targetNodeId, static_cast<u32>(targetField)),
                });
            }
        }

        // The counters must stay ahead of every loaded ID, even if the file was edited by hand -
        // otherwise the next node or link would reuse an existing ID.
        for (const NodeInstance& node : graph.nodes)
            graph.nextNodeId = std::max(graph.nextNodeId, node.id + 1);
        for (const PinLink& link : graph.links)
            graph.nextLinkId = std::max(graph.nextLinkId, link.id + 1);
    }

    inline void to_json(nlohmann::json& j, const ParticleGroup& p) {
        j = nlohmann::json{
            {"groupName", p.groupName}, {"particleCount", p.particleCount}, {"size", p.size},
            {"velocity", p.velocity},   {"particleColor", p.particleColor}, {"minLifeTime", p.minLifetime},
            {"graph", p.graph}, {"id", p.id}
        };
    }

    inline void from_json(const nlohmann::json& j, ParticleGroup& p) {
        j.at("groupName").get_to(p.groupName);
        j.at("particleCount").get_to(p.particleCount);
        j.at("size").get_to(p.size);
        j.at("velocity").get_to(p.velocity);
        j.at("particleColor").get_to(p.particleColor);
        j.at("minLifeTime").get_to(p.minLifetime);
        // Scenes saved before graphs were stored have no "graph" key; the group keeps an empty graph.
        if (j.contains("graph"))
            j.at("graph").get_to(p.graph);
        // Groups saved before they had IDs get one from the ParticleSystem loader.
        if (j.contains("id"))
            j.at("id").get_to(p.id);
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

        // Groups without an ID (older scenes) or with a duplicate one (hand-edited file) get a fresh ID;
        // the editor remembers the selected group by ID, so every ID must be unique.
        for (const ParticleGroup& group : ps.allParticleGroups())
            ps.nextParticleGroupID = std::max(ps.nextParticleGroupID, group.id + 1);
        std::vector<u32> seenGroupIds;
        for (ParticleGroup& group : ps.allParticleGroups()) {
            if (group.id == 0 || std::ranges::find(seenGroupIds, group.id) != seenGroupIds.end())
                group.id = ps.nextParticleGroupID++;
            seenGroupIds.push_back(group.id);
        }

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