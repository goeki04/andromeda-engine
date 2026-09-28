#include "scene.hpp"
#include "a_geometry.hpp"
#include "a_components.hpp"
#include "a_subsystem_manager.hpp"
namespace Andromeda{

    void SceneManager::addEntity(u32 meshID, const std::string& name, ECS::Component::Transform transform)
    {
        auto handle = m_Registry.createHandle();
        handle.add<ECS::Component::Transform>(transform);
        handle.add<ECS::Component::MeshRenderer>({ meshID});
        handle.add<ECS::Component::Material>({ "PBRMaterial" });
        handle.add<ECS::Component::Tag>({ name });
        auto aabb = m_ResourceManager->getMeshByID(meshID).getAABB();
        handle.add<ECS::Component::AABB>(aabb);
    }

    Entity SceneManager::addEmptyEntity(const std::string& name, ECS::Component::Transform transform)
    {
        auto handle = m_Registry.createHandle();
        handle.add<ECS::Component::Transform>(transform);
        handle.add<ECS::Component::Tag>({ name });
        return handle.id;
    }

    void SceneManager::deleteEntity(const Entity id)
    {
        m_Registry.destroyEntity(id);
    }
    void SceneManager::start()
    {
        m_ResourceManager = SystemManager::getInstance().getSubsystem<ResourceManager>();
        if (!m_ResourceManager) {
            throw std::runtime_error("m_ResourceManager is not initialized");
        }
    }
}