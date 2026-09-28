#pragma once

/**
 * @file a_particle_graph_system.hpp
 * @brief Subsystem that runs the node graph of every particle group, once per frame.
 */

#include <string_view>
#include "a_ISubsystem.hpp"
#include "a_node_evaluation.hpp"
#include "a_sensor_data.hpp"
#include "a_event_manager.hpp"
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

        void destroy() override {}

        static constexpr std::string_view GetStaticName() { return "ParticleGraphSystem"; }

        const char* getSubsystemName() const override {
            return GetStaticName().data();
        }

    private:
        float m_Time = 0.0f;                          ///< Seconds the scene has been running; see the Time node.
        ECS::ComponentRegistry* m_Registry = nullptr; ///< Pointer to the scene's registry.
        bool m_HasTelemetry = false;
        SensorTelemetry m_Telemetry; ///< The most recent telemetry, updated by updateSensorChannels().
        std::vector<SensorChannel> m_SensorChannels;
        EventListenerID m_OnSensorMessageReceived; ///< The subscription to the telemetry event, released in destroy().
        /**
         * @brief Flattens the most recent telemetry into name/value pairs the graph can read.
         * @details Driven by reflection, so a new sensor type needs no change here: it brings its own
         *          StructInfo, and every field that has channels (ChannelTraits) shows up as a channel.
         *          Fields without channels, a sensor name for instance, are skipped - the graph computes
         *          with numbers.
         */

        void updateSensorChannels();
    };
}
