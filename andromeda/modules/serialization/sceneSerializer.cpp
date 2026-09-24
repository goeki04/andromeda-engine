#include "sceneSerializer.hpp"
#include "a_components.hpp"
#include "generated_components.hpp"
#include "a_logger.hpp"
#include <algorithm>
#include <fstream>
#include <tuple>
#include <type_traits>
#include <iostream>
#include <unordered_set>
#include "a_Primitives.hpp"
namespace Andromeda {

	bool SceneSerializer::save(const std::string& filepath, ECS::ComponentRegistry& registry, ResourceManager& rm)
	{
		// Build and dump the whole JSON in memory before touching the file: an exception while serializing
		// (e.g. a value nlohmann cannot write, such as a string that is not valid UTF-8) is logged and the
		// save fails, instead of ending the editor or leaving a half-written scene file behind.
		std::string text;
		try {
			nlohmann::json root;
			const std::string meshTypeName = typeid(ECS::Component::MeshRenderer).name();

			for (const auto &poolPtr: registry.m_Pools | std::views::values) {
				const std::string typeName = poolPtr->getTypeName();
				// The MeshRenderer pool is handled specially below: its volatile, session-local
				// meshID must be persisted as a stable mesh name instead of a raw number.
				if (typeName == meshTypeName) continue;
				root[typeName] = poolPtr->serializePool();
			}

			// MeshRenderer: store stable mesh names instead of volatile IDs.
			if (registry.m_Pools.contains(std::type_index(typeid(ECS::Component::MeshRenderer)))) {
				auto& meshPool = registry.getPool<ECS::Component::MeshRenderer>();
				nlohmann::json mr;
				mr["entities"] = meshPool.getEntities();
				nlohmann::json names = nlohmann::json::array();
				for (const auto& comp : meshPool.data()) {
					names.push_back(rm.getMeshNameByID(comp.meshID));
				}
				mr["meshNames"] = names;
				root[meshTypeName] = mr;
			}

			text = root.dump(4);
		} catch (const std::exception& e) {
			A_ERROR("Scene could not be serialized: {}", e.what());
			return false;
		}

		std::ofstream file(filepath);
		if (!file.is_open()) {
			A_ERROR("Scene file '{}' could not be opened for writing", filepath);
			return false;
		}
		file << text << std::endl;
		return true;
	}
    namespace {
        /**
         * @brief Loads the pool of every component type in @p directory that is present in @p root.
         * @details Driven by the generated ComponentDirectory, so a new component is loaded without
         *          touching this file - save() already writes every pool generically. MeshRenderer is
         *          skipped: its mesh IDs are session-local and need the name translation in load().
         * @return The highest entity ID found.
         */
        template<typename... Components>
        ECS::Entity loadComponentPools(const nlohmann::json& root, ECS::ComponentRegistry& registry,
                                       std::tuple<Components...>* /*directory*/) {
            ECS::Entity maxID = 0;
            auto loadPool = [&]<typename C>() {
                if constexpr (!std::is_same_v<C, ECS::Component::MeshRenderer>) {
                    const std::string name = typeid(C).name();
                    if (!root.contains(name))
                        return;
                    auto& pool = registry.getPool<C>();
                    pool.deserializePool(root[name]);
                    for (const auto id : pool.getEntities())
                        maxID = std::max(maxID, id);
                }
            };
            (loadPool.template operator()<Components>(), ...);
            return maxID;
        }
    }

    bool SceneSerializer::load(const std::string& filepath, ECS::ComponentRegistry& registry, ResourceManager& rm)
    {
        // Read and parse first, clear afterwards: a missing or broken file must leave the scene untouched.
        std::ifstream file(filepath);
        if (!file.is_open()) {
            A_WARN("No scene file at '{}'", filepath);
            return false;
        }

        nlohmann::json root;
        try {
            file >> root;
        } catch (const nlohmann::json::exception& e) {
            A_ERROR("Scene file '{}' could not be parsed: {}", filepath, e.what());
            return false;
        }

        registry.clearRegistry();
        ECS::Entity maxID = loadComponentPools(root, registry, static_cast<ECS::Component::ComponentDirectory*>(nullptr));

        const std::string meshName = typeid(ECS::Component::MeshRenderer).name();
        if (root.contains(meshName)) {
            const auto& mr = root[meshName];
            const auto entities = mr.at("entities").get<std::vector<ECS::Entity>>();
            const auto names = mr.at("meshNames").get<std::vector<std::string>>();

            // Translate stable mesh names back into the current session's mesh IDs,
            // then rebuild the pool from the resolved data.
            nlohmann::json rebuilt;
            rebuilt["entities"] = nlohmann::json::array();
            rebuilt["components"] = nlohmann::json::array();
            for (size_t i = 0; i < entities.size(); ++i) {
                u32 id;
                if (!rm.tryGetMeshIDByName(names[i], id)) {
                    A_WARN("mesh '{}' not found; skipping entity {}", names[i], entities[i]);
                    continue;
                }
                rebuilt["entities"].push_back(entities[i]);
                rebuilt["components"].push_back(ECS::Component::MeshRenderer{ id });
            }

            auto& pool = registry.getPool<ECS::Component::MeshRenderer>();
            pool.deserializePool(rebuilt);
            for (auto id : pool.getEntities()) {
                if (id > maxID) maxID = id;
            }
        }

        if (maxID > 0 || !root.empty()) {
            registry.m_NextID = maxID + 1;
        }
        registry.rebuildActiveEntities();
        return true;
    }
}