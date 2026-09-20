#pragma once
#include "a_node_pins.hpp"
namespace Andromeda::Gui::Node {

    struct [[ParticleNode]] AddNode {
        Input<float> a;
        Input<float> b;
        Output<float> result;
    };
}