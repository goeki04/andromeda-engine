#pragma once

/**
 * @file a_Node_Links.hpp
 * @brief Drawing links and everything that changes them: connecting, deleting nodes and links.
 *
 * @details The editor only knows what it drew this frame; the graph data is what gets saved and
 *          evaluated. So every accepted editor action is mirrored into the ParticleGraph here.
 */

#include <algorithm>
#include "imgui_node_editor.h"
#include "a_Node_PinQuery.hpp"
#include "a_node_graph.hpp"

namespace Andromeda::Gui::Node {

    inline void drawLink(ParticleGraph& graph) {
        for (const auto& link : graph.links) {
            ed::Link(ed::LinkId(link.id),ed::PinId(link.sourceId),ed::PinId(link.targetId));
        }
    }

    /**
     * @brief Stores a link from output @p source to input @p target.
     * @details An input has at most one link, so an existing link into @p target is replaced - which is
     *          what lets the user re-plug an input by simply dragging a new link onto it.
     */
    inline void addLink(ParticleGraph& graph, ed::PinId source, ed::PinId target) {
        std::erase_if(graph.links, [target](const PinLink& link) { return link.targetId == target.Get(); });
        graph.links.emplace_back(PinLink{graph.nextLinkId, source.Get(), target.Get()});
        graph.nextLinkId++;
    }

    /**
     * @brief Handles dragging links: connects pins, and reports a link dropped on empty canvas.
     * @return The pin a link was dragged from and released over empty space - the caller opens the
     *         "Add Node" popup and connects the new node to it. An invalid PinId otherwise.
     * @note Call between ed::Begin() and ed::End(), after the nodes were drawn.
     */
    inline ed::PinId createLink(ParticleGraph& graph) {
        ed::PinId droppedFrom;

        if (ed::BeginCreate()) {
            ed::PinId source;
            ed::PinId target;

            if (ed::QueryNewLink(&source, &target)) {
                PinRole sourceRole = getPinRole(graph, source);
                PinRole targetRole = getPinRole(graph, target);
                bool isSameNode = getPinInfo(source).nodeId == getPinInfo(target).nodeId;

                if (sourceRole == PinRole::Input && targetRole == PinRole::Output) {
                    std::swap(source, target);
                    std::swap(sourceRole, targetRole);
                }

                bool hasValidRoles = sourceRole == PinRole::Output && targetRole == PinRole::Input;
                bool hasValidTypes = canConnect(getPinValueType(graph, source), getPinValueType(graph, target));
                if (hasValidRoles && hasValidTypes && !isSameNode) {
                    if (ed::AcceptNewItem())
                        addLink(graph, source, target);
                } else {
                    ed::RejectNewItem();
                }
            }

            // Same drag, but released over empty canvas instead of over a pin.
            ed::PinId pin;
            if (ed::QueryNewNode(&pin)) {
                if (ed::AcceptNewItem())
                    droppedFrom = pin;
            }
        }
        ed::EndCreate();

        return droppedFrom;
    }

    /**
     * @brief Links @p fromPin to the first matching pin of @p newNode: an output to its first input,
     *        an input to its first output. Does nothing if the node has no such pin or @p fromPin's
     *        node was deleted in the meantime.
     */
    inline void connectToNewNode(ParticleGraph& graph, ed::PinId fromPin, const NodeInstance& newNode) {
        const PinRole fromRole = getPinRole(graph, fromPin);
        if (fromRole != PinRole::Input && fromRole != PinRole::Output)
            return;

        const PinRole wantedRole = fromRole == PinRole::Output ? PinRole::Input : PinRole::Output;
        const i32 fieldIndex = findFirstPin(getPinValueType(graph, fromPin), newNode, wantedRole);
        if (fieldIndex < 0)
            return;

        const ed::PinId newPin = makePinId(newNode.id, static_cast<u32>(fieldIndex));
        if (fromRole == PinRole::Output)
            addLink(graph, fromPin, newPin);
        else
            addLink(graph, newPin, fromPin);
    }

    /** @brief Removes a node and every link attached to it from the graph data. */
    inline void removeNode(ParticleGraph& graph, u32 nodeId) {
        std::erase_if(graph.nodes, [nodeId](const NodeInstance& node) { return node.id == nodeId; });

        // The editor reports a deleted node's links as well, but only the ones it drew. The graph data
        // is what gets saved and evaluated, so it must never keep a link to a node that is gone.
        std::erase_if(graph.links, [nodeId](const PinLink& link) {
            return getPinInfo(ed::PinId(link.sourceId)).nodeId == nodeId ||
                   getPinInfo(ed::PinId(link.targetId)).nodeId == nodeId;
        });
    }

    /**
     * @brief Handles the editor's delete action (Delete key on a selection) for nodes and links.
     * @details Nodes are queried first: accepting a node makes the editor append that node's links to
     *          the items still to delete, so the link loop afterwards reports them too. Querying links
     *          first would miss them.
     * @note Call between ed::Begin() and ed::End(), after the nodes and links were drawn.
     */
    inline void deleteSelection(ParticleGraph& graph) {
        if (ed::BeginDelete()) {
            ed::NodeId deletedNodeId;
            while (ed::QueryDeletedNode(&deletedNodeId)) {
                if (ed::AcceptDeletedItem())
                    removeNode(graph, static_cast<u32>(deletedNodeId.Get()));
            }

            ed::LinkId deletedLinkId;
            while (ed::QueryDeletedLink(&deletedLinkId)) {
                if (ed::AcceptDeletedItem()) {
                    const u32 linkId = static_cast<u32>(deletedLinkId.Get());
                    std::erase_if(graph.links, [linkId](const PinLink& link) { return link.id == linkId; });
                }
            }
        }
        ed::EndDelete();
    }
}
