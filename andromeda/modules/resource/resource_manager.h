#pragma once

/**
 * @file resource_manager.h
 * @brief Central asset-management subsystem: loads, caches and serves meshes, textures, cubemaps, shaders and materials.
 */

#include <vector>
#include <unordered_map>
#include <string>
#include <memory>
#include <assimp/scene.h>
#include "a_components.hpp"
#include "OpenGL/a_opengl_texture.hpp"
#include "a_geometry.hpp"
#include "a_model_record.hpp"
#include "OpenGL/a_opengl_handles.hpp"
#include "a_primitives.hpp"
#include "a_ISubsystem.hpp"
#include "a_cubemapData.hpp"
#include "a_IGraphicsContext.hpp"
#include "a_Material.hpp"

namespace Andromeda {

    /**
     * @class ResourceManager
     * @brief asset management subsystem for the Andromeda Engine.
     * * The ResourceManager handles the asynchronous or synchronous loading, caching,
     * and distribution of all heavy assets. This includes 3D models (via Assimp),
     * textures, HDR/LDR cubemaps, UI icons, and the compilation of OpenGL shader programs.
     * It implements the IModelProvider interface to supply model data to the Editor.
     */
    class ResourceManager : public ISubsystem, public IModelProvider {
    public:

        /** @brief CPU-side storage of loaded mesh geometry, mapped by unique ID. */
        std::unordered_map<u32, Andromeda::Mesh> m_Meshes;

        /** @brief OpenGL texture handles for device-specific icons (e.g., ESP32, Sensors). */
        std::unordered_map<deviceType, GLtexture> m_DeviceIcons;

        /** @brief OpenGL texture handles for UI/Editor icons, mapped by filename. */
        std::unordered_map<std::string, GLtexture> m_EditorIcons;

        /** @brief Loaded cubemap environment data (LDR skies or HDR panoramas), mapped by name. */
        std::unordered_map<std::string, CubemapData> m_CubemapData;

        /**
         * @brief What the imported model files declared about their materials, keyed by material name.
         * @details Filled while importing, consumed by the renderer once it has a shader and a
         *          context to build real Materials from. Submesh::materialName indexes into this.
         */
        std::unordered_map<std::string, MaterialDef> m_MaterialDefs;

        /** @brief GPU textures loaded for model materials, keyed by absolute file path. */
        std::unordered_map<std::string, GLtexture> m_ModelTextures;

        /**
         * @brief Loads a model texture once and returns its GPU id, 0 when it cannot be loaded.
         * @details Repeated calls with the same path return the cached texture, so materials that
         *          share a map do not upload it twice. Unlike the editor icons these are sampled
         *          with wrapping and mipmaps, which is what surface textures need.
         * @param srgb True for colour maps (albedo, emissive), which are authored in sRGB and have
         *             to be converted to linear before lighting. False for data maps such as
         *             roughness and normals - converting those would corrupt their values.
         */
        u32 loadModelTexture(const std::string& path, bool srgb);

        /**
         * @brief Gets the static compile-time string identifier of the subsystem.
         * @return A string_view containing "ResourceManager".
         */
        static constexpr std::string_view GetStaticName() { return "ResourceManager"; }

        /**
         * @brief Gets the runtime string identifier of the subsystem.
         * @return A C-string containing the subsystem's name.
         */
        [[nodiscard]] const char* getSubsystemName() const override { return GetStaticName().data(); }

        /**
         * @brief Initializes the subsystem, pre-compiles core shaders, and initiates the asset loading pipeline.
         */
        void start() override;

        /**
         * @brief Recursively scans the "models" directory and loads all .fbx and .obj files via Assimp.
         */
        void loadModels();

        /**
         * @brief Scans and loads all cubemaps and environment maps.
         * Handles both LDR sets (6 individual textures) and HDR equirectangular panoramas.
         */
        void loadAllCubeMaps();

        /**
         * @brief Loads PNG/JPG device icons from the designated asset folder and registers them.
         */
        void loadDeviceIcons();

        /**
         * @brief Loads PNG/JPG editor UI icons from the designated asset folder and registers them.
         */
        void loadEditorIcons();

        /**
         * @brief Retrieves the OpenGL texture ID for a specific editor icon.
         * @param name The filename of the icon (without path).
         * @return The OpenGL texture ID. Throws an exception if the icon is not found.
         */
        u32 getEditorIconID(const std::string& name);

        /**
         * @brief Matches an icon filename to a specific hardware device type enum.
         * @param iconName The filename of the icon to check.
         * @return The corresponding deviceType, or deviceType::DEFAULT if no match is found.
         */
        static deviceType findDeviceIcon(const std::string& iconName);

        /**
         * @brief Retrieves the OpenGL Vertex Array Object (VAO) ID for a given mesh.
         * @param meshID The unique identifier of the mesh.
         * @return The OpenGL VAO ID.
         */
        [[nodiscard]] u32 getMeshVaoByID(u32 meshID) const;

        /**
         * @brief Retrieves the number of indices for a given mesh, required for glDrawElements.
         * @param meshID The unique identifier of the mesh.
         * @return The size of the index buffer.
         */
        [[nodiscard]] u32 getMeshIndexSizeByID(u32 meshID) const;

        /**
         * @brief Retrieves the CPU-side mesh data structure.
         * @param meshID The unique identifier of the mesh.
         * @return A constant reference to the Mesh data.
         */
        [[nodiscard]] const Mesh& getMeshByID(u32 meshID) const;

        /**
         * @brief Resolves a (volatile) mesh ID to its stable, persistable name.
         * @param id The session-local mesh ID.
         * @return The mesh name, or an empty string if the ID is unknown.
         */
        [[nodiscard]] std::string getMeshNameByID(u32 id) const;

        /**
         * @brief Resolves a stable mesh name to the current session's mesh ID.
         * @param name The stable mesh name (e.g. "Default_Cube").
         * @param out Receives the resolved mesh ID on success.
         * @return true if the name is known, false otherwise.
         */
        [[nodiscard]] bool tryGetMeshIDByName(const std::string& name, u32& out) const;

        /**
         * @brief Registers the built-in primitive meshes (Cube, Plane, Sphere) under
         *        stable names so that loaded scenes can always resolve them.
         */
        void registerDefaultPrimitives();

        /**
         * @brief Gets the total amount of unique 3D models currently loaded in memory.
         * @return The count of ModelRecords.
         */
        [[nodiscard]] u32 getModelRecordsSize() const;

        /**
         * @brief Retrieves the complete map of model records.
         * @return A constant reference to the unordered_map of ModelRecords.
         */
        [[nodiscard]] const std::unordered_map<uint32_t, ModelRecord>& getModelRecords() const;

        /**
         * @brief Utility to load a 2D image from disk directly into an OpenGL texture.
         * @param path The filepath to the image.
         * @return A GLtexture struct containing the OpenGL ID and dimensions.
         */
        static GLtexture CreateOpenGLTexture(const char* path);

        /**
         * @brief Loads a traditional 6-face LDR cubemap.
         * @param name The unique identifier for this cubemap.
         * @param paths A vector containing exactly 6 file paths mapped to cube faces.
         */
        void loadAndStoreCubemap(const std::string& name, const std::vector<std::string>& paths);

        /**
         * @brief Loads a single-file HDR/HDRI equirectangular panorama.
         * @param file The full system path to the HDR file.
         */
        void loadAndStoreCubemap(const std::string& file);

        /**
         * @brief Returns the total number of loaded models (Implementation of IModelProvider).
         * @return The model count.
         */
        [[nodiscard]] u32 getModelCount() const override;

        /**
         * @brief Retrieves metadata for a specific model (Implementation of IModelProvider).
         * @param index The sequential index of the model.
         * @return A constant reference to the ModelRecord.
         */
        [[nodiscard]] const ModelRecord& getModelData(u32 index) const override;

        /**
         * @brief Retrieves the OpenGL texture ID for a specific device type (Implementation of IModelProvider).
         * @param type The deviceType enum.
         * @return The OpenGL texture ID.
         */
        [[nodiscard]] u32 getDeviceIconID(deviceType type) const override;

        /**
         * @brief Dynamically registers a custom, procedurally generated mesh into the resource manager.
         * @param mesh The generated Mesh data (passed via r-value reference for move semantics).
         * @param name The display name of the mesh (e.g., "Cube", "Sphere").
         * @return The newly assigned unique mesh ID.
         */
        u32 registerCustomMesh(Mesh&& mesh, const std::string& name);


        /**
         * @brief Loads, compiles and caches a shader program through the RHI under a lookup name.
         * @param ctx The graphics context used to create the program (backend-agnostic).
         * @param name The key under which the resulting shader is cached and later retrieved.
         * @param vertPath Path to the vertex shader source.
         * @param fragPath Path to the fragment shader source.
         * @return A handle to the created (or already cached) shader program.
         */
        ShaderProgramHandle loadShaderRHI(IGraphicsContext* ctx, const std::string& name, const std::string& vertPath, const std::string& fragPath);

        /**
         * @brief Loads, compiles and caches a compute shader program through the RHI under a lookup name.
         * @details Shares the same name -> handle cache as @c loadShaderRHI, so compute and
         *          graphics programs must use distinct cache keys.
         * @param ctx The graphics context used to create the program (backend-agnostic).
         * @param name The key under which the resulting shader is cached and later retrieved.
         * @param computePath Path to the compute shader source.
         * @return A handle to the created (or already cached) compute program.
         */
        ShaderProgramHandle loadComputeShaderRHI(IGraphicsContext* ctx, const std::string& name, const std::string& computePath);
        ShaderProgramHandle loadComputeShaderRHI(IGraphicsContext* ctx, const std::string& name, const std::string& computePath, const char* stageName);
        /**
         * @brief Retrieves a previously loaded shader program by name.
         * @param name The cache key the shader was registered under.
         * @return The cached shader handle (a default/zero handle if not found).
         */
        ShaderProgramHandle getShaderRHI(const std::string& name) const;

        /**
         * @brief Creates and caches a material instance bound to a given shader program.
         * @param materialName The key under which the material is cached.
         * @param shaderHandle The shader program the material wraps.
         * @param ctx The graphics context used to allocate any backing GPU resources (e.g. UBOs).
         * @return A shared pointer to the newly created material.
         */
        std::shared_ptr<Material> createMaterial(const std::string& materialName, ShaderProgramHandle shaderHandle, IGraphicsContext* ctx);

        /**
         * @brief Retrieves a previously created material by name.
         * @param materialName The cache key the material was registered under.
         * @return A shared pointer to the material, or nullptr if not found.
         */
        std::shared_ptr<Material> getMaterial(const std::string& materialName) const;

        /**
         * @brief Releases all GPU-side RHI resources (shaders, materials) owned by the manager.
         * @details Must be called while the graphics context is still valid, i.e. before shutdown.
         * @param ctx The graphics context used to destroy the resources.
         */
        void destroyRHIResources(IGraphicsContext* ctx);

        u32 getPreviewTextureID(const u32 meshID) const override;
        void setPreviewTextureID(const u32 meshID,const u32 textureID);
    private:
        /**
         * @brief Decodes image data from disk into CPU memory using stb_image.
         * @param data Reference to the CubemapData structure to populate.
         */
        static void loadCubemapTexture(CubemapData& data);

        /**
         * @brief Maps a file path to its corresponding cube face index based on naming conventions.
         * @param data The CubemapData structure where the path will be assigned.
         * @param path The file path to analyze.
         */
        static void MapCubemapFacesLDR(CubemapData& data, const std::string& path);

        std::unordered_map<u32, ModelRecord> m_ModelRecords;     /**< Stores metadata for all loaded models. */
        std::unordered_map<i32, MeshGPUHandle> m_GPUMeshes;      /**< Stores OpenGL VAO/VBO/EBO handles for loaded meshes. */
        std::vector<u32> m_ModelIndexList;                       /**< Sequential list of model IDs for UI iteration. */
        std::unordered_map<std::string, u32> m_MeshIDbyName;     /**< Maps string names to internal Mesh IDs to prevent duplicates. */
        std::unordered_map<u32,u32> m_PreviewTextureIDs;

        /** @brief Cached RHI shader programs, keyed by their registration name. */
        std::unordered_map<std::string, ShaderProgramHandle> m_RhiShaders;
        /** @brief Cached material instances, keyed by their registration name. */
        std::unordered_map<std::string, std::shared_ptr<Material>> m_Materials;

        u32 m_NextMeshID = 0; /**< Autoincrementing counter for assigning unique Mesh IDs. */


        /**
         * @brief Iterates over CPU meshes and generates their corresponding OpenGL buffers (VAO/VBO/EBO).
         */
        void setupMeshes();

        /**
         * @brief Recursively processes Assimp nodes, extracting vertex and index data into Andromeda format.
         * @param meshId The internal Andromeda ID being assigned to this scene.
         * @param scene The parent Assimp scene.
         * @param node The current Assimp node being processed.
         */
        void processNode(uint32_t meshId, const aiScene* scene, aiNode* node, const std::string& modelDirectory);

        /**
         * @brief Records one imported material under its name and returns that name.
         * @param material The Assimp material to read.
         * @param modelDirectory Directory of the model file, the base for its relative texture paths.
         */
        std::string recordMaterial(const aiMaterial* material, const std::string& modelDirectory);

        /**
         * @brief Initiates the Assimp importer for a specific file and sets up the internal Mesh structure.
         * @param path The full system path to the 3D model file.
         */
        void loadScene(const std::string& path);
    };
}