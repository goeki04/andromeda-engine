#pragma once

/**
 * @file a_particle_binding_system.hpp
 * @brief Subsystem that applies event bindings to the particle groups in the scene.
 */

#include "a_ISubsystem.hpp"
#include <string_view>
#include "a_event_manager.hpp"
#include <string>
namespace Andromeda::ECS {
    class ComponentRegistry;
}
namespace Andromeda {
    
    /**
     * @class ParticleBindingSystem
     * @brief Drives ParticleGroup fields from engine events, once per frame.
     *
     * @details A particle group does not subscribe to anything - it "listens" to an event by
     *          carrying a matching entry in its @c eventBindings. This subsystem holds the single
     *          subscription for the whole engine, remembers the most recent payload, and walks
     *          every ParticleSystem component in @c update() to apply it. Groups and bindings can
     *          therefore be created and destroyed at runtime without touching the subscription.
     */
    class ParticleBindingSystem : public ISubsystem {
    public:
        /** @brief Registers the event subscription. */
        void start() override;

        /** @brief Applies the most recent payload to every bound particle group. Once per frame. */
        void update() override;

        /** @brief Cancels the event subscription. */
        void destroy() override;

        static constexpr std::string_view GetStaticName() { return "ParticleBindingSystem"; }
        EventListenerID m_OnSensorMessageReceived;
        std::string payload;
        ECS::ComponentRegistry* m_Registry = nullptr; ///< Pointer to the scene's registry
        /**
         * @brief Gets the runtime string identifier of the subsystem.
         * @return A C-string containing the subsystem's name.
         */
        const char* getSubsystemName() const override {
            return GetStaticName().data();
        }
    };
}
