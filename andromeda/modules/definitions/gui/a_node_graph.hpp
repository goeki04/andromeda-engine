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
#include <string_view>
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

    /**
     * @brief A connection between an output pin and an input pin.
     * @details Stores plain numbers, not ed::LinkId / ed::PinId: this data module must not depend on
     *          the node editor. The GUI converts them, e.g. ed::PinId(link.sourceId).
     */
    struct PinLink {
        u32 id = 0;       ///< Editor link ID; 0 is reserved ("no link") by imgui-node-editor.
        u64 sourceId = 0; ///< Pin ID of the output the link starts at (see makePinId).
        u64 targetId = 0; ///< Pin ID of the input the link ends at.
    };

    /** @brief The node graph of one particle group: every NodeInstance in it and the ID counter. */
    struct ParticleGraph {
        u32 nextNodeId = 1;           ///< Next free node ID. Never reused, so node and pin IDs stay stable
        u32 nextLinkId = 1;
        std::vector<NodeInstance> nodes; ///< Nodes in creation order.
        std::vector<PinLink> links; ///< Links in creation order.
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

    /** @brief Appends a node holding @p data at @p position and returns it. */
    inline NodeInstance& addNode(ParticleGraph& graph, NodeData data, vec2 position) {
        NodeInstance& node = graph.nodes.emplace_back();
        node.id = graph.nextNodeId++;
        node.position = position;
        node.data = std::move(data);
        return node;
    }

    /** @brief Appends a default-constructed node of type @p typeIndex at @p position and returns it. */
    inline NodeInstance& addNode(ParticleGraph& graph, size_t typeIndex, vec2 position) {
        return addNode(graph, makeNodeData(typeIndex), position);
    }

    /** @brief The node with @p nodeId, or nullptr. */
    inline const NodeInstance* findNode(const ParticleGraph& graph, u32 nodeId) {
        for (const NodeInstance& node : graph.nodes) {
            if (node.id == nodeId)
                return &node;
        }
        return nullptr;
    }

    /** @brief Type name of the node in @p data, e.g. "AddNode". Variant order equals ReflectedNodesNames order. */
    inline std::string_view nodeTypeName(const NodeData& data) {
        return Meta::ReflectedNodesNames[data.index()];
    }

    /** @brief Variant index of the node type called @p name, or -1 if no such type exists (any more). */
    inline i32 nodeTypeIndex(std::string_view name) {
        for (size_t i = 0; i < Meta::ReflectedNodesNames.size(); ++i) {
            if (Meta::ReflectedNodesNames[i] == name)
                return static_cast<i32>(i);
        }
        return -1;
    }

    /** @brief Name of field @p fieldIndex of the node in @p data, or "" if there is no such field. */
    inline std::string_view fieldNameAt(const NodeData& data, u32 fieldIndex) {
        return std::visit([fieldIndex](const auto& node) -> std::string_view {
            constexpr auto& names = Meta::StructInfo<std::decay_t<decltype(node)>>::fieldNames;
            return fieldIndex < names.size() ? names[fieldIndex] : std::string_view{};
        }, data);
    }

    /** @brief Index of the field called @p name in the node in @p data, or -1 if it has none by that name. */
    inline i32 fieldIndexOf(const NodeData& data, std::string_view name) {
        return std::visit([name](const auto& node) -> i32 {
            constexpr auto& names = Meta::StructInfo<std::decay_t<decltype(node)>>::fieldNames;
            for (size_t i = 0; i < names.size(); ++i) {
                if (names[i] == name)
                    return static_cast<i32>(i);
            }
            return -1;
        }, data);
    }

} // namespace Andromeda
