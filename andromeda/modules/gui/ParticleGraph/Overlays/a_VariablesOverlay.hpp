#pragma once

/**
 * @file a_VariablesOverlay.hpp
 * @brief The variables overlay of the particle editor, and the binding between variables and their nodes.
 *
 * @details The variables belong to the ParticleSystem and are shared by the graphs of all its groups.
 *          Dragging a row onto the canvas creates a node bound to that variable; from then on the two are
 *          kept equal while drawing (syncFromVariable / writeBackToVariable).
 */

#include <vector>
#include "imgui.h"
#include "imgui_internal.h" // ImRect
#include "a_components.hpp"
#include "a_node_graph.hpp"
#include "a_particle_group.hpp"

namespace Andromeda::Gui::Overlay {

    /** @brief Contents of the variables overlay: menu bar with search and "+", then one card per variable. */
    void drawVariableList(ECS::Component::ParticleSystem& system, ImGuiTextFilter& filter);

    /** @brief The variable node type matching the variable's type, bound to it and holding its current value. */
    NodeData makeNodeForVariable(const GraphVariable& variable);

    /**
     * @brief Brings a node that is bound to a variable in line with it. Call right before drawing the node.
     * @return The bound variable, or nullptr for unbound and non-variable nodes. The pointer points into
     *         @p variables and is only valid until that vector changes.
     */
    GraphVariable* syncFromVariable(NodeInstance& node, std::vector<GraphVariable>& variables);

    /** @brief Writes an edit made in a bound node back to its variable. Call right after drawing the node. */
    void writeBackToVariable(const NodeInstance& node, GraphVariable& variable);

    /**
     * @brief Accepts a variable dropped onto the canvas and creates a matching node under the mouse.
     * @param canvasRect The whole canvas in screen space.
     * @note Call between ed::Suspend() and ed::Resume(): screen coordinates, but still inside the editor
     *       so ed::ScreenToCanvas works.
     */
    void acceptVariableDrop(const ImRect& canvasRect, const std::vector<GraphVariable>& variables,
                            ParticleGraph& graph);
}
