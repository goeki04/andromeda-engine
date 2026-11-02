// Saved particle graphs and particle systems. The format is built to survive change - node types and
// fields saved by name, older scenes migrated on load - and these tests hold it to that.

#include <gtest/gtest.h>
#include <algorithm>
#include <vector>
#include "a_component_parser.hpp"
#include "a_LogCapture.hpp"

using namespace Andromeda;
using namespace Andromeda::Gui::Node;
using Andromeda::ECS::Component::ParticleSystem;
using Andromeda::Test::LogCapture;

namespace {
    u64 pin(const NodeInstance& node, std::string_view field) {
        const i32 index = fieldIndexOf(node.data, field);
        EXPECT_GE(index, 0) << "no field '" << field << "'";
        return encodePinId(node.id, static_cast<u32>(index));
    }

    PinLink& link(ParticleGraph& graph, const NodeInstance& from, std::string_view out, const NodeInstance& to, std::string_view in) {
        return graph.links.emplace_back(PinLink{ graph.nextLinkId++, pin(from, out), pin(to, in) });
    }

    /** @brief Sensor(pm2_5) -> Remap(0..50 -> 0.5..4) -> Output.size, plus a bound FloatVariable. */
    ParticleGraph sampleGraph() {
        ParticleGraph graph;
        Sensor sensor;
        sensor.channel = "pm2_5";
        Remap remap;
        remap.inMax = 50.0f;
        remap.outMin = 0.5f;
        remap.outMax = 4.0f;
        FloatVariable variable;
        variable.value = 2.5f;
        variable.variableId = 3;

        // IDs, not references: addNode() returns an element of graph.nodes, which the next addNode() may move.
        const u32 sensorId = addNode(graph, sensor, vec2(10.0f, 20.0f)).id;
        const u32 remapId = addNode(graph, remap, vec2(200.0f, 20.0f)).id;
        const u32 outputId = addNode(graph, OutputNode{}, vec2(400.0f, 20.0f)).id;
        addNode(graph, variable, vec2(10.0f, 200.0f));
        link(graph, *findNode(graph, sensorId), "value", *findNode(graph, remapId), "value");
        link(graph, *findNode(graph, remapId), "result", *findNode(graph, outputId), "size");
        return graph;
    }

    const NodeInstance& nodeWithId(const ParticleGraph& graph, u32 id) {
        const NodeInstance* node = findNode(graph, id);
        EXPECT_NE(node, nullptr) << "no node " << id;
        return *node;
    }
}

// ----------------------------------------------------------------------------------------------
// ParticleGraph
// ----------------------------------------------------------------------------------------------

TEST(ParticleGraphJson, RoundTripKeepsNodesLinksPositionsAndValues) {
    const ParticleGraph original = sampleGraph();

    const ParticleGraph loaded = nlohmann::json(original).get<ParticleGraph>();

    EXPECT_EQ(nlohmann::json(loaded), nlohmann::json(original));
    ASSERT_EQ(loaded.nodes.size(), 4u);
    ASSERT_EQ(loaded.links.size(), 2u);
    EXPECT_EQ(std::get<Sensor>(nodeWithId(loaded, 1).data).channel, "pm2_5");
    EXPECT_EQ(std::get<Remap>(nodeWithId(loaded, 2).data).outMax.value, 4.0f);
    EXPECT_EQ(std::get<FloatVariable>(nodeWithId(loaded, 4).data).variableId, 3u);
    EXPECT_EQ(loaded.links[0].targetId, original.links[0].targetId);
}

TEST(ParticleGraphJson, LinksAreStoredByPinNameNotByFieldIndex) {
    // A field index changes when a node type gains a field; the name does not.
    const nlohmann::json saved = sampleGraph();

    const nlohmann::json& firstLink = saved.at("links").at(0);
    EXPECT_EQ(firstLink.at("sourcePin"), "value");
    EXPECT_EQ(firstLink.at("targetPin"), "value");
    EXPECT_EQ(saved.at("links").at(1).at("targetPin"), "size");
}

TEST(ParticleGraphJson, PinNamesAreTurnedBackIntoTheRightPins) {
    const nlohmann::json saved = nlohmann::json::parse(R"({
        "nodes": [ { "id": 1, "type": "FloatVariable", "fields": {} },
                   { "id": 2, "type": "Lerp", "fields": {} } ],
        "links": [ { "id": 1, "sourceNode": 1, "sourcePin": "out", "targetNode": 2, "targetPin": "t" } ]
    })");

    const ParticleGraph graph = saved.get<ParticleGraph>();

    ASSERT_EQ(graph.links.size(), 1u);
    EXPECT_EQ(graph.links[0].targetId, pin(nodeWithId(graph, 2), "t"));
    EXPECT_EQ(graph.links[0].sourceId, pin(nodeWithId(graph, 1), "out"));
}

TEST(ParticleGraphJson, OutputsAreNotSaved) {
    // They are recomputed every frame; saving them would only make the file lie after a change.
    const nlohmann::json saved = sampleGraph();

    const nlohmann::json& remapFields = saved.at("nodes").at(1).at("fields");
    EXPECT_FALSE(remapFields.contains("result"));
    EXPECT_TRUE(remapFields.contains("inMax"));
}

TEST(ParticleGraphJson, NodeOfARemovedTypeIsSkippedTogetherWithItsLinks) {
    const nlohmann::json saved = nlohmann::json::parse(R"({
        "nodes": [ { "id": 1, "type": "Teleporter", "fields": {} },
                   { "id": 2, "type": "Sin", "fields": {} },
                   { "id": 3, "type": "Abs", "fields": {} } ],
        "links": [ { "id": 1, "sourceNode": 1, "sourcePin": "out", "targetNode": 2, "targetPin": "value" },
                   { "id": 2, "sourceNode": 2, "sourcePin": "result", "targetNode": 3, "targetPin": "value" } ]
    })");

    LogCapture log;
    const ParticleGraph graph = saved.get<ParticleGraph>();

    EXPECT_EQ(graph.nodes.size(), 2u);
    ASSERT_EQ(graph.links.size(), 1u);
    EXPECT_EQ(graph.links[0].id, 2u);
    EXPECT_TRUE(log.contains("unknown node type 'Teleporter'")) << log.text();
}

TEST(ParticleGraphJson, LinkToARemovedPinIsDropped) {
    const nlohmann::json saved = nlohmann::json::parse(R"({
        "nodes": [ { "id": 1, "type": "Sin", "fields": {} }, { "id": 2, "type": "Abs", "fields": {} } ],
        "links": [ { "id": 1, "sourceNode": 1, "sourcePin": "result", "targetNode": 2, "targetPin": "gone" } ]
    })");

    LogCapture log;
    const ParticleGraph graph = saved.get<ParticleGraph>();

    EXPECT_TRUE(graph.links.empty());
    EXPECT_TRUE(log.contains("no longer exists")) << log.text();
}

TEST(ParticleGraphJson, MissingFieldsKeepTheirDefaultAndUnknownOnesAreIgnored) {
    const nlohmann::json saved = nlohmann::json::parse(R"({
        "nodes": [ { "id": 1, "type": "Remap", "fields": { "inMax": 5.0, "removedField": 1 } } ]
    })");

    const ParticleGraph graph = saved.get<ParticleGraph>();

    const Remap& remap = std::get<Remap>(nodeWithId(graph, 1).data);
    EXPECT_EQ(remap.inMax.value, 5.0f);
    EXPECT_EQ(remap.inMin.value, Remap{}.inMin.value);
    EXPECT_EQ(remap.outMax.value, Remap{}.outMax.value);
}

TEST(ParticleGraphJson, FieldOfTheWrongTypeKeepsItsDefaultAndWarns) {
    // One bad value must not cost the user the whole graph.
    const nlohmann::json saved = nlohmann::json::parse(R"({
        "nodes": [ { "id": 1, "type": "Remap", "fields": { "inMax": "fifty", "outMax": 3.0 } } ]
    })");

    LogCapture log;
    const ParticleGraph graph = saved.get<ParticleGraph>();

    const Remap& remap = std::get<Remap>(nodeWithId(graph, 1).data);
    EXPECT_EQ(remap.inMax.value, Remap{}.inMax.value);
    EXPECT_EQ(remap.outMax.value, 3.0f);
    EXPECT_TRUE(log.contains("field 'inMax' could not be read")) << log.text();
}

TEST(ParticleGraphJson, IdCountersStayAheadOfEveryLoadedId) {
    // A hand-edited file with stale counters must not make the next node reuse an existing ID.
    const nlohmann::json saved = nlohmann::json::parse(R"({
        "nextNodeId": 1, "nextLinkId": 1,
        "nodes": [ { "id": 12, "type": "Sin", "fields": {} }, { "id": 4, "type": "Abs", "fields": {} } ],
        "links": [ { "id": 30, "sourceNode": 12, "sourcePin": "result", "targetNode": 4, "targetPin": "value" } ]
    })");

    const ParticleGraph graph = saved.get<ParticleGraph>();

    EXPECT_EQ(graph.nextNodeId, 13u);
    EXPECT_EQ(graph.nextLinkId, 31u);
}

TEST(ParticleGraphJson, GraphFromANewerVersionLoadsWhatItKnowsAndWarns) {
    const nlohmann::json saved = nlohmann::json::parse(R"({
        "version": 99, "nodes": [ { "id": 1, "type": "Sin", "fields": {} } ]
    })");

    LogCapture log;
    const ParticleGraph graph = saved.get<ParticleGraph>();

    EXPECT_EQ(graph.nodes.size(), 1u);
    EXPECT_TRUE(log.contains("format version 99")) << log.text();
}

// ----------------------------------------------------------------------------------------------
// GraphVariable / ParticleGroup / ParticleSystem
// ----------------------------------------------------------------------------------------------

TEST(ParticleSystemJson, VariablesKeepTheirType) {
    // An int variable must not come back as a float: bound nodes only accept their own type.
    const std::vector<GraphVariable> variables = {
        { 1, "count", GraphValue(i32(7)) },
        { 2, "speed", GraphValue(1.5f) },
        { 3, "enabled", GraphValue(true) },
        { 4, "offset", GraphValue(vec2(1.0f, 2.0f)) },
        { 5, "color", GraphValue(vec3(0.1f, 0.2f, 0.3f)) },
    };

    for (const GraphVariable& variable : variables) {
        SCOPED_TRACE(variable.name);
        const GraphVariable loaded = nlohmann::json(variable).get<GraphVariable>();
        EXPECT_EQ(loaded.value.index(), variable.value.index());
        EXPECT_EQ(loaded.value, variable.value);
        EXPECT_EQ(loaded.id, variable.id);
    }
}

TEST(ParticleSystemJson, VariableOfAnUnknownTypeIsAnError) {
    const nlohmann::json saved = nlohmann::json::parse(R"({ "id": 1, "name": "q", "type": "quaternion", "value": 0 })");

    EXPECT_ANY_THROW((void)saved.get<GraphVariable>());
}

TEST(ParticleSystemJson, GroupFromBeforeBoxExtentsKeepsTheDefault) {
    const nlohmann::json saved = nlohmann::json::parse(R"({
        "groupName": "Smoke", "particleCount": 10, "size": 1.0, "velocity": [0, 1, 0],
        "particleColor": [1, 1, 1], "minLifeTime": 2.0
    })");

    const ParticleGroup group = saved.get<ParticleGroup>();

    EXPECT_EQ(group.groupName, "Smoke");
    EXPECT_EQ(group.boxExtents, ParticleGroup{}.boxExtents);
    EXPECT_TRUE(group.graph.nodes.empty());
}

TEST(ParticleSystemJson, VariablesOfOldScenesMoveFromTheGroupsToTheSystem) {
    // Variables used to belong to single groups; now the system owns them for all its graphs.
    const nlohmann::json saved = nlohmann::json::parse(R"({
        "nextParticleGroupID": 3, "useParticleGroups": true,
        "particleGroups": [
            { "groupName": "A", "particleCount": 1, "size": 1, "velocity": [0,0,0], "particleColor": [1,1,1],
              "minLifeTime": 1, "id": 1,
              "graphVariables": [ { "name": "speed", "type": "float", "value": 2.0 } ] },
            { "groupName": "B", "particleCount": 1, "size": 1, "velocity": [0,0,0], "particleColor": [1,1,1],
              "minLifeTime": 1, "id": 2,
              "graphVariables": [ { "name": "count", "type": "int", "value": 5 } ] }
        ]
    })");

    const ParticleSystem system = saved.get<ParticleSystem>();

    ASSERT_EQ(system.graphVariables.size(), 2u);
    EXPECT_EQ(system.graphVariables[0].name, "speed");
    EXPECT_EQ(system.graphVariables[1].name, "count");
    // Saved without IDs: each gets a fresh one, distinct and non-zero.
    EXPECT_NE(system.graphVariables[0].id, 0u);
    EXPECT_NE(system.graphVariables[1].id, 0u);
    EXPECT_NE(system.graphVariables[0].id, system.graphVariables[1].id);
}

TEST(ParticleSystemJson, MissingOrDuplicateGroupIdsAreReassigned) {
    // The editor remembers the selected group by ID, so every ID has to be unique and non-zero.
    const nlohmann::json saved = nlohmann::json::parse(R"({
        "nextParticleGroupID": 1, "useParticleGroups": true,
        "particleGroups": [
            { "groupName": "A", "particleCount": 1, "size": 1, "velocity": [0,0,0], "particleColor": [1,1,1], "minLifeTime": 1 },
            { "groupName": "B", "particleCount": 1, "size": 1, "velocity": [0,0,0], "particleColor": [1,1,1], "minLifeTime": 1, "id": 5 },
            { "groupName": "C", "particleCount": 1, "size": 1, "velocity": [0,0,0], "particleColor": [1,1,1], "minLifeTime": 1, "id": 5 }
        ]
    })");

    ParticleSystem system = saved.get<ParticleSystem>();

    std::vector<u32> ids;
    for (const ParticleGroup& group : system.allParticleGroups()) {
        ids.push_back(group.id);
    }
    EXPECT_EQ(std::ranges::count(ids, 0u), 0);
    std::ranges::sort(ids);
    EXPECT_EQ(std::ranges::adjacent_find(ids), ids.end()) << "duplicate group id";
    EXPECT_GT(system.nextParticleGroupID, ids.back());
}

TEST(ParticleSystemJson, NextVariableIdStaysAheadOfLoadedIds) {
    const nlohmann::json saved = nlohmann::json::parse(R"({
        "nextParticleGroupID": 2, "useParticleGroups": false, "nextVariableId": 1,
        "particleGroups": [ { "groupName": "A", "particleCount": 1, "size": 1, "velocity": [0,0,0],
                              "particleColor": [1,1,1], "minLifeTime": 1, "id": 1 } ],
        "graphVariables": [ { "id": 8, "name": "speed", "type": "float", "value": 1.0 } ]
    })");

    const ParticleSystem system = saved.get<ParticleSystem>();

    EXPECT_GT(system.nextVariableId, 8u);
}
