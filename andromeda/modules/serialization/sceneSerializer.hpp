#pragma once

/**
 * @file sceneSerializer.hpp
 * @brief Saving and loading of the ECS scene state to and from JSON files.
 */

#include <string>
#include <nlohmann/json.hpp>
#include "a_registry.hpp"
#include "a_IMeshLookup.hpp"
namespace Andromeda {

	/**
	 * @class SceneSerializer
	 * @brief Persists and restores a @c ComponentRegistry (the whole scene) as JSON.
	 *
	 * @details Walks the registry's component pools to write/read each component type. Stateless;
	 *          all functionality is exposed through static methods. toJson()/fromJson() do the actual
	 *          work in memory, save()/load() only add the file around it - so the format can be
	 *          tested without touching the disk.
	 *
	 *          Mesh references are the one special case: a MeshRenderer's meshID only means something
	 *          in the current session, so it is written as the asset's Guid plus the mesh name and
	 *          translated back through an @c IMeshLookup when loading.
	 *
	 * @warning Component pools are keyed by @c typeid(T).name(), which is compiler-specific.
	 *          See @c save() / @c load() for the cross-platform implications.
	 */
	class SceneSerializer{
	public:
		/**
		 * @brief Builds the JSON document for the whole registry.
		 * @throws nlohmann::json::exception if a component holds a value JSON cannot represent.
		 */
		static nlohmann::json toJson(ECS::ComponentRegistry& registry, const IMeshLookup& meshes);

		/**
		 * @brief Replaces the contents of @p registry with the scene described by @p root.
		 * @details The scene is built in a separate registry first and only moved into @p registry
		 *          once it was read completely: a damaged document leaves the current scene untouched.
		 *          An entity whose mesh cannot be found in @p meshes loses its MeshRenderer (logged),
		 *          its other components are kept.
		 * @return false if @p root could not be read; @p registry is unchanged then.
		 */
		static bool fromJson(const nlohmann::json& root, ECS::ComponentRegistry& registry, const IMeshLookup& meshes);

        /**
     * @brief Serializes the current ECS state into a JSON file.
     * * Iterates through all component pools in the registry and stores their
     * data using the type name as the primary key. The file is replaced atomically.
     * * @param filepath Path to the destination file.
     * @param registry Reference to the ECS registry to be saved.
     * @return true if saving was successful, false otherwise.
     * * @warning @b CROSS-PLATFORM-ISSUE: This function uses `typeid(T).name()` as a key.
     * Compilers like MSVC (Windows) and GCC (Linux) produce different strings for the same type.
     * Files saved on Windows might not be loadable on Linux.
     * * @note @b SOLUTION: Replace `typeid(T).name()` with a manual string identifier
     * (e.g., a `static constexpr const char* name` inside each component struct).
     */
		static bool save(const std::string& filepath, ECS::ComponentRegistry& registry, const IMeshLookup& meshes);
        /**
     * @brief Deserializes ECS data from a JSON file into the registry.
     * * This function attempts to find known component pools within the JSON structure.
     * It reconstructs the Sparse-Set for each pool and updates the global entity counter.
     * A missing or broken file leaves the registry untouched.
     * * @param filepath Path to the source file.
     * @param registry Reference to the ECS registry to be populated.
     * @return true if loading was successful, false otherwise.
     * * @bug If the compiler-generated type name in the file does not match the current
     * platform's `typeid(T).name()`, the pool will be skipped.
     */
		static bool load(const std::string& filepath, ECS::ComponentRegistry& registry, const IMeshLookup& meshes);
	};
}
