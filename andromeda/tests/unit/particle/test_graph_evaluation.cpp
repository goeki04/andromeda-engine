// Running a particle graph: values travel along links, nodes run in dependency order, and the guards
// that keep a broken graph or a bad value from reaching the particles. Plain arithmetic of single
// nodes (Add, Sin, ...) is not tested on purpose - there is nothing in it that could break quietly.

#include <gtest/gtest.h>
#include <cmath>
#include <string_view>
#include <vector>
#include "a_node_evaluation.hpp"

using namespace Andromeda;
using namespace Andromeda::Gui::Node;

namespace {
    /** @brief Builds a graph by node ID, so tests read like the wiring they describe. */
    class GraphBuilder {
    public:
        template<typename NodeT>
        u32 add(NodeT node) {
            return addNode(m_Graph, std::move(node), vec2(0.0f)).id;
        }

        void connect(u32 from, std::string_view output, u32 to, std::string_view input) {
            m_Graph.links.push_back(PinLink{ m_Graph.nextLinkId++, pinOf(from, output), pinOf(to, input) });
        }

        /** @brief A link whose source node does not exist (deleted, or a hand-edited file). */
        void connectFromMissingNode(u32 to, std::string_view input) {
            m_Graph.links.push_back(PinLink{ m_Graph.nextLinkId++, encodePinId(999, 0), pinOf(to, input) });
        }

        template<typename NodeT>
        NodeT& node(u32 id) {
            for (NodeInstance& instance : m_Graph.nodes) {
                if (instance.id == id)
                    return std::get<NodeT>(instance.data);
            }
            throw std::out_of_range("no such node");
        }

        ParticleGroup evaluate(const GraphContext& context = {}, ParticleGroup group = {}) {
            evaluateGraph(m_Graph, group, context);
            return group;
        }

    private:
        u64 pinOf(u32 nodeId, std::string_view field) const {
            const NodeInstance* node = findNode(m_Graph, nodeId);
            const i32 index = node ? fieldIndexOf(node->data, field) : -1;
            EXPECT_GE(index, 0) << "node " << nodeId << " has no field '" << field << "'";
            return encodePinId(nodeId, static_cast<u32>(index));
        }

        ParticleGraph m_Graph;
    };

    FloatVariable floatVariable(float value, u32 variableId = 0) {
        FloatVariable node;
        node.value = value;
        node.variableId = variableId;
        return node;
    }
}

TEST(GraphEvaluation, SensorValueTravelsThroughTheChainIntoTheGroup) {
    GraphBuilder graph;
    Sensor sensor;
    sensor.channel = "pm2_5";
    Remap remap;
    remap.inMax = 50.0f;
    remap.outMin = 0.5f;
    remap.outMax = 4.0f;
    const u32 sensorNode = graph.add(sensor);
    const u32 remapNode = graph.add(remap);
    const u32 output = graph.add(OutputNode{});
    graph.connect(sensorNode, "value", remapNode, "value");
    graph.connect(remapNode, "result", output, "size");
    const std::vector<SensorChannel> channels = { { "pm10", 99.0f }, { "pm2_5", 25.0f } };

    const ParticleGroup group = graph.evaluate({ .sensorChannels = channels });

    EXPECT_FLOAT_EQ(group.size, 2.25f);
}

TEST(GraphEvaluation, NodesRunInDependencyOrderNotInCreationOrder) {
    // Created back to front: a loop over graph.nodes in storage order would read the Multiply
    // before its inputs were computed and deliver a stale result.
    GraphBuilder graph;
    const u32 output = graph.add(OutputNode{});
    const u32 multiply = graph.add(Multiply{});
    const u32 three = graph.add(floatVariable(3.0f));
    const u32 two = graph.add(floatVariable(2.0f));
    graph.connect(multiply, "result", output, "size");
    graph.connect(three, "out", multiply, "a");
    graph.connect(two, "out", multiply, "b");

    EXPECT_FLOAT_EQ(graph.evaluate().size, 6.0f);
}

TEST(GraphEvaluation, UnconnectedOutputPinsLeaveTheGroupsValuesAlone) {
    // The user typed size = 7 in the details panel and only drives the particle count by graph.
    GraphBuilder graph;
    IntVariable count;
    count.value = 250;
    const u32 countNode = graph.add(count);
    const u32 output = graph.add(OutputNode{});
    graph.connect(countNode, "out", output, "particleCount");
    ParticleGroup preset;
    preset.size = 7.0f;

    const ParticleGroup group = graph.evaluate({}, preset);

    EXPECT_EQ(group.particleCount, 250);
    EXPECT_FLOAT_EQ(group.size, 7.0f);
}

TEST(GraphEvaluation, CycleLeavesTheGroupUntouched) {
    // There is no valid order for a cycle; evaluating anyway would feed values from the last frame
    // back in and make the particles drift.
    GraphBuilder graph;
    const u32 first = graph.add(AddNode{});
    const u32 second = graph.add(AddNode{});
    const u32 output = graph.add(OutputNode{});
    graph.connect(first, "result", second, "a");
    graph.connect(second, "result", first, "a");
    graph.connect(second, "result", output, "size");
    ParticleGroup preset;
    preset.size = 7.0f;

    EXPECT_FLOAT_EQ(graph.evaluate({}, preset).size, 7.0f);
}

TEST(GraphEvaluation, LinkFromAMissingNodeIsIgnored) {
    // The rest of the graph still runs; the input falls back to its own value.
    GraphBuilder graph;
    const u32 multiply = graph.add(Multiply{});
    graph.node<Multiply>(multiply).a = 4.0f;
    graph.node<Multiply>(multiply).b = 2.0f;
    const u32 output = graph.add(OutputNode{});
    graph.connectFromMissingNode(multiply, "a");
    graph.connect(multiply, "result", output, "size");

    EXPECT_FLOAT_EQ(graph.evaluate().size, 8.0f);
}

TEST(GraphEvaluation, IntOutputFeedsAFloatInput) {
    // canConnect() lets the editor draw int -> float links; evaluation has to deliver them too.
    GraphBuilder graph;
    IntVariable four;
    four.value = 4;
    const u32 intNode = graph.add(four);
    const u32 half = graph.add(floatVariable(0.5f));
    const u32 multiply = graph.add(Multiply{});
    const u32 output = graph.add(OutputNode{});
    graph.connect(intNode, "out", multiply, "a");
    graph.connect(half, "out", multiply, "b");
    graph.connect(multiply, "result", output, "size");

    EXPECT_FLOAT_EQ(graph.evaluate().size, 2.0f);
}

TEST(GraphEvaluation, BoundVariableOverridesTheNodesOwnValue) {
    // With the editor panel closed, nothing syncs node and variable - evaluation must read the variable.
    GraphBuilder graph;
    const u32 variableNode = graph.add(floatVariable(1.0f, 3));
    const u32 output = graph.add(OutputNode{});
    graph.connect(variableNode, "out", output, "size");
    const std::vector<GraphVariable> variables = { { 3, "size", GraphValue(9.0f) } };

    EXPECT_FLOAT_EQ(graph.evaluate({ .variables = variables }).size, 9.0f);
}

TEST(GraphEvaluation, VariableOfAnotherTypeOrDeletedOneKeepsTheNodesValue) {
    GraphBuilder graph;
    const u32 wrongType = graph.add(floatVariable(1.5f, 3)); // variable 3 is an int
    const u32 deleted = graph.add(floatVariable(2.0f, 5));   // variable 5 no longer exists
    const u32 add = graph.add(AddNode{});
    const u32 output = graph.add(OutputNode{});
    graph.connect(wrongType, "out", add, "a");
    graph.connect(deleted, "out", add, "b");
    graph.connect(add, "result", output, "size");
    const std::vector<GraphVariable> variables = { { 3, "count", GraphValue(i32(100)) } };

    EXPECT_FLOAT_EQ(graph.evaluate({ .variables = variables }).size, 3.5f);
}

TEST(GraphEvaluation, MissingSensorChannelKeepsTheLastReading) {
    // No data yet, or a scene made for another sensor: dropping to 0 every frame would make the
    // particles flicker, so the node holds its last value.
    GraphBuilder graph;
    Sensor sensor;
    sensor.channel = "pm2_5";
    const u32 sensorNode = graph.add(sensor);
    const u32 output = graph.add(OutputNode{});
    graph.connect(sensorNode, "value", output, "size");
    const std::vector<SensorChannel> reading = { { "pm2_5", 10.0f } };
    graph.evaluate({ .sensorChannels = reading });

    EXPECT_FLOAT_EQ(graph.evaluate({ .sensorChannels = {} }).size, 10.0f);
}

TEST(GraphEvaluation, DivisionByZeroKeepsThePreviousResultInsteadOfInfinity) {
    GraphBuilder graph;
    const u32 divide = graph.add(Divide{});
    const u32 output = graph.add(OutputNode{});
    graph.connect(divide, "result", output, "size");
    graph.node<Divide>(divide).a = 6.0f;
    graph.node<Divide>(divide).b = 3.0f;
    graph.evaluate();

    graph.node<Divide>(divide).b = 0.0f;
    const float size = graph.evaluate().size;

    EXPECT_TRUE(std::isfinite(size));
    EXPECT_FLOAT_EQ(size, 2.0f);
}

TEST(GraphEvaluation, RemapWithAnEmptyInputRangeGivesOutMinInsteadOfNaN) {
    GraphBuilder graph;
    Remap remap;
    remap.inMin = 5.0f;
    remap.inMax = 5.0f;
    remap.outMin = 1.5f;
    remap.value = 5.0f;
    const u32 remapNode = graph.add(remap);
    const u32 output = graph.add(OutputNode{});
    graph.connect(remapNode, "result", output, "size");

    EXPECT_FLOAT_EQ(graph.evaluate().size, 1.5f);
}

TEST(GraphEvaluation, ClampWithSwappedBoundsStillClamps) {
    // std::clamp with min > max is undefined behaviour; the node sorts the bounds first.
    GraphBuilder graph;
    Clamp clamp;
    clamp.min = 10.0f;
    clamp.max = 0.0f;
    clamp.value = 15.0f;
    const u32 clampNode = graph.add(clamp);
    const u32 output = graph.add(OutputNode{});
    graph.connect(clampNode, "result", output, "size");

    EXPECT_FLOAT_EQ(graph.evaluate().size, 10.0f);
}

TEST(GraphEvaluation, TimeComesFromTheContextSoPausingFreezesIt) {
    GraphBuilder graph;
    const u32 time = graph.add(Time{});
    const u32 output = graph.add(OutputNode{});
    graph.connect(time, "seconds", output, "size");
    graph.connect(time, "deltaTime", output, "minLifetime");

    const ParticleGroup group = graph.evaluate({ .time = 12.5f, .deltaTime = 0.0f });

    EXPECT_FLOAT_EQ(group.size, 12.5f);
    EXPECT_FLOAT_EQ(group.minLifetime, 0.0f);
}

TEST(GraphEvaluation, FloatToIntRoundsToTheNearestWholeNumber) {
    // 2.9 particles should be 3, not 2.
    GraphBuilder graph;
    ToInt toInt;
    toInt.value = 2.9f;
    const u32 toIntNode = graph.add(toInt);
    const u32 output = graph.add(OutputNode{});
    graph.connect(toIntNode, "out", output, "particleCount");

    EXPECT_EQ(graph.evaluate().particleCount, 3);
}
