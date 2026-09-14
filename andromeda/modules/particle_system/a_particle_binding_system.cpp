#include "a_particle_binding_system.hpp"
#include "a_registry.hpp"
#include "a_logger.hpp"
namespace Andromeda {

    void ParticleBindingSystem::start()
    {
        
        m_OnSensorMessageReceived = EventManager::getInstance().AddEventListener<OnSensorMessageReceived>([this](const auto& snr) { 
                payload = snr.message;
            });
    }

    void ParticleBindingSystem::update()
    {
        A_INFO("ParticleBindingSystem::update() - payload: {}", payload);
    }

    void ParticleBindingSystem::destroy()
    {
        EventManager::getInstance().RemoveEventListener(m_OnSensorMessageReceived);
    }
}
