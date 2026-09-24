#pragma once

/**
 * @file a_GraphCoords.hpp
 * @brief Conversions between the graph's own vec2 and ImGui's ImVec2.
 *
 * @details The graph data stores positions as vec2 (it must not depend on ImGui), while the node editor
 *          speaks ImVec2. Everything that moves a node across that border goes through here.
 */

#include "imgui.h"
#include "a_Primitives.hpp"

namespace Andromeda::Gui {

    inline ImVec2 toImVec2(vec2 v) {
        return ImVec2(v.x, v.y);
    }

    inline vec2 toVec2(ImVec2 v) {
        return vec2(v.x, v.y);
    }
}
