#pragma once
#include "a_primitives.hpp"
#include <string>
#include "a_EventTypes.hpp"
#include <variant>
#include <functional>
namespace Andromeda {

    struct ParticleGroup {
        std::string groupName = "ParticleGroup_1";  ///< Name of the particle group for identification.
        i32 particleCount = 1000;                ///< Maximum number of particles allowed in this group.
        float size = 1.0f;                       ///< Size of each particle in this group.
        vec3 velocity = {1.0f, 1.0f, 1.0f};      ///< Initial velocity of particles in this group.
        vec3 particleColor = {1.0f, 1.0f, 1.0f}; ///< Color of the particles (RGB).
        float minLifeTime = 0.0f;                ///< Minimum lifetime of the particles in seconds.
        //std::vector<ParticleBindings> bindings;  ///< List of event bindings for this particle group.
    };
} // namespace Andromeda   