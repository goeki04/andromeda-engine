#pragma once

/**
 * @file a_IMeshLookup.hpp
 * @brief The questions the scene serializer asks about meshes.
 */

#include <string>
#include "a_primitives.hpp"
#include "a_Guid.hpp"

namespace Andromeda {

    /**
     * @class IMeshLookup
     * @brief Translates between session-local mesh IDs and references a saved file can keep.
     * @details Mesh IDs are handed out while models load and differ from run to run, so a scene file
     *          stores the asset's Guid, plus the mesh name as fallback for meshes without a file such
     *          as the built-in primitives. These four questions are all the serializer needs. Asking
     *          them through this interface instead of the whole ResourceManager keeps the serializer
     *          free of assimp and OpenGL, and lets tests answer them from a plain map.
     */
    class IMeshLookup {
    public:
        virtual ~IMeshLookup() = default;

        /** @brief Display name of the mesh, or "" for an unknown ID. */
        [[nodiscard]] virtual std::string getMeshNameByID(u32 id) const = 0;

        /** @brief The mesh registered under @p name; @p out is left untouched if there is none. */
        [[nodiscard]] virtual bool tryGetMeshIDByName(const std::string& name, u32& out) const = 0;

        /** @brief The mesh loaded from the asset with @p guid; @p out is left untouched if there is none. */
        [[nodiscard]] virtual bool tryGetMeshIDByGuid(const Guid& guid, u32& out) const = 0;

        /** @brief The asset a mesh was loaded from. False for meshes without a file, e.g. primitives. */
        [[nodiscard]] virtual bool tryGetGuidByMeshID(u32 meshID, Guid& out) const = 0;
    };
}
