#include "Renderer.h"
#include "a_subsystem_manager.hpp"
#include "window_manager.hpp"
#include "resource_manager.h"
#include "a_math.hpp"
#include "scene.hpp"
#include "a_event_manager.hpp"
#include "OpenGL/a_GLcubemap.hpp"
#include "a_PrimitiveGenerator.hpp"
#include "OpenGL/a_opengl_upload.hpp"
#include "a_shader_generated.hpp"
#include "a_clearFlags.hpp"
#include "a_colors.hpp"
#include "a_logger.hpp"
#include <algorithm>
#include "a_AssetPreviewRenderer.hpp"

using namespace Andromeda::ECS;
namespace Andromeda {

    auto Renderer::initRenderer() -> void
    {
        m_GLContext = std::make_unique<OpenGLContext>();
        m_RenderContext = m_GLContext.get();
        m_RenderContext->initRenderContext();
        m_ResourceManager = SystemManager::getInstance().getSubsystem<ResourceManager>();
        m_SceneManager = SystemManager::getInstance().getSubsystem<SceneManager>();
        assert(m_ResourceManager && "ResourceManager is nullptr in Renderer::Start()");
        assert(m_SceneManager && "SceneManager is nullptr in Renderer::Start()");
        m_Cam = &m_SceneManager->m_EditorCamData;

        m_FrameConstants.initialize();

        m_ColorUBO.initialize(sizeof(Generated::ColorBuffer));
        m_GridUBO.initialize(sizeof(Generated::GridBuffer));
        m_GridParamsUBO.initialize(sizeof(Generated::GridParamsBuffer));
        m_OutlineUBO.initialize(sizeof(Generated::OutlineParamsBuffer));

        m_pbrMaterialUBO.initialize(sizeof(Generated::pbrMaterial));
        m_AssetPreviewRenderer = std::make_unique<AssetPreviewRenderer>(m_ResourceManager, m_RenderContext,m_ResourceManager, &m_FrameConstants);
    }

    u64 createEmitterKey(const Entity id, const u32 groupId)
    {
        return static_cast<u64>(id) << 32 | groupId;
    }

    void Renderer::particleUpdatePass()
    {
        m_UsedKeys.clear();
        auto& registry = m_SceneManager->m_Registry;
        auto& particleSystemPool = registry.getPool<Component::ParticleSystem>();
        auto& transformPool = registry.getPool<Component::Transform>();
        m_GroupRanges.clear();
        u32 base = 0;
        for (const Entity entity : particleSystemPool.getEntities())
        {
            auto& system = particleSystemPool.get(entity);
            if (transformPool.has(entity))
            {
                auto& transform = transformPool.get(entity);

                for (auto& group : system.getParticleGroups())
                {
                    const u32 count = static_cast<u32>(std::clamp(group.particleCount,0,kMaxParticlesPerGroup));

                    auto& range = m_GroupRanges.emplace_back(entity, group.id, base, count);
                    base += count;
                    const u64 key = createEmitterKey(entity, group.id);
                    m_UsedKeys.insert(key);
                    auto &emitter = m_Emitters[key];
                    if (!emitter.isInitialized())
                    {
                        emitter.initialize(*m_RenderContext, m_ResourceManager, *m_Cam);
                    }
                    emitter.applyGroup(group, transform.position, range);
                    emitter.update();

                }
            }
        }

        std::erase_if(m_Emitters, [&](const auto& entry)
        {
            return !m_UsedKeys.contains(entry.first);
        });
    }

    void Renderer::prefilterCubemapBaking(){
        ShaderProgramHandle prefilterMapHandle = m_ResourceManager->loadShaderRHI(m_RenderContext,"Prefilter_Shader", SHADER_PATH "equirect.vert", SHADER_PATH "PBR/prefilter.frag");
        RenderPassSpecs prefilterSpecs;
        prefilterSpecs.depthTest = false;
        prefilterSpecs.cullMode = CullMode::None;
        m_RenderContext->setRenderPassSpecs(prefilterSpecs);
        
        m_RenderContext->bindShaderProgram(prefilterMapHandle);
        m_RenderContext->setParameter(prefilterMapHandle, "environmentMap", 0);
        m_RenderContext->setParameter(prefilterMapHandle, "proj", CubemapGL::cubeProjection);
        m_RenderContext->bindTextureCube(0, m_EnvironmentCubemap);
        m_RenderContext->bindFramebuffer(m_BakingBuffer.get());
        u32 maxMipLevels = 5;

        for (u32 mip = 0; mip < maxMipLevels; ++mip) {
            u32 mipWidth = 128 * std::pow(0.5, mip);
            u32 mipHeight = 128 * std::pow(0.5, mip);
            
            m_RenderContext->setViewport(0,0, mipWidth, mipHeight);
            float roughness = static_cast<float>(mip) / static_cast<float>(maxMipLevels - 1);
            m_RenderContext->setParameter(prefilterMapHandle, "roughness", roughness);

            for (u32 i = 0; i < 6; ++i) {
                m_RenderContext->setParameter(prefilterMapHandle, "view", CubemapGL::cubeViews[i]);
                m_RenderContext->framebufferTexture2D(i,m_PrefilterMap,mip);
                m_RenderContext->clear(ClearFlags::Color);
                m_RenderContext->drawIndexed(cubemapGpuHandle.vao, 36);
            }
        }
        m_RenderContext->bindFramebuffer(nullptr);
    }

    void Renderer::irradianceCubemapBaking()
    {
        RenderPassSpecs specs;
        specs.cullMode = CullMode::None;
        specs.depthTest = false;
        m_RenderContext->setRenderPassSpecs(specs);
        m_BakingBuffer->resize(ivec2(32, 32));
        m_IrradianceCubemap.height = 32;
        m_IrradianceCubemap.width = 32;
        SamplerState irradianceSampler;
        irradianceSampler.type = TextureType::Cubemap;
        irradianceSampler.minFilter = FilterModeMin::Linear;
        irradianceSampler.magFilter = FilterModeMag::Linear;
        CubemapGL::AllocateCubemapTexture(m_RenderContext, &m_IrradianceCubemap, irradianceSampler);

        m_PrefilterMap.height = 128;
        m_PrefilterMap.width = 128;
        SamplerState prefilterSampler;
        prefilterSampler.type = TextureType::Cubemap;
        prefilterSampler.minFilter = FilterModeMin::Trillinear;
        prefilterSampler.magFilter = FilterModeMag::Linear;
        CubemapGL::AllocateCubemapTextureWithMipmap(m_RenderContext, &m_PrefilterMap, prefilterSampler);

        ShaderProgramHandle irradianceShaderHandle = m_ResourceManager->loadShaderRHI(m_RenderContext, "Irradiance_Shader", SHADER_PATH "equirect.vert", SHADER_PATH "irradiance.frag");
        m_RenderContext->bindShaderProgram(irradianceShaderHandle);
        m_RenderContext->bindTextureCube(0,m_EnvironmentCubemap);

        m_RenderContext->setParameter(irradianceShaderHandle, "proj", CubemapGL::cubeProjection);
        m_RenderContext->setViewport(0,0,32,32);
        m_RenderContext->bindFramebuffer(m_BakingBuffer.get());
        for (u32 i = 0; i < 6; ++i) {
            m_RenderContext->setParameter(irradianceShaderHandle, "view", CubemapGL::cubeViews[i]);
            m_RenderContext->attachCubemapFace(i, m_IrradianceCubemap.textureID);
            m_RenderContext->clear(ClearFlags::Color | ClearFlags::Depth, vec4(0.0f, 0.0f, 0.0f, 1.0f));
            m_RenderContext->drawIndexed(cubemapGpuHandle.vao, 36);
        }
        m_RenderContext->unbindFramebuffer();
        m_RenderContext->setViewport(0,0,512,512);
        m_BakingBuffer->resize(ivec2(512, 512));
        RenderPassSpecs resetSpecs;
        m_RenderContext->setRenderPassSpecs(resetSpecs);
    }
    /// <summary>
    /// TODO: remove this api specific function and add uvs to the mesh class
    /// </summary>
    unsigned int quadVAO = 0; ///< VAO of the screen-space quad used for full-screen passes (e.g., BRDF LUT baking); lazily created on the first renderQuad () call.
    unsigned int quadVBO; ///< VBO backing quadVAO's position + UV vertex attributes.

    /**
     * @brief Draws a full-screen quad (position + UV) with a triangle strip, lazily creating its VAO/VBO on first call.
     * @details Used by passes that need to run a shader over every pixel of the current render
     *          target without real scene geometry (e.g., the BRDF LUT integration pass in @c brdfLUTBaking()).
     */
    void renderQuad()
    {
        if (quadVAO == 0)
        {
            float quadVertices[] = {
                -1.0f,  1.0f, 0.0f, 0.0f, 1.0f,
                -1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
                 1.0f,  1.0f, 0.0f, 1.0f, 1.0f,
                 1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
            };
            glGenVertexArrays(1, &quadVAO);
            glGenBuffers(1, &quadVBO);
            glBindVertexArray(quadVAO);
            glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
            glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), static_cast<void*>(nullptr));
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), reinterpret_cast<void*>(3 * sizeof(float)));
        }
        glBindVertexArray(quadVAO);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        glBindVertexArray(0);
    }
    void Renderer::brdfLUTBaking() {
        
        RenderPassSpecs specs;
        specs.cullMode = CullMode::None;
        specs.depthTest = false;
        m_RenderContext->setRenderPassSpecs(specs);

        ShaderProgramHandle brdfShaderHandle = m_ResourceManager->loadShaderRHI(m_RenderContext, "BRDF_Shader", SHADER_PATH "PBR/brdf.vert", SHADER_PATH "PBR/brdf.frag");
        
        SamplerState brdfSampler;
        brdfSampler.type = TextureType::Texture2D;
        brdfSampler.minFilter = FilterModeMin::Linear;
        brdfSampler.magFilter = FilterModeMag::Linear;
        brdfSampler.wrapS = WrapMode::ClampToEdge;
        brdfSampler.wrapT = WrapMode::ClampToEdge;

        m_BrdfLUTTexture = m_RenderContext->generateTexture(512, 512, brdfSampler, TextureFormat::RG16_FLOAT);
        m_RenderContext->allocateTexture(m_BrdfLUTTexture);
        m_RenderContext->bindSamplerState(m_BrdfLUTTexture.textureID, brdfSampler);

        m_RenderContext->setViewport(0, 0, 512, 512);
        m_RenderContext->bindFramebuffer(m_BakingBuffer.get());

        m_RenderContext->bindToTarget(m_BrdfLUTTexture);
        m_RenderContext->framebufferTexture2D(m_BrdfLUTTexture, 0);
        m_RenderContext->bindShaderProgram(brdfShaderHandle);
        m_RenderContext->clear(ClearFlags::Color);
        renderQuad();
        m_RenderContext->unbindTexture();
        m_RenderContext->bindFramebuffer(nullptr);

        RenderPassSpecs resetSpecs;
        m_RenderContext->setRenderPassSpecs(resetSpecs);
    }

    void Renderer::start()
    {
        initRenderer();
        m_FramebufferSize = ivec2(Window::g_WindowWidth, Window::g_WindowHeight);

        if (m_FramebufferSize.x == 0 || m_FramebufferSize.y == 0) {
            throw std::runtime_error("Framebuffer has an start value of 0!!!");
        }
        m_Cam->framebufferSize = m_FramebufferSize;
        m_TexelSize = 1.0f / vec2(m_FramebufferSize.x, m_FramebufferSize.y);

        createFramebuffers();
        m_EnvironmentCubemap.width = 512;
        m_EnvironmentCubemap.height = 512;
        SamplerState environmentMap;
        environmentMap.type = TextureType::Cubemap;
        environmentMap.minFilter = FilterModeMin::Trillinear;
        environmentMap.magFilter = FilterModeMag::Linear;
        CubemapGL::AllocateCubemapTexture(m_RenderContext,&m_EnvironmentCubemap, environmentMap);

        u32 hdrMapID = m_ResourceManager->m_CubemapData["citrus_orchard_road_puresky_4k"].textureID;
        Mesh cubeMesh;
        PrimitiveGenerator::generateCube(cubeMesh);

        createMesh(cubemapGpuHandle, cubeMesh);

        ShaderProgramHandle equirectangularHandle = m_ResourceManager->loadShaderRHI(m_RenderContext, "equirectangular_Shader", SHADER_PATH "equirect.vert", SHADER_PATH "equirect.frag");
        CubemapGL::ConvertEquiretangularToCubemap(
            m_RenderContext, equirectangularHandle,
            hdrMapID,
            m_BakingBuffer.get(),
            m_EnvironmentCubemap.textureID,
            [&]() {
                m_RenderContext->drawIndexed(cubemapGpuHandle.vao, 36);
            }
        );

        m_RenderContext->bindTextureCube(0,m_EnvironmentCubemap);
        m_RenderContext->generateMipmap(TextureType::Cubemap);

        irradianceCubemapBaking();
        prefilterCubemapBaking();
        m_CubeVao = m_RenderContext->createEmptyVAO();
        brdfLUTBaking();
        createMaterials();
        m_AssetPreviewRenderer->initialize();
        RenderPassSpecs initSpecs;
        m_RenderContext->setRenderPassSpecs(initSpecs);
        registerEvents();
    }

    std::shared_ptr<RHIFramebuffer> Renderer::helperCreateFBO(const ivec2 size, const std::vector<FramebufferTextureFormat>& formats, u32 samples) {
        FramebufferSpecification specs;
        specs.width = size.x;
        specs.height = size.y;
        specs.samples = samples;

        for (auto f : formats) {
            FramebufferTextureSpecification texSpec{ .textureFormat = f };
            specs.attachments.push_back(texSpec);
        }
        return m_RenderContext->createFramebuffer(specs);
    }
    void Renderer::createFramebuffers()
    {
        m_MsaaBuffer = helperCreateFBO(m_FramebufferSize, { FramebufferTextureFormat::RGBA8, FramebufferTextureFormat::DEPTH24Stencil8 }, 4);
        m_SceneBuffer = helperCreateFBO(m_FramebufferSize, { FramebufferTextureFormat::RGBA8 }, 1);
        m_SelectionBuffer = helperCreateFBO(m_FramebufferSize, { FramebufferTextureFormat::None, FramebufferTextureFormat::DEPTH24Stencil8 }, 1);
        m_PostprocessBuffer = helperCreateFBO(m_FramebufferSize, { FramebufferTextureFormat::RGBA8 }, 1);
        m_BakingBuffer = helperCreateFBO(ivec2(512, 512), { FramebufferTextureFormat::DEPTH32F, FramebufferTextureFormat::RGBA16F }, 1);
    }

    void Renderer::createMaterials()
    {
        ShaderProgramHandle outlineShaderHandle = m_ResourceManager->loadShaderRHI(m_RenderContext, "Outline_Shader", SHADER_PATH "outline.vert", SHADER_PATH "outline.frag");
        auto outlineMaterial = m_ResourceManager->createMaterial("OutlineMaterial", outlineShaderHandle, m_RenderContext);

        ShaderProgramHandle maskShaderHandle = m_ResourceManager->loadShaderRHI(m_RenderContext, "Mask_Shader", SHADER_PATH "mask.vert", SHADER_PATH "mask.frag");
        m_ResourceManager->createMaterial("SelectionMaterial", maskShaderHandle, m_RenderContext);

        ShaderProgramHandle skyboxShaderHandle = m_ResourceManager->loadShaderRHI(m_RenderContext, "Skybox_Shader", SHADER_PATH "skybox.vert", SHADER_PATH "skybox.frag");
        auto skyboxMaterial = m_ResourceManager->createMaterial("SkyboxMaterial", skyboxShaderHandle, m_RenderContext);

        ShaderProgramHandle pbrShaderHandle = m_ResourceManager->loadShaderRHI(m_RenderContext, "PBR_Shader", SHADER_PATH "PBR/pbr.vert", SHADER_PATH "PBR/pbr.frag");
        if (auto pbrMaterial = m_ResourceManager->createMaterial("PBRMaterial", pbrShaderHandle, m_RenderContext)) {
            Generated::pbrMaterial plasticData;
            // Linearized: the palette holds display-space values, the shader works in linear space.
            plasticData.albedo = vec3(srgbToLinear(Colors::LightGrey));
            plasticData.metallic = 0.0f;
            plasticData.roughness = 0.8f;
            plasticData.ao = 1.0f;
            // Must be zero: a stray value here would switch on samplers that hold no texture.
            plasticData.textureFlags = 0;

            TextureBinding irradianceBinding, prefilterBinding;;
            irradianceBinding.apiID = m_IrradianceCubemap.textureID;

            prefilterBinding.apiID = m_PrefilterMap.textureID;
            pbrMaterial->setUBOData(plasticData, 2, m_RenderContext);
            pbrMaterial->addTexture({ m_IrradianceCubemap.textureID, 0 });
            pbrMaterial->addTexture({ m_PrefilterMap.textureID, 1 });
            pbrMaterial->addTexture({ m_BrdfLUTTexture.textureID, 2 });
        }

        createImportedMaterials(pbrShaderHandle);
    }

    /**
     * @brief Turns everything the model importer recorded into real GPU materials.
     * @details The importer only sees files, so it stores intent in ResourceManager::m_MaterialDefs.
     *          Here each definition becomes a Material on the shared PBR shader, with its maps
     *          bound on top of the three IBL textures every PBR material needs. Submeshes find
     *          them again by material name.
     * @param pbrShaderHandle The PBR shader all imported materials share.
     */
    void Renderer::createImportedMaterials(const ShaderProgramHandle pbrShaderHandle)
    {
        for (const auto& [name, def] : m_ResourceManager->m_MaterialDefs) {
            auto material = m_ResourceManager->createMaterial(name, pbrShaderHandle, m_RenderContext);
            if (!material) {
                A_WARN("Imported material '{}' could not be created", name);
                continue;
            }

            // Colour maps are authored in sRGB and must be linearised, data maps must not be.
            const u32 albedoTex = m_ResourceManager->loadModelTexture(def.albedoMap, true);
            const u32 emissiveTex = m_ResourceManager->loadModelTexture(def.emissiveMap, true);
            const u32 roughnessTex = m_ResourceManager->loadModelTexture(def.roughnessMap, false);
            const u32 normalTex = m_ResourceManager->loadModelTexture(def.normalMap, false);

            u32 flags = 0;
            if (albedoTex != 0)    flags |= 1u;  // HAS_ALBEDO_MAP
            if (roughnessTex != 0) flags |= 2u;  // HAS_ROUGHNESS_MAP
            if (normalTex != 0)    flags |= 4u;  // HAS_NORMAL_MAP
            if (emissiveTex != 0)  flags |= 8u;  // HAS_EMISSIVE_MAP

            Generated::pbrMaterial data;
            data.albedo = def.albedo;
            data.metallic = def.metallic;
            data.roughness = def.roughness;
            data.ao = 1.0f;
            // The reflector may type the flag word as signed, so convert to whatever it chose.
            data.textureFlags = static_cast<decltype(data.textureFlags)>(flags);

            material->setUBOData(data, 2, m_RenderContext);
            material->addTexture({ m_IrradianceCubemap.textureID, 0 });
            material->addTexture({ m_PrefilterMap.textureID, 1 });
            material->addTexture({ m_BrdfLUTTexture.textureID, 2 });
            if (albedoTex != 0)    material->addTexture({ albedoTex, 3 });
            if (roughnessTex != 0) material->addTexture({ roughnessTex, 4 });
            if (normalTex != 0)    material->addTexture({ normalTex, 5 });
            if (emissiveTex != 0)  material->addTexture({ emissiveTex, 6 });
        }
    }

    void Renderer::registerEvents()
    {
        EventManager::getInstance().AddEventListener<SceneObjectSelected>([this](const SceneObjectSelected& event) {
            this->m_SelectedForHighlighting = event.entity;
            });

        EventManager::getInstance().AddEventListener<OnEnableWireFrame>([this](const OnEnableWireFrame& event) {
            this->m_WireframeActive = event.state;
            });
    }

    void Renderer::update()
    {
        processResizeTimer();
        windowClearPass();
        scenePassBegin();
        particleUpdatePass();
        geometryPass();
        proceduralPass();
        vfxPass();
        scenePassEndResolve();
        selectionPass(m_SelectedForHighlighting);
        postprocessingPass();
        m_RenderContext->m_FrameCount++;
    }

    void Renderer::destroy() {
        m_RenderContext->deleteVertexArrays(m_CubeVao);
    }

    void Renderer::setActiveCamera(amath::CameraData* camData) {
        assert(camData && "CameraData is nullptr in Renderer::setActiveCamera()");
        m_Cam = camData;
    }

    void Renderer::scenePassBegin() const {
        if (!m_Cam) return;

        m_RenderContext->bindFramebuffer(m_MsaaBuffer.get());
        m_RenderContext->clear(ClearFlags::Color | ClearFlags::Depth, vec4(0.2f, 0.2f, 0.35f, 1.0f));

        Generated::CameraBuffer camData;
        camData.viewMatrix = m_Cam->viewMatrix;
        camData.projMatrix = m_Cam->projection;
        camData.camPos = m_Cam->cameraPos;
        auto& mutableCamUBO = const_cast<RHIConstantBuffer&>(m_FrameConstants.cameraUBO);
        mutableCamUBO.setData(&camData, sizeof(Generated::CameraBuffer));
        mutableCamUBO.bind(0);

        Generated::lights lightData;

        lightData.lightPositions[0] = vec4(5.0f, 5.0f, 5.0f, 1.0f);
        lightData.lightPositions[1] = vec4(-5.0f, 5.0f, 5.0f, 1.0f);
        lightData.lightPositions[2] = vec4(5.0f, -5.0f, 5.0f, 1.0f);
        lightData.lightPositions[3] = vec4(-5.0f, -5.0f, 5.0f, 1.0f);

        lightData.lightColors[0] = vec4(300.0f, 300.0f, 300.0f, 1.0f);
        lightData.lightColors[1] = vec4(300.0f, 300.0f, 300.0f, 1.0f);
        lightData.lightColors[2] = vec4(300.0f, 300.0f, 300.0f, 1.0f);
        lightData.lightColors[3] = vec4(300.0f, 300.0f, 300.0f, 1.0f);

        auto& mutableLightUBO = const_cast<RHIConstantBuffer&>(m_FrameConstants.lightUBO);
        mutableLightUBO.setData(&lightData, sizeof(Generated::lights));
        mutableLightUBO.bind(3);
    }

	void Renderer::vfxPass() {
        RenderPassSpecs specs;
        specs.blendMode = BlendMode::AlphaBlend;
        specs.depthWrite = false;
        m_RenderContext->setRenderPassSpecs(specs);
		for (auto& val : m_Emitters | std::views::values)
		{
		    val.render();
		}
        RenderPassSpecs resetSpecs;
        m_RenderContext->setRenderPassSpecs(resetSpecs);
	}

    void Renderer::geometryPass() const {
        RenderPassSpecs specs;
        if (m_WireframeActive) {
            specs.rasterizerMode = RasterizerMode::Wireframe;
            specs.cullMode = CullMode::None;
        }
        else {
            specs.rasterizerMode = RasterizerMode::Fill;
            specs.cullMode = CullMode::Back;
        }
        specs.depthTest = true;
        m_RenderContext->setRenderPassSpecs(specs);

        auto& meshPool = m_SceneManager->m_Registry.getPool<Component::MeshRenderer>();
        auto& transformPool = m_SceneManager->m_Registry.getPool<Component::Transform>();
        auto& materialPool = m_SceneManager->m_Registry.getPool<Component::Material>();
        const auto& entitiesWithMesh = meshPool.getEntities();
        const auto& meshData = meshPool.data();

        auto& mutableObjectUBO = const_cast<RHIConstantBuffer&>(m_FrameConstants.modelUBO);

        for (size_t i = 0; i < entitiesWithMesh.size(); ++i) {
            Entity e = entitiesWithMesh[i];

            if (transformPool.has(e)) [[likely]] {
                const auto& meshComp = meshData[i];
                const auto& materialComp = materialPool.get(e);
                const auto& transform = transformPool.get(e);
                
                auto material = m_ResourceManager->getMaterial(materialComp.materialName);
                u32 vao = m_ResourceManager->getMeshVaoByID(meshComp.meshID);
                u32 indexCount = m_ResourceManager->getMeshIndexSizeByID(meshComp.meshID);

                if (material && vao != 0) [[likely]] {
                    Generated::ObjectBuffer objData;
                    objData.model = transform.modelMatrix();
                    mutableObjectUBO.setData(&objData, sizeof(Generated::ObjectBuffer));
                    mutableObjectUBO.bind(1);

                    const auto& submeshes = m_ResourceManager->getMeshByID(meshComp.meshID).submeshes;
                    if (submeshes.size() < 2) {
                        // One material for the whole mesh: the component's material wins, which
                        // keeps hand-placed objects and primitives editable from the editor.
                        material->bind(m_RenderContext);
                        m_RenderContext->drawIndexed(vao, indexCount);
                    }
                    else {
                        // An imported model with several materials, one draw call per run. The
                        // component's material stays the fallback for runs whose material is
                        // missing, so a broken import still shows geometry instead of nothing.
                        for (const auto& submesh : submeshes) {
                            const auto submeshMaterial = m_ResourceManager->getMaterial(submesh.materialName);
                            (submeshMaterial ? submeshMaterial : material)->bind(m_RenderContext);
                            m_RenderContext->drawIndexed(vao, submesh.indexCount, submesh.indexOffset);
                        }
                    }
                }
                else {
                    A_WARN("material or vao missing for entity {}", e);
                }
            }
        }
        RenderPassSpecs resetSpecs;
        m_RenderContext->setRenderPassSpecs(resetSpecs);
    }

    void Renderer::proceduralPass() const {
        RenderPassSpecs specs;
        specs.cullMode = CullMode::None;
        specs.depthTest = true;
        specs.depthFunction = DepthFunc::LEqual;
        specs.blendMode = BlendMode::AlphaBlend;
        m_RenderContext->setRenderPassSpecs(specs);

        if (auto skyboxMat = m_ResourceManager->getMaterial("SkyboxMaterial")) [[likely]] {
            skyboxMat->bind(m_RenderContext);
            m_RenderContext->bindTextureCube(0, m_EnvironmentCubemap);

            m_RenderContext->drawIndexed(cubemapGpuHandle.vao, 36);
        }
        RenderPassSpecs resetSpecs;
        m_RenderContext->setRenderPassSpecs(resetSpecs);
    }

    void Renderer::selectionPass(Entity selectedEntity) const {
        m_RenderContext->bindFramebuffer(m_SelectionBuffer.get());
        m_RenderContext->clear(ClearFlags::Color | ClearFlags::Depth, vec4(0.0f, 0.0f, 0.0f, 0.0f));

        if (selectedEntity == ECS::INVALID_ENTITY_ID) {
            m_RenderContext->unbindFramebuffer();
            return;
        }

        EntityHandle handle = { selectedEntity, &m_SceneManager->m_Registry };
        if (handle.has<Component::MeshRenderer>() && handle.has<Component::Transform>()) {

            const auto& meshComp = handle.get<Component::MeshRenderer>();
            const auto& transform = handle.get<Component::Transform>();

            auto material = m_ResourceManager->getMaterial("SelectionMaterial");
            u32 vao = m_ResourceManager->getMeshVaoByID(meshComp.meshID);
            u32 indexCount = m_ResourceManager->getMeshIndexSizeByID(meshComp.meshID);

            if (material && vao != 0) {
                RenderPassSpecs specs;
                specs.rasterizerMode = RasterizerMode::Fill;
                specs.cullMode = CullMode::None;
                specs.depthTest = true;
                specs.depthFunction = DepthFunc::LEqual;
                m_RenderContext->setRenderPassSpecs(specs);

                Generated::ObjectBuffer objData;
                objData.model = transform.modelMatrix();
                auto& mutableObjectUBO = const_cast<RHIConstantBuffer&>(m_FrameConstants.modelUBO);
                mutableObjectUBO.setData(&objData, sizeof(Generated::ObjectBuffer));
                mutableObjectUBO.bind(1);

                material->bind(m_RenderContext);
                m_RenderContext->drawIndexed(vao, indexCount);

                RenderPassSpecs resetSpecs;
                m_RenderContext->setRenderPassSpecs(resetSpecs);
            }
        }
        m_RenderContext->unbindFramebuffer();
    }

    void Renderer::postprocessingPass() const {
        m_RenderContext->bindFramebuffer(m_PostprocessBuffer.get());

        RenderPassSpecs specs;
        specs.depthTest = false;
        specs.blendMode = BlendMode::AlphaBlend;
        specs.cullMode = CullMode::None;
        m_RenderContext->setRenderPassSpecs(specs);

        if (auto outlineMat = m_ResourceManager->getMaterial("OutlineMaterial")) [[likely]] {
            Generated::OutlineParamsBuffer outlineData;
            outlineData.texelSize = m_TexelSize;

            auto& mutableOutlineUBO = const_cast<RHIConstantBuffer&>(m_OutlineUBO);
            mutableOutlineUBO.setData(&outlineData, sizeof(Generated::OutlineParamsBuffer));
            mutableOutlineUBO.bind(0);

            outlineMat->bind(m_RenderContext);
            m_RenderContext->bindShaderTexture(10, m_SceneBuffer->getColorAttachmentTexture(0));
            m_RenderContext->bindShaderTexture(11, m_SelectionBuffer->getColorAttachmentTexture(0));
            m_RenderContext->drawArrays(m_CubeVao, 3);
        }

        RenderPassSpecs resetSpecs;
        m_RenderContext->setRenderPassSpecs(resetSpecs);
        m_RenderContext->unbindFramebuffer();
    }

    void Renderer::scenePassEndResolve() const {
        m_RenderContext->blitFramebuffer(m_MsaaBuffer.get(), m_SceneBuffer.get(), false);
        m_RenderContext->unbindFramebuffer();
    }

    void Renderer::windowClearPass()
    {
        m_RenderContext->setViewport(0, 0, Window::g_WindowWidth, Window::g_WindowHeight);
        m_RenderContext->clear(ClearFlags::Color | ClearFlags::Depth, vec4(0.10f, 0.10f, 0.10f, 1.0f));
    }

    void Renderer::createCubemapTexture(CubemapData& data) {
        if (m_Api == AndromedaGXAPI::OpenGL) {
            CubemapGL::CubemapTextureUploadGL(data);
        }
    }

    Texture Renderer::getFinalSceneViewportTexture() const
    {
        return m_PostprocessBuffer->getColorAttachmentTexture(0);
    }

    void Renderer::onViewportResize(ivec2 newSize)
    {
        if (newSize.x <= 0 || newSize.y <= 0) return;
        if (newSize == m_FramebufferSize) return;

        m_TargetSize = newSize;
        m_ResizePending = true;
        m_ResizeTimer = 0.15f;
    }

    void Renderer::processResizeTimer()
    {
        if (!m_ResizePending) return;

        m_ResizeTimer -= SystemManager::s_deltaTime;
        if (m_ResizeTimer <= 0.0f)
        {
            m_FramebufferSize = m_TargetSize;
            m_TexelSize = 1.0f / vec2(static_cast<float>(m_FramebufferSize.x), static_cast<float>(m_FramebufferSize.y));
            m_Cam->framebufferSize = m_FramebufferSize;

            m_MsaaBuffer->resize(m_FramebufferSize);
            m_SceneBuffer->resize(m_FramebufferSize);
            m_SelectionBuffer->resize(m_FramebufferSize);
            m_PostprocessBuffer->resize(m_FramebufferSize);

            m_ResizePending = false;
        }
    }
}