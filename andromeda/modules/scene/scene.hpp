#pragma once

/**
 * @file scene.hpp
 * @brief Subsystem that owns the active scene: its entity registry and editor camera.
 */

#include "a_registry.hpp"
#include <string>
#include "a_ISubsystem.hpp"
#include "a_math.hpp"
#include "resource_manager.h"
namespace Andromeda {
	using Entity = uint32_t; ///< Convenience alias for an ECS entity ID at scene scope.

	/**
	 * @class SceneManager
	 * @brief Holds the scene's ECS registry and editor camera, and provides entity creation/removal.
	 *
	 * @details As an @c ISubsystem it is owned by the @c SystemManager. It exposes the component
	 *          registry (@c m_Registry) that all other systems (renderer, editor, serializer)
	 *          operate on, and the editor's camera state (@c m_EditorCamData). Entity helpers
	 *          resolve mesh resources through the @c ResourceManager.
	 */
	class SceneManager : public ISubsystem{
	public:
		static constexpr const char* subsystemName = "Scene"; ///< Legacy/static name label for the subsystem.
		ECS::ComponentRegistry m_Registry;   ///< The registry holding all entities and components of the active scene.
		amath::CameraData m_EditorCamData;   ///< Camera state used to view/navigate the scene in the editor.

		/**
		 * @brief Creates a new entity in the scene with a mesh, name and transform.
		 * @param meshID Resource ID of the mesh to attach (via a MeshRenderer component).
		 * @param name Display name assigned to the entity's Tag component.
		 * @param transform Initial position/rotation/scale of the entity.
		 */
		void addEntity(u32 meshID, const std::string& name, ECS::Component::Transform transform);

		/**
		 * @brief Creates an entity with nothing but a Transform and a Tag, like Unity's "Create Empty".
		 * @details No MeshRenderer, no Material, no AABB: an empty entity is a place in the world that
		 *          other components are added to, a particle system for instance. Adding a MeshRenderer
		 *          with mesh ID 0 instead would make the renderer look up a mesh that does not exist.
		 * @return The new entity, so the caller can select it right away.
		 */
		Entity addEmptyEntity(const std::string& name, ECS::Component::Transform transform = {});

		/**
		 * @brief Removes an entity and all of its components from the scene.
		 * @param id The entity ID to destroy.
		 */
		void deleteEntity(Entity id);

		/** @brief Initializes the subsystem and caches the @c ResourceManager dependency. */
		void start() override;

		/**
		 * @brief Gets the static compile-time string identifier of the subsystem.
		 * @return A string_view containing "SceneManager".
		 */
		static constexpr std::string_view GetStaticName() { return "SceneManager"; }

		/**
		 * @brief Gets the runtime string identifier of the subsystem.
		 * @return A C-string containing the subsystem's name.
		 */
		const char* getSubsystemName() const override {
			return GetStaticName().data();
		}
	private:
		ResourceManager* m_ResourceManager = nullptr; ///< Cached resource manager used to resolve meshes for new entities.
	};
}