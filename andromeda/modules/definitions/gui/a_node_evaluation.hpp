#pragma once

/**
 * @file a_node_evaluation.hpp
 * @brief Runs a particle node graph: what each node computes, and in which order the nodes run.
 *
 * @details Data only, no editor: the graph must also run with the panel closed. The node types
 *          themselves are the [[ParticleNode]] structs in a_Nodes.hpp; every one of them needs an
 *          evaluateNode() overload here, otherwise the build stops on the fallback's static_assert.
 */

#include "a_particle_group.hpp"
#include <variant>
#include <span>
#include <unordered_map>
#include <vector>
#include "a_Primitives.hpp"
#include "a_node_pins.hpp"
#include "a_Nodes.hpp"
namespace Andromeda {

    /**
     * @brief Everything a graph may read besides its own nodes. Grows with the node types.
     * @details Passed in rather than fetched: this header is data only and must not reach into the
     *          engine's subsystems. A node that needs one of these takes the context as a second
     *          parameter of its evaluateNode() overload; evaluateGraph() picks that form when it exists.
     */
    struct GraphContext {
        std::span<const GraphVariable> variables; ///< The variables of the node's ParticleSystem.
        float time = 0.0f;      ///< Seconds the scene has been running; stands still while paused.
        float deltaTime = 0.0f; ///< Seconds since the last frame; 0 while paused.
    };

    // NodeValue, canConnect() and readAs() live in a_node_pins.hpp, next to ValueType.

    // What a node computes: reads its Input and Param fields, writes its Output fields. The graph takes
    // care of moving values between nodes, so an overload never looks at links or at other nodes.

    /** @brief Fallback: a node type without its own rule is a mistake, not a node that does nothing. */
    template<typename T>
    void evaluateNode(T& node) {
        static_assert(sizeof(T) == 0, "evaluateNode not implemented for this type");
    }

    inline void evaluateNode(Gui::Node::AddNode& node) {
        node.result.value = node.a.value + node.b.value;
    }

    inline void evaluateNode(Gui::Node::Subtract& node) {
        node.result.value = node.a.value - node.b.value;
    }

    inline void evaluateNode(Gui::Node::Multiply& node) {
        node.result.value = node.a.value * node.b.value;
    }

    inline void evaluateNode(Gui::Node::Divide& node) {
        // Dividing by zero would put inf or NaN into the particles, and NaN never washes out again.
        if (node.b.value != 0.0f)
            node.result.value = node.a.value / node.b.value;
    }

    inline void evaluateNode(Gui::Node::Lerp& node) {
        node.result.value = std::lerp(node.a.value, node.b.value, node.t.value);
    }

    inline void evaluateNode(Gui::Node::Clamp& node) {
        const float low = std::min(node.min.value, node.max.value);
        const float high = std::max(node.min.value, node.max.value);
        node.result.value = std::clamp(node.value.value, low, high);
    }

    inline void evaluateNode(Gui::Node::Time& node, const GraphContext& context) {
        node.seconds.value = context.time;
        node.deltaTime.value = context.deltaTime;
    }

    /** @brief End of the chain: computes nothing, evaluateGraph() copies its inputs into the group. */
    inline void evaluateNode(Gui::Node::OutputNode& node) { }

    /** @brief A node that just forwards its param to its output, like the variable nodes. */
    template<typename T>
    concept VariableNode = requires(T& n) { n.out.value = n.value.value; };

    template<VariableNode T>
    void evaluateNode(T& node) {
        node.out.value = node.value.value;
    }

    /** @brief The variable with @p id, or nullptr: id 0 means unbound, and a bound one may be deleted. */
    inline const GraphVariable* findVariable(std::span<const GraphVariable> variables, u32 id) {
            if (id == 0)
                return nullptr;
            for (const GraphVariable& variable : variables) {
                if (variable.id == id)
                    return &variable;
            }
            return nullptr;
    }

    /**
     * @brief Copies the value of the bound GraphVariable into a variable node, if it is bound to one.
     * @details The variables live in the ParticleSystem, the node only stores their ID. In the editor
     *          the panel keeps both sides in sync while drawing; evaluation must not rely on that, or a
     *          graph would run on stale values whenever the panel is closed.
     *          Nodes without a variableId, an unbound node (id 0), a deleted variable or one whose type no
     *          longer matches the node keep the node's own value.
     * @note Call once per node right before evaluateNode(), so later nodes see the current value.
     */
    inline void applyBoundVariable(NodeInstance& node, std::span<const GraphVariable> variables) {
        std::visit(
            [variables](auto& data) {
                if constexpr (requires { data.variableId; data.value.value; }) {
                    const GraphVariable* variable = findVariable(variables, data.variableId);
                    if (variable == nullptr)
                        return;
                    using ValueT = std::decay_t<decltype(data.value.value)>;
                    if (const ValueT* bound = std::get_if<ValueT>(&variable->value))
                        data.value.value = *bound;
                }
            },
            node.data);
    }
    /* checks if the node with the given ID exists in the graph and returns the index*/
    inline i32 nodeIndexOf(const ParticleGraph& graph, u32 nodeId) {
        for (size_t i = 0; i < graph.nodes.size(); i++) {
            if (graph.nodes[i].id == nodeId) {
                return static_cast<i32>(i);
            }
        }
        return -1;
    }

    /**
     * @brief Runs @p graph once and writes the result into @p group.
     * @details A node may only run once every node feeding it has run, so the nodes are first sorted
     *          into a running order (Kahn's algorithm) and then evaluated in that order. Everything in
     *          here works on positions in graph.nodes, not on node IDs: IDs are never reused and so have
     *          gaps, positions are 0 .. n-1 and can index a plain vector.
     * @param context Everything the nodes may read besides the graph itself: the system's variables
     *        (see applyBoundVariable) and the frame's time. Nodes that need it get an evaluateNode()
     *        overload taking it as a second parameter.
     */
    inline void evaluateGraph(ParticleGraph& graph, ParticleGroup& group, const GraphContext& context) {

        size_t size = graph.nodes.size();
        
        std::unordered_map<u64, NodeValue> input;     // PinId -> value
        std::unordered_map<u64, u64> incomingLink;    
        std::vector<std::vector<u32>> outgoing(size); // node -> the nodes it feeds
        std::vector<u32> incomingCount(size);         // node -> how many of its inputs a link fills
        std::vector<u32> order;                       // nodes in running order
        std::vector<u32> queue;                       // nodes whose inputs are all ready
        queue.reserve(size);
        order.reserve(size);

        // Both ends of every link, as positions. A link whose node is gone (deleted, hand-edited file)
        // is skipped whole: counting it without recording the edge would leave a node waiting forever.
        for (size_t i = 0; i < graph.links.size(); i++) {
            auto& link = graph.links[i];

            const i32 target = nodeIndexOf(graph, decodePinId(link.targetId).nodeId);
            const i32 source = nodeIndexOf(graph, decodePinId(link.sourceId).nodeId);
            if (target < 0 || source < 0) {
                continue;
            }
            incomingLink[link.targetId] = link.sourceId;
            incomingCount[target]++;
            outgoing[source].push_back(target);
        }

        // Nodes that wait for nobody start the order: variables, and anything with unconnected inputs.
        for (size_t i = 0; i < size; i++) {
            if (incomingCount[i] == 0) {
                queue.push_back(static_cast<u32>(i));
            }
        }

        // The queue keeps growing while it is worked off: a node joins it the moment its last
        // supplier is done, which is exactly when its counter reaches zero.
        while (!queue.empty()) {
            auto index = queue.back();
            queue.pop_back();
            order.push_back(index);
            for (auto target : outgoing[index]) {
                incomingCount[target]--;
                if (incomingCount[target] == 0) {
                    queue.push_back(target);
                }
            }
        }

        if (order.size() < graph.nodes.size())
            return; // cycle: some counters never reached zero

        for (size_t i = 0; i < order.size(); i++) {
            NodeInstance& node = graph.nodes[order[i]];
            applyBoundVariable(node, context.variables);
            u32 fieldIndex = 0;
            std::visit([&](auto& data) { 
                Meta::forEachField(data, [&](auto const&, auto& member) { 
                    using PinT = std::decay_t<decltype(member)>;
                        u64 pinId = encodePinId(node.id, fieldIndex);
                        if constexpr (pinRole<PinT> == PinRole::Input) {
                            const auto link = incomingLink.find(pinId);
                            if (link != incomingLink.end()) {
                                const auto value = input.find(link->second);
                                if (value != input.end()) {
                                    // readAs applies the same conversions canConnect() allows, so every
                                    // link the editor let the user draw really delivers its value.
                                    readAs(value->second, member.value);
                                }
                            }
                        }
                        ++fieldIndex;
                    });

                if constexpr (requires { evaluateNode(data, context); })
                    evaluateNode(data, context);
                else
                    evaluateNode(data);

                if constexpr (std::is_same_v<std::decay_t<decltype(data)>, Gui::Node::OutputNode>) {
                    // Only connected inputs are written: an unconnected one would push its fallback
                    // (0) over the value the user typed in the details panel.
                    const auto isConnected = [&](std::string_view fieldName) {
                        const i32 field = fieldIndexOf(node.data, fieldName);
                        return field >= 0 && incomingLink.contains(encodePinId(node.id, static_cast<u32>(field)));
                    };

                    if (isConnected("size"))
                        group.size = data.size.value;
                    if (isConnected("particleCount"))
                        group.particleCount = data.particleCount.value;
                    if (isConnected("velocity"))
                        group.velocity = data.velocity.value;
                    if (isConnected("particleColor"))
                        group.particleColor = data.particleColor.value;
                    if (isConnected("minLifetime"))
                        group.minLifetime = data.minLifetime.value;
                }
                fieldIndex = 0;
                Meta::forEachField(data, [&](auto const&, auto& member) {
                        using PinT = std::decay_t<decltype(member)>;
                        if constexpr (pinRole<PinT> == PinRole::Output) {
                            input[encodePinId(node.id, fieldIndex)] = member.value;
                        }
                        ++fieldIndex;
                    });
                }, node.data);
        }
    }
}
