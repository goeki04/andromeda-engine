#pragma once

/**
 * @file a_particle_binding_system.hpp
 * @brief Subsystem that applies event bindings to the particle groups in the scene.
 *
 * @details This file is the reference for the whole binding feature, because it is the place where
 *          the three halves meet: the events (a_EventTypes.hpp), the authoring data
 *          (a_particle_group.hpp) and the editor UI (a_particle_component_ui.hpp).
 *
 * ### How a binding flows through the engine
 *
 * 1. The sensor line arrives on the network thread and is pushed into a queue
 *    (a_tcp_sensor_client.cpp).
 * 2. NetworkManager::update() drains the queue on the main thread and dispatches the event
 *    through the EventManager.
 * 3. ParticleBindingSystem, subscribed since start(), stores the payload in a member. It does
 *    @b not touch any component from inside the callback - that would mean writing to the ECS in
 *    the middle of another subsystem's update.
 * 4. ParticleBindingSystem::update() walks every ParticleSystem component, every ParticleGroup in
 *    it and every EventBinding in that group, and applies the stored payload wherever a binding
 *    refers to the event it just received.
 *
 * ### Making a new event bindable
 *
 * 1. Annotate the event struct with @c [[BindableEvent]]. Without the marker the generator skips
 *    it and it never appears in the editor's event dropdown. Engine-wide events live in
 *    a_EventTypes.hpp; a sensor-specific one belongs next to its payload (see a_sensor_events.hpp),
 *    so that the engine core does not start depending on the data module.
 * 2. Give the event a payload whose fields have channels. @c ChannelTraits<std::string>::count is
 *    0, so a raw string carries nothing bindable - parse before dispatching. OnSensorMessageReceived
 *    carries a @c SensorTelemetry variant, and the channels come from reflecting its alternatives
 *    (BMV080Telemetry and any sensor added to the variant later), not from the event itself.
 * 3. Add the header to the event generator's inputs in modules/definitions/CMakeLists.txt, then
 *    build. The @c generate_ecs_metadata target re-runs the scanner, so the event lands in
 *    Meta::ReflectedEvents and Meta::ReflectedEventsNames, and the dropdown picks it up with no
 *    change to the UI code.
 * 4. Subscribe to it in ParticleBindingSystem::start(). This is currently the one manual step:
 *    the subscription names a single concrete event type, so a newly marked event shows up in the
 *    dropdown but stays silent until it is subscribed here as well.
 * 5. Make sure something actually dispatches the event, otherwise the binding has no source.
 *
 * ### Creating a binding in the editor
 *
 * Select an entity with a particle system, expand a particle group, open "Event Bindings" and
 * press "+ Binding". Both dropdowns start at -1 ("nothing selected"); pick the event in the first
 * and the target field in the second. The field list only offers fields that have channels, so
 * groupName and eventBindings never appear there.
 */

#include "a_ISubsystem.hpp"
#include <string_view>
#include "a_event_manager.hpp"
#include <string>
#include "a_particle_group.hpp"
#include "a_sensor_events.hpp"
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
     *
     *          The number of subscriptions scales with the number of bindable event @e types, not
     *          with the number of groups or bindings in the scene - that is what keeps the
     *          lifetime handling down to a single ID per event type.
     *
     * @note @c m_HasTelemetry exists because std::variant default-constructs to its first
     *       alternative: without the flag, "nothing received yet" would be indistinguishable from
     *       a BMV080Telemetry reading of all zeroes.
     */
    class ParticleBindingSystem : public ISubsystem {
    public:
        /**
         * @brief Writes the stored payload into every binding of one particle group.
         * @param bindings The group's binding list; entries are updated in place.
         * @note Entries whose eventIndex is out of range are skipped rather than treated as an
         *       error: -1 is the normal state of a binding the user has not filled in yet, and a
         *       scene file may be older than the current event list.
         */
        void applyBinding(std::vector<EventBinding>& bindings);

        /** @brief Looks up the scene registry and registers the event subscription. */
        void start() override;

        /** @brief Applies the most recent payload to every bound particle group. Once per frame. */
        void update() override;

        /** @brief Cancels the event subscription. */
        void destroy() override;

        static constexpr std::string_view GetStaticName() { return "ParticleBindingSystem"; }

        /** @brief Handle of the OnSensorMessageReceived subscription, released in destroy(). */
        EventListenerID m_OnSensorMessageReceived;

        /**
         * @brief Gets the runtime string identifier of the subsystem.
         * @return A C-string containing the subsystem's name.
         */
        const char* getSubsystemName() const override {
            return GetStaticName().data();
        }

    private:
        SensorTelemetry m_Telemetry{};                ///< Payload of the most recent event, applied next update().
        bool m_HasTelemetry = false;                  ///< False until the first event arrived; see note below.
        ECS::ComponentRegistry* m_Registry = nullptr; ///< Pointer to the scene's registry
    };
}
