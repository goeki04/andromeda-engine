#include "a_AssetPreviewRenderer.hpp"
#include "a_logger.hpp"
#include "a_colors.hpp"
#include "a_geometry.hpp"
#include "a_shader_generated.hpp"

namespace Andromeda
{
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
        constexpr vec3 eye(1.6f, 1.2f, 1.6f);
        const mat4 view = amath::lookAt(eye, vec3(0.0f), vec3(0.0f, 1.0f, 0.0f));
        const mat4 proj = amath::perspective(glm::radians(35.0f), 1.0f, 0.1f, 10.0f);
        RenderPassSpecs specs;
        specs.depthTest = true;
        specs.cullMode = CullMode::Back;
        specs.rasterizerMode = RasterizerMode::Fill;

        Generated::CameraBuffer camData;
        camData.camPos = eye;
        camData.viewMatrix = view;
        camData.projMatrix = proj;
        m_FrameConstants->cameraUBO.setData(&camData, sizeof(camData));
        m_FrameConstants->cameraUBO.bind(0);

        Generated::lights lightData;
        lightData.lightPositions[0] = vec4( 2.0f,  2.0f,  2.0f, 1.0f);
        lightData.lightPositions[1] = vec4(-2.0f,  1.5f,  2.0f, 1.0f);
        lightData.lightPositions[2] = vec4( 2.0f, -1.0f,  2.0f, 1.0f);
        lightData.lightPositions[3] = vec4(-2.0f, -1.0f, -2.0f, 1.0f);

        for (auto& l : lightData.lightColors)
        {
            l = vec4(25.0f,25.0f,25.0f,1.0f);
        }
        m_FrameConstants->lightUBO.setData(&lightData, sizeof(lightData));
        m_FrameConstants->lightUBO.bind(3);
        RenderPassSpecs postSpecs;
        postSpecs.depthTest = false;
        postSpecs.cullMode = CullMode::None;
        m_FullscreenVao = m_RenderContext->createEmptyVAO();

        const ShaderProgramHandle postHandle = m_ResourceManager->loadShaderRHI(
            m_RenderContext, "PreviewPost_Shader", SHADER_PATH "outline.vert", SHADER_PATH "preview_post.frag");
        const auto postMaterial = m_ResourceManager->createMaterial("PreviewPostMaterial", postHandle, m_RenderContext);

        for (u32 i = 0; i < modelCount; ++i)
        {
            const auto& record = m_Provider->getModelData(i);
            Texture tex = m_RenderContext->generateTexture(256,256,samplerState,TextureFormat::RGBA8_UNORM);
            m_RenderContext->allocateTexture(tex);
            m_RenderContext->bindSamplerState(tex.textureID, samplerState);

            m_RenderContext->bindFramebuffer(m_PreviewFbo.get());
            m_RenderContext->setRenderPassSpecs(specs);
            m_RenderContext->setViewport(0, 0, 256, 256);
            m_RenderContext->clear(ClearFlags::All, Colors::Transparent);
            renderFramebufferPreview(record);

            m_RenderContext->blitFramebuffer(m_PreviewFbo.get(), m_PreviewResolveFbo.get(), false);

            m_RenderContext->bindFramebuffer(m_PreviewPostFbo.get());
            m_RenderContext->framebufferTexture2D(tex,0);
            m_RenderContext->setRenderPassSpecs(postSpecs);
            postMaterial->bind(m_RenderContext);
            m_RenderContext->bindShaderTexture(10, m_PreviewResolveFbo->getColorAttachmentTexture(0));
            m_RenderContext->drawArrays(m_FullscreenVao,3);

            m_PreviewTextures[record.meshID] = tex;
            m_ResourceManager->setPreviewTextureID(record.meshID, tex.textureID);
        }
        m_RenderContext->unbindFramebuffer();
        m_RenderContext->setRenderPassSpecs(RenderPassSpecs{});
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

    void AssetPreviewRenderer::createFbo(IGraphicsContext* ctx)
    {
        FramebufferSpecification specs;
        specs.height = 256;
        specs.width = 256;
        specs.samples = 4;
        specs.attachments.push_back({.textureFormat = FramebufferTextureFormat::RGBA8});
        specs.attachments.push_back({.textureFormat = FramebufferTextureFormat::DEPTH24Stencil8});
        m_PreviewFbo = ctx->createFramebuffer(specs);

        specs.samples = 1;
        specs.attachments.resize(1);
        m_PreviewPostFbo = ctx->createFramebuffer(specs);
        m_PreviewResolveFbo = ctx->createFramebuffer(specs);
    }

    void AssetPreviewRenderer::cachePreviewTextures()
    {

    }

    void AssetPreviewRenderer::renderFramebufferPreview(const ModelRecord& record)
    {
        const Mesh& mesh = m_ResourceManager->getMeshByID(record.meshID);
        const ECS::Component::AABB aabb = mesh.getAABB();

        const vec3 extent = aabb.max - aabb.min;
        const float longest = glm::max(extent.x, glm::max(extent.y, extent.z));
        const float scale = longest > 0.0f ? 1.0f / longest : 1.0f;

        mat4 model = glm::scale(mat4(1.0f), vec3(scale));
        model = amath::translate(model, -aabb.center);

        drawPreviewMesh(record, mesh, model);
    }
}
