#pragma once
#include "a_node_pins.hpp"
namespace Andromeda::Gui::Node {

    struct [[ParticleNode]] AddNode {
        Input<float> a;
        Input<float> b;
        Output<float> result;
    };

    struct [[ParticleNode]] IntVariable {
        Param<i32> value;
        Output<i32> out;
    };

    struct [[ParticleNode]] FloatVariable {
        Param<float> value;
        Output<float> out;
    };

    struct [[ParticleNode]] BoolVariable {
        Param<bool> value;
        Output<bool> out;
    };
}