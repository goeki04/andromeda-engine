#pragma once
#include "a_node_pins.hpp"
namespace Andromeda::Gui::Node {

    struct [[ParticleNode]] AddNode {
        Input<float> a;
        Input<float> b;
        Output<float> result;
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