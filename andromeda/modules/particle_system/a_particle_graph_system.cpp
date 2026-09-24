#include "a_particle_graph_system.hpp"

#include "a_components.hpp"
#include "a_node_evaluation.hpp"
#include "a_registry.hpp"
#include "a_subsystem_manager.hpp"
#include "scene.hpp"

namespace Andromeda {

    void ParticleGraphSystem::start() {
        auto sceneManager = SystemManager::getInstance().getSubsystem<SceneManager>();
        m_Registry = &sceneManager->m_Registry;
    }

    void ParticleGraphSystem::update() {
        // Paused means paused: the clock stands still, so a graph driven by time freezes with the
        // rest of the scene instead of jumping forward when playback resumes.
        const float deltaTime = SystemManager::s_paused ? 0.0f : SystemManager::s_deltaTime;
        m_Time += deltaTime;

        auto& pool = m_Registry->getPool<ECS::Component::ParticleSystem>();
        for (const ECS::Entity entity : pool.getEntities()) {
            auto& system = pool.get(entity);

            const GraphContext context{system.graphVariables, m_Time, deltaTime};
            for (ParticleGroup& group : system.allParticleGroups())
                evaluateGraph(group.graph, group, context);
        }
    }
}
