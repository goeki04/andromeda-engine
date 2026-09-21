#pragma once

/**
 * @file a_node_graph.hpp
 * @brief Runtime data of a particle node graph: which nodes exist, where they are and what they hold.
 *
 * @details The node types themselves are the [[ParticleNode]] structs in a_Nodes.hpp. The generator
 *          lists them in Meta::ReflectedNodes, and NodeData turns that list into a std::variant, so a
 *          new node type becomes storable here without touching this file.
 */

#include <cstddef>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>
#include "a_primitives.hpp"
#include "generated_node_meta.hpp"

namespace Andromeda {

    template<typename Tuple>
    struct TupleToVariant;

    template<typename... Ts>
    struct TupleToVariant<std::tuple<Ts...>> {
        using type = std::variant<Ts...>;
    };

    /** @brief One alternative per [[ParticleNode]] struct, in the order of Meta::ReflectedNodes. */
    using NodeData = typename TupleToVariant<Meta::ReflectedNodes>::type;

    /** @brief One node placed in a ParticleGraph: stable ID, canvas position and the fields of its node type. */
    struct NodeInstance {
        u32 id = 0;                 ///< Editor node ID; 0 is reserved ("no node") by imgui-node-editor.
        vec2 position = vec2(0.0f); ///< Position on the canvas, in canvas units.
        NodeData data;              ///< The concrete node struct (AddNode, FloatVariable, ...).
    };

    /** @brief The node graph of one particle group: every NodeInstance in it and the ID counter. */
    struct ParticleGraph {
        u32 nextNodeId = 1;           ///< Next free node ID. Never reused, so node and pin IDs stay stable.
        std::vector<NodeInstance> nodes; ///< Nodes in creation order.
    };

    namespace Detail {
        template<size_t... I> // I is a list of size_t values
        NodeData makeNodeData(size_t typeIndex,
                              std::index_sequence<I...>) { // fold expression over all indices of NodeData. Index sequence takes Indices like 0,1,2
            NodeData data;
            ((typeIndex == I ? (data.template emplace<I>(), true) : false) ||...); // ... enrolls this expression to all indices in the parameter pack I. If typeIndex matches I,
                                                                                   // emplace that variant alternative.
            return data;
        }
    } // namespace Detail

    /**
     * @brief Default-constructs the node type at @p typeIndex (index into Meta::ReflectedNodes).
     * @details The type is only known at runtime (picked in the editor menu). The fold expression
     *          checks every compile-time index once and constructs the one that matches.
     */
    inline NodeData makeNodeData(size_t typeIndex) {
        return Detail::makeNodeData(typeIndex, std::make_index_sequence<std::variant_size_v<NodeData>>{});
    }

    /** @brief Appends a node of type @p typeIndex at @p position and returns it. */
    inline NodeInstance& addNode(ParticleGraph& graph, size_t typeIndex, vec2 position) {
        NodeInstance& node = graph.nodes.emplace_back();
        node.id = graph.nextNodeId++;
        node.position = position;
        node.data = makeNodeData(typeIndex);
        return node;
    }
} // namespace Andromeda
