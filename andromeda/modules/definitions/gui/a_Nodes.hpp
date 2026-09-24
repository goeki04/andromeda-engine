#pragma once

/**
 * @file a_Nodes.hpp
 * @brief The node types of the particle graph: what each node holds and which pins it shows.
 *
 * @details A struct marked [[ParticleNode]] is picked up by the generator, listed in
 *          Meta::ReflectedNodes and offered in the editor's "Add Node" menu - nothing else has to
 *          be registered. Field order is part of the saved data: a pin ID is node ID + field index,
 *          so new fields go at the end, or saved links point at the wrong pin.
 *
 * @note These structs hold data only. What a node *computes* is an evaluateNode() overload in
 *       a_node_evaluation.hpp, so add the rule there whenever you add a node type here. A node type
 *       without its own overload is drawn and saved like any other, but its outputs stay at their
 *       default value. The rules cannot live here: the OutputNode writes into a ParticleGroup, and
 *       a_particle_group.hpp already includes this file through the graph.
 */

#include "a_node_pins.hpp"
namespace Andromeda::Gui::Node {

    struct [[ParticleNode]] AddNode {
        Input<float> a;
        Input<float> b;
        Output<float> result;
    };

    struct [[ParticleNode]] Subtract {
        Input<float> a;
        Input<float> b;
        Output<float> result;
    };

    struct [[ParticleNode]] Multiply {
        Input<float> a;
        Input<float> b;
        Output<float> result;
    };

    struct [[ParticleNode]] Divide {
        Input<float> a;
        Input<float> b;
        Output<float> result;
    };

    struct [[ParticleNode]] Clamp {
        Input<float> value;
        Param<float> min;
        Param<float> max;
        Output<float> result;
    };

    struct [[ParticleNode]] Lerp {
        Input<float> a;
        Input<float> b;
        Input<float> t;
        Output<float> result;
    };

    struct [[ParticleNode]] Time {
        Output<float> seconds;
        Output<float> deltaTime;
    };

    struct [[ParticleNode]] OutputNode {
        Input<i32> particleCount;
        Input<float> size;
        Input<vec3> velocity;
        Input<vec3> particleColor;
        Input<float> minLifetime;
    };

    // Variable nodes: with variableId == 0 (added from the menu) the value is the node's own constant.
    // Dropped from the variables list, variableId points at a GraphVariable of the ParticleSystem and the
    // editor keeps value and variable in sync both ways. variableId has no pin role, so it is not drawn;
    // it stays the last field so the pin IDs of value and out (field indices 0 and 1) never move.

    struct [[ParticleNode]] IntVariable {
        Param<i32> value;
        Output<i32> out;
        u32 variableId = 0; ///< GraphVariable this node is bound to, 0 = unbound.
    };

    struct [[ParticleNode]] FloatVariable {
        Param<float> value;
        Output<float> out;
        u32 variableId = 0; ///< GraphVariable this node is bound to, 0 = unbound.
    };

    struct [[ParticleNode]] BoolVariable {
        Param<bool> value;
        Output<bool> out;
        u32 variableId = 0; ///< GraphVariable this node is bound to, 0 = unbound.
    };
}