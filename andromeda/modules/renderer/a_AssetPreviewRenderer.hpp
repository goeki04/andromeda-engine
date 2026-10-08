#pragma once
#include <unordered_map>

#include "a_AssetPreviewRenderer.hpp"
#include "a_model_record.hpp"
#include "a_rhi_framebuffer.hpp"
#include "a_IGraphicsContext.hpp"
#include "resource_manager.h"
#include "a_FrameConstants.hpp"


namespace Andromeda
{
    class AssetPreviewRenderer
    {
    public:
        explicit AssetPreviewRenderer(IModelProvider* provider,IGraphicsContext* ctx, ResourceManager* rm, FrameConstants* frameConstant)
        {
            m_FrameConstants = frameConstant;
            m_RenderContext = ctx;
            m_ResourceManager = rm;
            m_Provider = provider;
        }
        void renderFramebufferPreview(const ModelRecord& record);
        void cachePreviewTextures();
        void initialize();
        void drawPreviewMesh(const ModelRecord& record, const Mesh& mesh, const mat4& modelData);
        void readTextureAtlas();
        void writeTextureAtlas();
    private:
        IModelProvider* m_Provider;
        IGraphicsContext* m_RenderContext;
        std::unique_ptr<RHIFramebuffer> m_PreviewFbo;
        std::unique_ptr<RHIFramebuffer> m_PreviewResolveFbo;
        std::unique_ptr<RHIFramebuffer> m_PreviewPostFbo;
        std::unordered_map<u32, Texture> m_PreviewTextures;
        u32 m_FullscreenVao = 0;
        Texture m_TextureAtlas;
        ResourceManager* m_ResourceManager;
        FrameConstants* m_FrameConstants;
        void createFbo(IGraphicsContext* ctx);
    };
}
