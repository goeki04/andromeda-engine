#pragma once

/**
 * @file a_Node_PinQuery.hpp
 * @brief Looking up what is behind an editor pin: its node, its role and the type it carries.
 *
 * @details A pin ID is node ID plus field index (see encodePinId in a_node_graph.hpp). Everything here
 *          turns such an ID back into the field it names, by walking the node's reflected fields.
 */

#include <algorithm>
#include <type_traits>
#include "imgui_node_editor.h"
#include "a_meta_core.hpp"
#include "a_node_graph.hpp"
#include "generated_node_meta.hpp"

namespace Andromeda::Gui::Node {
    namespace ed = ax::NodeEditor;

    using PinInfo = PinAddress;

    /** @brief Node and field of an editor pin; the bit layout lives in decodePinId (a_node_graph.hpp). */
    inline PinInfo getPinInfo(ed::PinId pinId) {
        return decodePinId(pinId.Get());
    }

    /** @brief Editor pin of field @p fieldIndex of node @p nodeId; the bit layout lives in encodePinId. */
    inline ed::PinId makePinId(u32 nodeId, u32 fieldIndex) {
        return ed::PinId(encodePinId(nodeId, fieldIndex));
    }

    inline NodeInstance* getNodeByPinId(ed::PinId pinId, ParticleGraph &graph) {
        PinInfo info = getPinInfo(pinId);
        auto it = std::find_if(graph.nodes.begin(), graph.nodes.end(), [info](const NodeInstance& node) {
            return node.id == info.nodeId;
        });
        if (it != graph.nodes.end()) {
            return &*it;
        }
        return nullptr;
    }

    inline ValueType getPinValueType(ParticleGraph& graph, ed::PinId pinId) {
        auto node = getNodeByPinId(pinId, graph);
        if (node == nullptr) {
            return ValueType::None;
        }
        const u32 searchedIndex = getPinInfo(pinId).fieldIndex;
        ValueType type = ValueType::None;
        std::visit([&](auto& data) {
                u32 index = 0;
                Meta::forEachField(data, [&](auto const&, auto& member) {
                    if (index == searchedIndex) {
                        using PinT = std::decay_t<decltype(member)>; // e.x. Input<float>, Output<vec3>, Param<i32>
                        using valueT = typename PinTraits<PinT>::value_type; // just the inner type, e.x. float, vec3, i32
                        type = valueTypeOf<valueT>;
                    }
                    ++index;
                });
            }, node->data);
        return type;
    }

    // canConnect() lives in a_node_pins.hpp, next to ValueType and readAs(): the editor and the
    // evaluation have to agree on which links are allowed, so the rule exists only once.

    inline PinRole getPinRole(ParticleGraph& graph, ed::PinId pinId) {
        auto node = getNodeByPinId(pinId, graph);
        if (node == nullptr) {
            return PinRole::None;
        }
        const u32 searchedIndex = getPinInfo(pinId).fieldIndex;
        PinRole role = PinRole::None;
        std::visit([&](auto& data) {
                u32 index = 0;
                Meta::forEachField(data, [&](auto const&, auto& member) {
                    if (index == searchedIndex) {
                        role = pinRole<std::decay_t<decltype(member)>>;
                    }
                    ++index;
                });
            }, node->data);
        return role;
    }

    /**
     * @brief Field index of the first pin with @p role in @p node, or -1 if it has none.
     * @details Same field walk as getPinRole, searching by role instead of by index.
     */
    inline i32 findFirstPin(ValueType other,const NodeInstance& node, PinRole role) {
        i32 found = -1;
        std::visit([&](const auto& data) {
            i32 index = 0;
            Meta::forEachField(data, [&](auto const&, const auto& member) {
                using PinT = std::decay_t<decltype(member)>;
                using ValueT = typename PinTraits<PinT>::value_type;
                const ValueType memberType = valueTypeOf<ValueT>;
                bool hasWantedRole = pinRole<std::decay_t<decltype(member)>> == role;
                bool hasMatchingType =
                    role == PinRole::Input
                        ? canConnect(other, memberType)
                        : canConnect(memberType, other);
                if (found < 0 && hasWantedRole && hasMatchingType)
                    found = index;
                ++index;
            });
        }, node.data);
        return found;
    }
}
