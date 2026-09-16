#include "a_particle_binding_system.hpp"

#include "a_registry.hpp"
#include "a_logger.hpp"
#include "a_components.hpp"
#include "generated_event_meta.hpp"
#include "a_subsystem_manager.hpp"
#include "scene.hpp"
namespace Andromeda {

    /**
     * @brief The event names offered by the editor dropdown, in the order the generator emits them.
     *
     * @details EventBinding::eventIndex is an index into exactly this array, so the order here is
     *          what gives the stored index its meaning. Inserting an event in a_EventTypes.hpp
     *          ahead of an existing one therefore shifts every saved binding behind it - a reason
     *          to serialize bindings by name rather than by index once scenes are shipped.
     */
    inline constexpr auto& g_EventNames = Andromeda::Meta::ReflectedEventsNames;
    
    void ParticleBindingSystem::applyBinding(std::vector<EventBinding>& bindings) {

        for (size_t i = 0; i < bindings.size(); ++i) {
            // -1 is the normal state of a binding the user has not filled in yet, and a scene
            // saved against an older event list can point past the end. Both are skipped quietly.
            const i32 idx = bindings[i].eventIndex;
            if (idx < 0 || idx >= static_cast<i32>(g_EventNames.size()))
                continue;

            // Next step: read the channel named by the binding out of m_Telemetry and write it
            // into the group's target field. Until the channel list exists there is nothing to
            // apply, so the loop only validates the selection.
        }
    }

    /**
     * @brief Looks up the scene registry and subscribes to the bindable events.
     * @note The subscription names one concrete event type, so every additional event that gets
     *       the [[BindableEvent]] marker needs its own AddEventListener call here. See the file
     *       header of a_particle_binding_system.hpp for the full checklist.
     */
    void ParticleBindingSystem::start()
    {
        auto scenemanager = SystemManager::getInstance().getSubsystem<SceneManager>();
        m_Registry = &scenemanager->m_Registry;
        // The callback only records the payload. Applying it here would mean writing to ECS
        // components from inside NetworkManager::update(), where the dispatch originates.
        m_OnSensorMessageReceived = EventManager::getInstance().AddEventListener<OnSensorMessageReceived>(
            [this](const OnSensorMessageReceived& e) {
                m_Telemetry = e.m_Telemetry;
                m_HasTelemetry = true;
            });
    }

    /**
     * @brief Applies the stored payload to every binding in the scene, once per frame.
     * @note Returns early until the first event has arrived - m_Telemetry would otherwise hold the
     *       variant's default-constructed first alternative, which looks like a real reading.
     * @note getPool<T>() creates an empty pool on demand, so a scene without a single particle
     *       system is not a special case here.
     */
    void ParticleBindingSystem::update()
    {
        if (!m_HasTelemetry)
            return;

        auto& pool = m_Registry->getPool<ECS::Component::ParticleSystem>();

        auto& entities = pool.getEntities();
        for (size_t i = 0; i < entities.size(); ++i) {
                auto& obj = pool.get(entities[i]);
            for (auto& group : obj.getParticleGroups()) {
                auto& bindings = group.eventBindings;
                applyBinding(bindings);
            }
        }
    }

    /** @brief Cancels the subscription so the EventManager stops calling into a dead object. */
    void ParticleBindingSystem::destroy()
    {
        EventManager::getInstance().RemoveEventListener(m_OnSensorMessageReceived);
    }
}
