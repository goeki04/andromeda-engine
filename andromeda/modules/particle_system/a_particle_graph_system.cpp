#include "a_particle_graph_system.hpp"
#include "a_components.hpp"
#include "a_registry.hpp"
#include "a_subsystem_manager.hpp"
#include "scene.hpp"
#include "a_sensor_events.hpp"
#include "generated_telemetry_meta.hpp"
namespace Andromeda {

    void ParticleGraphSystem::start() {
        auto sceneManager = SystemManager::getInstance().getSubsystem<SceneManager>();
        m_Registry = &sceneManager->m_Registry;

        m_OnSensorMessageReceived = EventManager::getInstance().AddEventListener<OnSensorMessageReceived>([this](const OnSensorMessageReceived& event) {
            m_HasTelemetry = true;
            m_Telemetry = event.m_Telemetry;
        });
    }

    void ParticleGraphSystem::update() {
        // Paused means paused: the clock stands still, so a graph driven by time freezes with the
        // rest of the scene instead of jumping forward when playback resumes.
        const float deltaTime = SystemManager::s_paused ? 0.0f : SystemManager::s_deltaTime;
        m_Time += deltaTime;
        updateSensorChannels();
        auto& pool = m_Registry->getPool<ECS::Component::ParticleSystem>();
        for (const ECS::Entity entity : pool.getEntities()) {
            auto& system = pool.get(entity);

            const GraphContext context{
                .sensorChannels = m_SensorChannels,
                .variables = system.graphVariables,
                .time = m_Time,
                .deltaTime = deltaTime,
            };
            for (ParticleGroup& group : system.allParticleGroups())
                evaluateGraph(group.graph, group, context);
        }
    }
    void ParticleGraphSystem::updateSensorChannels() {
        m_SensorChannels.clear();
        if (!m_HasTelemetry)
            return;

        std::visit([this](const auto& telemetry) { 
            Meta::forEachField(telemetry, [this](const auto& field, const auto& member) {
                  using V = std::decay_t<decltype(member)>;
                    if constexpr (ChannelTraits<V>::count == 1) {
                        m_SensorChannels.push_back({field.name, static_cast<float>(member)});
                    }
            });
        }, m_Telemetry);
    }
}
