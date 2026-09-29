#include "resource_manager.h"
#include "a_filesystem.hpp"
#include "a_geometry.hpp"
#include "a_primitiveGenerator.hpp"
#include "a_logger.hpp"
#include <string>
#define STB_IMAGE_IMPLEMENTATION

#include "stb_image.h"
#include <assimp/Importer.hpp>
#include "OpenGL/a_opengl_upload.hpp"
#include <assimp/postprocess.h>
#include "OpenGL/a_GLcubemap.hpp"
namespace Andromeda {
    void ResourceManager::start()
    {
        loadModels();
        loadDeviceIcons();
        loadEditorIcons();
        setupMeshes();
        registerDefaultPrimitives();
        loadAllCubeMaps();
        for (const auto& [name, data] : m_CubemapData) {
            A_DEBUG("Cubemap '{}' (ID {}) {}x{}", name, data.textureID, data.width, data.height);
            for (const auto& path : data.texturePath) {
                A_TRACE("    Path: {}", path);
            }
        }
    }

    void ResourceManager::loadModels()
    {
        std::array<std::string, 2> filter;
        filter[0] = ".fbx";
        filter[1] = ".obj";
        const std::vector<std::string> paths = Filesystem::getAllFilesInDirectoryRecursive(ASSET_PATH "models", filter);

        for (auto& v : paths) {
            loadScene(v);
        }
    }

    /**
 * @brief Scans and loads all cubemaps and environment maps from designated directories.
 * * This function handles two distinct loading paths:
 * 1. LDR Cubemaps: Scans the "/LDR" subdirectory for folders containing sets of 6 textures.
 * 2. HDR Panoramas: Scans the "/HDR" subdirectory for single-file .hdr or .hdri images.
 * *
 */
void ResourceManager::loadAllCubeMaps()
{
    std::vector<Filesystem::Directory> cubeMapLDR = Filesystem::getAllDirectories(CUBEMAP_PATH "/LDR");
    const std::vector<std::string> filter = {".hdr", ".hdri"};
    std::vector<std::string> cubeMapHDR = Filesystem::getAllFilesInDirectoryRecursive(CUBEMAP_PATH "/HDR", filter);

    if (!cubeMapLDR.empty())
    {
        for (auto& directory : cubeMapLDR)
        {
            loadAndStoreCubemap(directory.name, directory.files);
        }
    }

    if (!cubeMapHDR.empty())
    {
        for (std::string file : cubeMapHDR)
        {
            loadAndStoreCubemap(file);
        }
    }
}

/**
 * @brief Decodes image data from disk into CPU memory.
 * * Iterates through all registered paths in the CubemapData and uses stb_image
 * to load pixel data. It automatically switches between floating-point loading
 * for HDR and byte loading for standard LDR images.
 * * @param data Reference to the CubemapData structure to be populated with dimensions and pixels.
 * @note Pixel memory is allocated via stb_image and must be freed using CubemapData::freePixelData().
 */
void ResourceManager::loadCubemapTexture(CubemapData& data){
        for (u32 i = 0; i < data.texturePath.size(); i++) {
            const char* path = data.texturePath[i].c_str();
            if (data.isHDR)
            {
                stbi_set_flip_vertically_on_load(true);
                float* pixels = stbi_loadf(path, &data.width, &data.height, &data.channels, 0);
                data.pixelData[i] = pixels;
            }
            else
            {
                stbi_set_flip_vertically_on_load(false);
                unsigned char* pixels = stbi_load(path, &data.width, &data.height, &data.channels, 0);
                data.pixelData[i] = static_cast<void*>(pixels);
            }

            if (!data.pixelData[i])
            {
                A_ERROR("Failed to load cubemap face: {}", path);
            }
        }
}

/**
 * @brief Maps a file path to its corresponding cube face index based on naming conventions.
 * * Standard OpenGL Cubemap indexing:
 * - 0: Positive X (Right)
 * - 1: Negative X (Left)
 * - 2: Positive Y (Top)
 * - 3: Negative Y (Bottom)
 * - 4: Positive Z (Back)
 * - 5: Negative Z (Front)
 * *
 * * @param data The CubemapData structure where the path will be assigned.
 * @param path The file path to analyze.
 */
void ResourceManager::MapCubemapFacesLDR(CubemapData& data, const std::string& path)
{
    std::string fileName = Filesystem::getFileName(path);

    std::string lowerName = fileName;
    std::ranges::transform(lowerName, lowerName.begin(),
                           [](const unsigned char c){ return std::tolower(c); });

    if (lowerName.find("right") != std::string::npos)       data.texturePath[0] = path;
    else if (lowerName.find("left") != std::string::npos)   data.texturePath[1] = path;
    else if (lowerName.find("top") != std::string::npos)    data.texturePath[2] = path;
    else if (lowerName.find("bottom") != std::string::npos) data.texturePath[3] = path;
    else if (lowerName.find("back") != std::string::npos)   data.texturePath[4] = path;
    else if (lowerName.find("front") != std::string::npos)  data.texturePath[5] = path;
    else {
        A_WARN("Could not map cubemap face for file: {}", path);
    }
}
    /**
 * @brief Loads a cubemap from a set of 6 individual image files (LDR).
 * * This function is used for traditional sky boxes where each face of the cube
 * (+X, -X, +Y, -Y, +Z, -Z) is stored in a separate file. The files are
 * automatically mapped to their respective cube faces based on their filenames.
 * * @param name The unique identifier (key) to store the cubemap in the ResourceManager.
 * @param paths A vector containing exactly 6 file paths to the texture images.
 * * @note If the vector does not contain exactly 6 paths, the loading process
 * will be aborted and a warning will be logged to the console.
 * @warning Ensure that CubemapGL::CubemapTextureUploadGL(data) is called before
 * moving the data into the map to finalize the GPU upload.
 */
void ResourceManager::loadAndStoreCubemap(const std::string& name, const std::vector<std::string>& paths) {
    constexpr i32 cubemapSizeLDR = 6;
    if (paths.size() == cubemapSizeLDR)
    {
        CubemapData data;
        data.texturePath.resize(6);
        for (u32 i = 0; i < cubemapSizeLDR; i++)
        {
            MapCubemapFacesLDR(data, paths[i]);
        }

        loadCubemapTexture(data);
        CubemapGL::CubemapTextureUploadGL(data);
        m_CubemapData[name] = std::move(data);
    }
    else
    {
        A_WARN("Cubemap directory '{}' contains {} files! Skipping...", name, paths.size());
    }
}

/**
 * @brief Loads an environment map from a single HDR panorama file.
 * * This function automatically detects if the file is in a high dynamic range format.
 * The image is treated as an equirectangular panorama, commonly used for
 * Image Based Lighting (IBL) or high-quality sky boxes.
 * * @param file The full system path to the HDR/HDRI file.
 * * @note This sets the isHDR flag within CubemapData, which switches the
 * OpenGL upload process to use GL_RGB16F and GL_FLOAT for high precision.
 * @warning Ensure that CubemapGL::CubemapTextureUploadGL(data) is called before
 * moving the data into the map to finalize the GPU upload.
 */
void ResourceManager::loadAndStoreCubemap(const std::string& file) {
        if (stbi_is_hdr(file.c_str()))
        {
            CubemapData data;
            data.isHDR = true;
            data.texturePath.push_back(file);

            loadCubemapTexture(data);
            CubemapGL::CubemapTextureUploadGL(data);
            std::string name = Filesystem::getFileName(file);
            m_CubemapData[name] = std::move(data);
        }
}

    void ResourceManager::loadDeviceIcons()
    {
        std::array<std::string, 2> filter;
        filter[0] = ".png";
        filter[1] = ".jpg";
        const std::vector<std::string> paths = Filesystem::getAllFilesInDirectory(ASSET_PATH "icons/device/", filter);

        for (auto& v : paths) {
            std::string fileName = Filesystem::getFileName(v);
            deviceType type = findDeviceIcon(fileName);
            m_DeviceIcons[type] = CreateOpenGLTexture(v.c_str());
        }
    }

    u32 ResourceManager::getEditorIconID(const std::string& name)
    {
        auto it = m_EditorIcons.find(name);
        if (it != m_EditorIcons.end()) {
            return it->second.id;
        }
        throw std::runtime_error("Icon not found: " + name + "!");
    }

    u32 ResourceManager::registerCustomMesh(Mesh&& mesh, const std::string& name)
    {
        // Reuse an existing mesh registered under the same name to prevent duplicates
        // and to keep the name -> ID mapping stable across the session.
        if (const auto it = m_MeshIDbyName.find(name); it != m_MeshIDbyName.end()) {
            return it->second;
        }

        const uint32_t id = m_NextMeshID++;

        m_Meshes[id] = std::move(mesh);

        MeshGPUHandle& gpuHandle = m_GPUMeshes[id];
        createMesh(gpuHandle, m_Meshes[id]);

        m_ModelRecords[id] = ModelRecord{ id, name, deviceType::DEFAULT };
        m_ModelIndexList.push_back(id);
        m_MeshIDbyName.emplace(name, id);

        return id;
    }

    void ResourceManager::loadEditorIcons()
    {
        std::array<std::string,2> filter;
        filter[0] = ".png";
        filter[1] = ".jpg";
        const std::vector<std::string> paths = Filesystem::getAllFilesInDirectory(ASSET_PATH "icons/editor", filter);
        for (auto& v : paths)
        {
            std::string fileName = Filesystem::getFileName(v);
            m_EditorIcons[fileName] = CreateOpenGLTexture(v.c_str());
        }
    }

    deviceType ResourceManager::findDeviceIcon(const std::string &iconName) {
        for (auto& v : Filesystem::m_DirectoryNames) {
            if (v.first.find(iconName) != std::string::npos) {
                return v.second;
            }
        }
        return deviceType::DEFAULT;
    };

    u32 ResourceManager::getMeshVaoByID(const u32 meshID) const
    {
		auto it = m_GPUMeshes.find(meshID);
		if (it != m_GPUMeshes.end())
		{
			return it->second.vao;
		}
        A_WARN("Mesh ID {} not found!", meshID);
        return 0;
    }

    u32 ResourceManager::getMeshIndexSizeByID(const u32 meshID) const {
		auto it = m_Meshes.find(meshID);
        if (it != m_Meshes.end())
        {
			return it->second.indexBuffer.size();
        }
        A_WARN("Mesh ID {} not found!", meshID);
        return 0;
    }

    const Mesh& ResourceManager::getMeshByID(const u32 meshID) const
    {
        return m_Meshes.at(meshID);
    }

    std::string ResourceManager::getMeshNameByID(const u32 id) const
    {
        const auto it = m_ModelRecords.find(id);
        return it != m_ModelRecords.end() ? it->second.name : std::string{};
    }

    bool ResourceManager::tryGetMeshIDByName(const std::string& name, u32& out) const
    {
        const auto it = m_MeshIDbyName.find(name);
        if (it == m_MeshIDbyName.end()) return false;
        out = it->second;
        return true;
    }

    void ResourceManager::registerDefaultPrimitives()
    {
        Mesh cube;
        PrimitiveGenerator::generateCube(cube);
        registerCustomMesh(std::move(cube), "Default_Cube");

        Mesh plane;
        PrimitiveGenerator::generatePlane(plane);
        registerCustomMesh(std::move(plane), "Default_Plane");

        Mesh sphere;
        PrimitiveGenerator::generateSphere(sphere);
        registerCustomMesh(std::move(sphere), "Default_Sphere");
    }

    u32 ResourceManager::getModelRecordsSize() const
    {
        return m_ModelRecords.size();
    }

    const std::unordered_map<uint32_t, ModelRecord>& ResourceManager::getModelRecords() const
    {
        return m_ModelRecords;
    }

    u32 ResourceManager::getModelCount() const {
        return static_cast<u32>(m_ModelRecords.size());
    }
    const ModelRecord& ResourceManager::getModelData(u32 index) const {
        u32 modelID = m_ModelIndexList.at(index);
        return m_ModelRecords.at(modelID);
    }

    u32 ResourceManager::getDeviceIconID(const deviceType type) const {
        return m_DeviceIcons.at(type).id;
    }

    GLtexture ResourceManager::CreateOpenGLTexture(const char* path)
    {
        GLtexture texture;
        i32 w, h, channels;
        unsigned char* pixels = stbi_load(path, &w, &h, &channels, 4);
        if (!pixels) {
            const std::string log = stbi_failure_reason();
            throw std::runtime_error("Failed to load image: " + log);
        }

        texture.w = w; texture.h = h;

        glGenTextures(1, &texture.id);
        glBindTexture(GL_TEXTURE_2D, texture.id);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

        stbi_image_free(pixels);
        return texture;
    }


    u32 ResourceManager::loadModelTexture(const std::string& path, const bool srgb)
    {
        if (path.empty()) {
            return 0;
        }
        if (const auto it = m_ModelTextures.find(path); it != m_ModelTextures.end()) {
            return it->second.id;
        }

        GLtexture texture;
        i32 w, h, channels;
        unsigned char* pixels = stbi_load(path.c_str(), &w, &h, &channels, 4);
        if (!pixels) {
            A_WARN("Model texture could not be loaded ({}): {}", stbi_failure_reason(), path);
            m_ModelTextures.emplace(path, GLtexture{}); // remember the failure, do not retry every frame
            return 0;
        }

        texture.w = w;
        texture.h = h;

        glGenTextures(1, &texture.id);
        glBindTexture(GL_TEXTURE_2D, texture.id);

        // Surface textures repeat and are minified far more often than icons, so they need
        // wrapping and mipmaps - without them distant geometry aliases badly.
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, srgb ? GL_SRGB8_ALPHA8 : GL_RGBA8, w, h, 0,
                     GL_RGBA, GL_UNSIGNED_BYTE, pixels);
        glGenerateMipmap(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, 0);

        stbi_image_free(pixels);

        // GLtexture owns its GL object and is move-only, so the id is read before handing it
        // over - after the move the local copy is reset to 0.
        const u32 id = texture.id;
        m_ModelTextures.emplace(path, std::move(texture));
        return id;
    }

    void ResourceManager::setupMeshes()
    {
        for (auto& [id, mesh] : m_Meshes)
        {
            MeshGPUHandle& gpuHandle = m_GPUMeshes[id];
            createMesh(gpuHandle, mesh);
        }
    }
    /**
     * @brief Records what one imported material declares, so the renderer can build it later.
     * @details Texture paths in a model file are relative to the file itself, so they are resolved
     *          against its directory here - the renderer no longer knows where the model came from.
     * @param material The Assimp material to read.
     * @param modelDirectory Directory of the model file, used as the base for relative paths.
     * @return The material name, which is also the key the submesh refers to.
     */
    std::string ResourceManager::recordMaterial(const aiMaterial* material, const std::string& modelDirectory)
    {
        aiString nameValue;
        std::string name = (material->Get(AI_MATKEY_NAME, nameValue) == AI_SUCCESS)
                               ? std::string(nameValue.C_Str())
                               : std::string("UnnamedMaterial");

        if (m_MaterialDefs.contains(name)) {
            return name;
        }

        MaterialDef def;
        def.name = name;

        aiColor3D diffuse(1.0f, 1.0f, 1.0f);
        if (material->Get(AI_MATKEY_COLOR_DIFFUSE, diffuse) == AI_SUCCESS) {
            def.albedo = vec3(diffuse.r, diffuse.g, diffuse.b);
        }

        // Wavefront stores a specular exponent (Ns, 0..1000), the renderer wants roughness.
        // Higher exponent means a tighter highlight, so the two run in opposite directions.
        float shininess = 0.0f;
        if (material->Get(AI_MATKEY_SHININESS, shininess) == AI_SUCCESS && shininess > 0.0f) {
            def.roughness = std::clamp(1.0f - std::sqrt(shininess / 1000.0f), 0.05f, 1.0f);
        }

        // map_Kd, map_Ns, map_Ke and map_Bump in the order Assimp reports them for Wavefront.
        // map_Bump arrives as HEIGHT rather than NORMALS, so both are tried.
        const auto texturePath = [&](const aiTextureType type) -> std::string {
            aiString relative;
            if (material->GetTexture(type, 0, &relative) != AI_SUCCESS) {
                return {};
            }
            return Filesystem::resolveRelativeTo(modelDirectory, relative.C_Str());
        };

        def.albedoMap = texturePath(aiTextureType_DIFFUSE);
        def.roughnessMap = texturePath(aiTextureType_SHININESS);
        def.emissiveMap = texturePath(aiTextureType_EMISSIVE);
        def.normalMap = texturePath(aiTextureType_NORMALS);
        if (def.normalMap.empty()) {
            def.normalMap = texturePath(aiTextureType_HEIGHT);
        }

        m_MaterialDefs.emplace(name, std::move(def));
        return name;
    }

    void ResourceManager::processNode(const uint32_t meshId, const aiScene* scene, aiNode* node,
                                      const std::string& modelDirectory)
    {
        auto& newMesh = m_Meshes.at(meshId);
        for (unsigned int i = 0; i < node->mNumMeshes; i++) {
            aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
            aiColor3D color(1.0f, 1.0f, 1.0f);
            aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];

            std::vector<unsigned int> indexBuffer;
            std::vector<Vertex> vertices;
            vertices.reserve(mesh->mNumVertices);

            bool hasNormals = mesh->HasNormals();
            if (!hasNormals) {
                A_WARN("Mesh vertex has no normals");
            }
            material->Get(AI_MATKEY_COLOR_DIFFUSE, color);
            for (unsigned int j = 0; j < mesh->mNumVertices; j++) {
                Vertex vertex{};
                aiVector3D worldPos = mesh->mVertices[j];
                vertex.pos = glm::vec3(worldPos.x, worldPos.y, worldPos.z);

                if (hasNormals) {
                    vertex.normal = glm::vec3(mesh->mNormals[j].x, mesh->mNormals[j].y, mesh->mNormals[j].z);
                }

                if (mesh->HasTextureCoords(0)) {
                    vertex.uv = glm::vec2(mesh->mTextureCoords[0][j].x, mesh->mTextureCoords[0][j].y);
                }
                vertex.color = glm::vec3(color.r, color.g, color.b);
                vertices.push_back(vertex);
            }

            auto& vb = newMesh.vertexbuffer;
            auto& ib = newMesh.indexBuffer;

            const uint32_t baseVertex = static_cast<uint32_t>(vb.size());
            const uint32_t indexStart = static_cast<uint32_t>(ib.size());

            vb.insert(vb.end(), vertices.begin(), vertices.end());

            for (unsigned int k = 0; k < mesh->mNumFaces; k++) {
                const aiFace& f = mesh->mFaces[k];
                for (unsigned int t = 0; t < f.mNumIndices; t++) {
                    ib.push_back(baseVertex + f.mIndices[t]);
                }
            }

            // One submesh per imported mesh: they share the buffers above, but each keeps its
            // own material, so a room does not end up painted in a single texture.
            newMesh.submeshes.push_back(Submesh{
                indexStart,
                static_cast<uint32_t>(ib.size()) - indexStart,
                recordMaterial(material, modelDirectory)
            });
        }
        for (unsigned int i = 0; i < node->mNumChildren; i++) {
            processNode(meshId, scene, node->mChildren[i], modelDirectory);
        }
    }

    void ResourceManager::loadScene(const std::string& path) {
        Assimp::Importer importer;
        const aiScene* scene = importer.ReadFile(
            path.c_str(),
            aiProcess_Triangulate |
            aiProcess_ConvertToLeftHanded |
            aiProcess_FlipUVs |
            aiProcess_GenNormals
        );

        if (!scene) {
            throw std::runtime_error(importer.GetErrorString());
        }

        std::string meshName = Filesystem::getFileName(path);
        if (m_MeshIDbyName.contains(meshName)) {
            A_DEBUG("Mesh '{}' already exists, skipping load", meshName);
            return;
        }

        const uint32_t id = m_NextMeshID++;
        const deviceType dt = Filesystem::findDeviceTypeByPath(path);

        m_Meshes[id] = Andromeda::Mesh{};

        m_ModelRecords[id] = ModelRecord{id,meshName,dt};
        m_ModelIndexList.push_back(id);

        m_MeshIDbyName.emplace(meshName, id);

        aiNode* rootNode = scene->mRootNode;
        processNode(id, scene, rootNode, Filesystem::getDirectory(path));
    }


    ShaderProgramHandle ResourceManager::loadShaderRHI(IGraphicsContext* ctx, const std::string& name, const std::string& vertPath, const std::string& fragPath) {
        auto it = m_RhiShaders.find(name);
        if (it != m_RhiShaders.end()) {
            return it->second;
        }

        ShaderProgramHandle handle = ctx->createShaderProgram(vertPath, fragPath);
        m_RhiShaders[name] = handle;
        return handle;
    }

    ShaderProgramHandle ResourceManager::loadComputeShaderRHI(IGraphicsContext* ctx, const std::string& name, const std::string& computePath) {
        auto it = m_RhiShaders.find(name);
        if (it != m_RhiShaders.end()) {
            return it->second;
        }

        ShaderProgramHandle handle = ctx->createComputeProgram(computePath);
        m_RhiShaders[name] = handle;
        return handle;
    }

    ShaderProgramHandle ResourceManager::loadComputeShaderRHI(IGraphicsContext* ctx, const std::string& name, const std::string& computePath, const char* stageName) {
        auto it = m_RhiShaders.find(name);
        if (it != m_RhiShaders.end()) {
            return it->second;
        }

        std::string entryPoint = computePath;
        size_t spvPos = entryPoint.rfind(".spv");
        if (spvPos != std::string::npos) {
            entryPoint = entryPoint.substr(0, spvPos);
        }
        size_t lastDot = entryPoint.find_last_of('.');
        if (lastDot != std::string::npos) {
            entryPoint = entryPoint.substr(lastDot + 1);
        }

        ShaderProgramHandle handle = ctx->createComputeProgram(computePath, entryPoint.c_str(), stageName);
        m_RhiShaders[name] = handle;
        return handle;
    }

    ShaderProgramHandle ResourceManager::getShaderRHI(const std::string& name) const {
        auto it = m_RhiShaders.find(name);
        if (it != m_RhiShaders.end()) [[likely]] {
            return it->second;
        }
        return ShaderProgramHandle{ 0 };
    }

    std::shared_ptr<Material> ResourceManager::createMaterial(const std::string& materialName, ShaderProgramHandle shaderHandle, IGraphicsContext* ctx) {
        auto it = m_Materials.find(materialName);
        if (it != m_Materials.end()) {
            return it->second;
        }

        auto newMaterial = std::make_shared<Material>(shaderHandle, ctx);
        m_Materials[materialName] = newMaterial;

        return newMaterial;
    }

    std::shared_ptr<Material> ResourceManager::getMaterial(const std::string& materialName) const {
        auto it = m_Materials.find(materialName);
        if (it != m_Materials.end()) [[likely]] {
            return it->second;
        }
        return nullptr;
    }

    void ResourceManager::destroyRHIResources(IGraphicsContext* ctx) {
        for (auto& [name, handle] : m_RhiShaders) {
            ctx->destroyShaderProgram(handle);
        }
        m_RhiShaders.clear();
        m_Materials.clear();
    }
}