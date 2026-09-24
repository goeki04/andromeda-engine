#pragma once

/**
 * @file a_particle_graph_system.hpp
 * @brief Subsystem that runs the node graph of every particle group, once per frame.
 */

#include <string_view>
#include "a_ISubsystem.hpp"

namespace Andromeda::ECS {
    class ComponentRegistry;
}

namespace Andromeda {

    /**
     * @class ParticleGraphSystem
     * @brief Evaluates every ParticleGroup's graph and writes the result into the group.
     *
     * @details Runs here and not in the particle editor, for two reasons: a graph has to keep
     *          running while the panel is closed, and the Time node needs a clock that does not
     *          depend on a window being open. The editor only draws what this system computed.
     *
     *          Every group is evaluated, also the ones hidden while useParticleGroups is off -
     *          otherwise turning groups on would show values frozen at the moment they were hidden.
     */
    class ParticleGraphSystem : public ISubsystem {
    public:
        /** @brief Looks up the scene registry. */
        void start() override;

        /** @brief Advances the graph clock and evaluates every graph in the scene. */
        void update() override;

        static constexpr std::string_view GetStaticName() { return "ParticleGraphSystem"; }

        const char* getSubsystemName() const override {
            return GetStaticName().data();
        }

    private:
        float m_Time = 0.0f;                          ///< Seconds the scene has been running; see the Time node.
        ECS::ComponentRegistry* m_Registry = nullptr; ///< Pointer to the scene's registry.
    };
}
