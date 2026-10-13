#include "a_AssetPreviewRenderer.hpp"

#include <complex>

#include "a_logger.hpp"
#include "a_colors.hpp"
#include "a_geometry.hpp"
#include "a_shader_generated.hpp"
#include "a_filesystem.hpp"
namespace Andromeda
{

    void AssetPreviewRenderer::uploadCameraAndLightData() {
        constexpr vec3 eye(1.6f, 1.2f, 1.6f);
        const mat4 view = amath::lookAt(eye, vec3(0.0f), vec3(0.0f, 1.0f, 0.0f));
        const mat4 proj = amath::perspective(glm::radians(35.0f), 1.0f, 0.1f, 10.0f);
        Generated::CameraBuffer camData;
        camData.camPos = eye;
        camData.viewMatrix = view;
        camData.projMatrix = proj;
        m_FrameConstants->cameraUBO.setData(&camData, sizeof(camData));
        m_FrameConstants->cameraUBO.bind(0);

        Generated::lights lightData;
        lightData.lightPositions[0] = vec4(2.0f, 2.0f, 2.0f, 1.0f);
        lightData.lightPositions[1] = vec4(-2.0f, 1.5f, 2.0f, 1.0f);
        lightData.lightPositions[2] = vec4(2.0f, -1.0f, 2.0f, 1.0f);
        lightData.lightPositions[3] = vec4(-2.0f, -1.0f, -2.0f, 1.0f);
        for (auto& l : lightData.lightColors) {
            l = vec4(25.0f, 25.0f, 25.0f, 1.0f);
        }
        m_FrameConstants->lightUBO.setData(&lightData, sizeof(lightData));
        m_FrameConstants->lightUBO.bind(3);
    }
    ///Call this function after creating the framebuffer for the assetpreviewrenderer
    void AssetPreviewRenderer::initialize()
    {
        if (m_Provider == nullptr)
        {
            A_WARN("provider is nullptr, returning from AssetPreviewRenderer");
            return;
        }

        const u32 modelCount = m_Provider->getModelCount();
        createFbo(m_RenderContext);
        SamplerState samplerState;
        samplerState.minFilter = FilterModeMin::Linear;

        m_RenderPassSpecs.scene.depthTest = true;
        m_RenderPassSpecs.scene.cullMode = CullMode::Back;
        m_RenderPassSpecs.scene.rasterizerMode = RasterizerMode::Fill;

        uploadCameraAndLightData();

        m_RenderPassSpecs.postProcess.depthTest = false;
        m_RenderPassSpecs.postProcess.cullMode = CullMode::None;
        m_FullscreenVao = m_RenderContext->createEmptyVAO();

        const ShaderProgramHandle postHandle = m_ResourceManager->loadShaderRHI(
            m_RenderContext, "PreviewPost_Shader", SHADER_PATH "outline.vert", SHADER_PATH "preview_post.frag");
        m_PostProcessMaterial = m_ResourceManager->createMaterial("PreviewPostMaterial", postHandle, m_RenderContext);

        for (const u32 meshID : m_ResourceManager->getModelRecords() | std::views::keys)
        {
            drawPreviewFramebuffer(meshID, samplerState, m_PostProcessMaterial, m_RenderPassSpecs);
        }
        m_RenderContext->unbindFramebuffer();
        m_RenderContext->setRenderPassSpecs(RenderPassSpecs{});
    }

    void AssetPreviewRenderer::drawPreviewFramebuffer(const u32 meshID, SamplerState& outputTex,const std::shared_ptr<Material>& postMaterial,const PreviewPassSpecs& specs)
    {
        // By mesh id, not by position: IDs and list indices happen to coincide today, but a
        // mesh removed at runtime would make them drift apart without a compiler error.
        const auto& records = m_ResourceManager->getModelRecords();
        const auto it = records.find(meshID);
        if (it == records.end())
        {
            A_WARN("No model record for mesh id {}, skipping preview", meshID);
            return;
        }
        const ModelRecord& record = it->second;
        Texture tex = m_RenderContext->generateTexture(256,256,outputTex,TextureFormat::RGBA8_UNORM);
        m_RenderContext->allocateTexture(tex);
        m_RenderContext->bindSamplerState(tex.textureID, outputTex);

        m_RenderContext->bindFramebuffer(m_PreviewFbo.get());
        m_RenderContext->setRenderPassSpecs(specs.scene);
        m_RenderContext->setViewport(0, 0, 256, 256);
        m_RenderContext->clear(ClearFlags::All, Colors::Transparent);
        uploadCameraAndLightData();
        renderFramebufferPreview(record);

        m_RenderContext->blitFramebuffer(m_PreviewFbo.get(), m_PreviewResolveFbo.get(), false);

        m_RenderContext->bindFramebuffer(m_PreviewPostFbo.get());
        m_RenderContext->framebufferTexture2D(tex,0);
        m_RenderContext->setRenderPassSpecs(specs.postProcess);
        postMaterial->bind(m_RenderContext);
        m_RenderContext->bindShaderTexture(10, m_PreviewResolveFbo->getColorAttachmentTexture(0));
        m_RenderContext->drawArrays(m_FullscreenVao,3);

        // A refresh re-renders a thumbnail that already exists, so the entry being replaced
        // owns a GPU texture nobody else will free. Publish the new one before releasing the
        // old, so the asset browser never reads an id that has just been deleted.
        const auto previous = m_PreviewTextures.find(record.meshID);
        const bool hadPrevious = previous != m_PreviewTextures.end();
        Texture outdated = hadPrevious ? previous->second : Texture{};

        m_PreviewTextures[record.meshID] = tex;
        m_ResourceManager->setPreviewTextureID(record.meshID, tex.textureID);

        if (hadPrevious) {
            // Safe to delete right away rather than at the end of the frame, because the
            // OS_Manager that dispatches FileChanged is registered before the editor in
            // main.cpp: no ImGui draw list built this frame can reference the old id yet.
            // Moving that registration later would break the assumption silently.
            m_RenderContext->destroyTexture(outdated);
        }
    }

    void AssetPreviewRenderer::drawPreviewMesh(const ModelRecord& record,const Mesh& mesh,const mat4& modelData)
    {
        Generated::ObjectBuffer objectData;
        objectData.model = modelData;
        m_FrameConstants->modelUBO.setData(&objectData, sizeof(objectData));
        m_FrameConstants->modelUBO.bind(1);

        const std::string materialName = "PBRMaterial";

        const u32 vao = m_ResourceManager->getMeshVaoByID(record.meshID);
        const u32 indexCount = m_ResourceManager->getMeshIndexSizeByID(record.meshID);
        const auto material = m_ResourceManager->getMaterial(materialName);

        if (vao == 0 || !material)
        {
            A_WARN("No Vao with ID {} or Material with name '{}' found", record.meshID, materialName);
            return;
        }
        if (mesh.submeshes.size() < 2)
        {
            material->bind(m_RenderContext);
            m_RenderContext->drawIndexed(vao,indexCount);
        }
        else
        {
            for (auto& m : mesh.submeshes)
            {
                const auto& submeshMaterial = m_ResourceManager->getMaterial(m.materialName);
                (submeshMaterial ? submeshMaterial : material)->bind(m_RenderContext);
                m_RenderContext->drawIndexed(vao,m.indexCount, m.indexOffset);
            }
        }
    }

    void AssetPreviewRenderer::readTextureAtlas()
    {

    }

    void AssetPreviewRenderer::writeTextureAtlas()
    {

    }

    void AssetPreviewRenderer::refresh(const MeshReloaded& event)
    {
        // Everything this used to check - directory, action, extension, name lookup - was
        // really asking "did the ResourceManager end up with a usable mesh?". It answers that
        // itself now by sending this event only after a successful import.
        if (m_PostProcessMaterial == nullptr)
        {
            A_WARN("Skipped asset preview creation for this asset. Invalid Material choice");
            return;
        }

        SamplerState samplerState;
        samplerState.minFilter = FilterModeMin::Linear;

        drawPreviewFramebuffer(event.meshID, samplerState, m_PostProcessMaterial, m_RenderPassSpecs);
        m_RenderContext->unbindFramebuffer();
        m_RenderContext->setRenderPassSpecs(RenderPassSpecs{});
    }

    void AssetPreviewRenderer::destroyRHIResources()
    {
        for (auto& [meshID, texture] : m_PreviewTextures)
        {
            m_RenderContext->destroyTexture(texture);
            // The resource manager hands these ids to the asset browser, so clear them here
            // too - a stale id outlives the texture and OpenGL reuses deleted names.
            m_ResourceManager->setPreviewTextureID(meshID, 0);
        }
        m_PreviewTextures.clear();
    }

    void AssetPreviewRenderer::createFbo(IGraphicsContext* ctx)
    {
        FramebufferSpecification specs;
        specs.height = 256;
        specs.width = 256;
        specs.samples = 4;
        // RGBA16F for the scene and its resolve, for the same reason as the renderer's own buffers:
        // 8 bits would clamp the PBR shader's linear radiance before preview_post.frag tonemaps it,
        // and the thumbnail would stop matching the viewport.
        specs.attachments.push_back({.textureFormat = FramebufferTextureFormat::RGBA16F});
        specs.attachments.push_back({.textureFormat = FramebufferTextureFormat::DEPTH24Stencil8});
        m_PreviewFbo = ctx->createFramebuffer(specs);

        specs.samples = 1;
        specs.attachments.resize(1);
        m_PreviewResolveFbo = ctx->createFramebuffer(specs);

        // The post pass writes display-ready colour, so its target is 8 bit - this is also the
        // texture the browser panel samples.
        specs.attachments[0].textureFormat = FramebufferTextureFormat::RGBA8;
        m_PreviewPostFbo = ctx->createFramebuffer(specs);
    }

    void AssetPreviewRenderer::cachePreviewTextures()
    {

    }

    void AssetPreviewRenderer::renderFramebufferPreview(const ModelRecord& record)
    {
        const Mesh& mesh = m_ResourceManager->getMeshByID(record.meshID);
        const ECS::Component::AABB aabb = mesh.getAABB();

        const vec3 extent = aabb.max - aabb.min;
        const float longest = amath::max(extent.x, amath::max(extent.y, extent.z));
        const float scale = longest > 0.0f ? 1.0f / longest : 1.0f;

        mat4 model = amath::scale(mat4(1.0f), vec3(scale));
        model = amath::translate(model, -aabb.center);

        drawPreviewMesh(record, mesh, model);
    }
}
