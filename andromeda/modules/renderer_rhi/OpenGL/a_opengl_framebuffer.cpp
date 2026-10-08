/**
 * @file a_opengl_framebuffer.cpp
 * @brief OpenGL implementation of RHIFramebuffer (FBO), and its color/depth/stencil attachments.
 * @details Compiled in only when ANDROMEDA_GRAPHICS_API=OpenGL (see modules/renderer_rhi/CMakeLists.txt).
 *          Defines both @c FramebufferImpl and the @c RHIFramebuffer methods that forward to it,
 *          so nothing outside this file ever needs to know OpenGL is involved.
 */

#include "a_rhi_framebuffer.hpp"
#include <GL/glew.h>
#include <vector>
#include <stdexcept>
#include <cassert>
#include "a_texture.hpp"
#include "a_samplerState.hpp"

namespace Andromeda {

	/**
	 * @class FramebufferImpl
	 * @brief OpenGL framebuffer object (FBO) with its color and depth/stencil attachments.
	 */
	class FramebufferImpl {
	public:
		/**
		 * @brief Creates an OpenGL framebuffer from the given specification.
		 * @param specs Size, sample count and attachment formats to build.
		 */
		explicit FramebufferImpl(const FramebufferSpecification& specs)
			: m_Specs(specs)
		{
			invalidate();
		}

		/** @brief Destroys the framebuffer and its attachments. */
		~FramebufferImpl() {
			destroy();
		}

		/** @brief Deletes the underlying OpenGL framebuffer and attachment objects. */
		void destroy() {
			if (m_RendererID) {
				glDeleteFramebuffers(1, &m_RendererID);

				if (!m_ColorAttachments.empty()) {
					std::vector<u32> textureIDs;
					textureIDs.reserve(m_ColorAttachments.size());

					for (const auto& tex : m_ColorAttachments) {
						textureIDs.push_back(tex.textureID);
					}
					glDeleteTextures(static_cast<GLsizei>(textureIDs.size()), textureIDs.data());
				}

				if (m_DepthAttachment) {
					glDeleteTextures(1, &m_DepthAttachment);
					glDeleteRenderbuffers(1, &m_DepthAttachment);
				}

				m_ColorAttachments.clear();
				m_DepthAttachment = 0;
				m_RendererID = 0;
			}
		}

		/** @copydoc RHIFramebuffer::getColorAttachmentTexture */
		const Texture& getColorAttachmentTexture(i32 index) const
		{
			assert(index < m_ColorAttachments.size() && "Framebuffer attachment index out of bounds!");
			return m_ColorAttachments[index];
		}

		/** @brief (Re)creates the FBO and all attachments from the current specification. */
		void invalidate() {
			if (m_RendererID) {
				destroy();
			}

			glGenFramebuffers(1, &m_RendererID);
			glBindFramebuffer(GL_FRAMEBUFFER, m_RendererID);

			bool isMultisampled = m_Specs.samples > 1;
			GLenum textureTarget = isMultisampled ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D;

			std::vector<FramebufferTextureSpecification> colorSpecs;
			FramebufferTextureSpecification depthSpec;
			bool hasDepth = false;

			for (const auto& spec : m_Specs.attachments) {
				if (spec.textureFormat == FramebufferTextureFormat::DEPTH24Stencil8 || spec.textureFormat == FramebufferTextureFormat::DEPTH32F) {
					depthSpec = spec;
					hasDepth = true;
				}
				else {
					colorSpecs.push_back(spec);
				}
			}

			attachColorAttachments(textureTarget, colorSpecs);
			attachDepthAttachment(isMultisampled, depthSpec, hasDepth);
			configureDrawBuffers();

			if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
				throw std::runtime_error("Andromeda RHI: Framebuffer is incomplete!");
			}

			glBindTexture(textureTarget, 0);
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
		}

		/** @copydoc RHIFramebuffer::resize */
		void resize(const ivec2& newSize) {
			if (newSize.x <= 0 || newSize.y <= 0) return;
			m_Specs.width = newSize.x;
			m_Specs.height = newSize.y;
			invalidate();
		}

		/** @copydoc RHIFramebuffer::getSpecification */
		const FramebufferSpecification& getSpecification() const {
			return m_Specs;
		}

		/** @brief Returns the raw OpenGL framebuffer object ID. */
		u32 getFramebufferID() const { return m_RendererID; }

	private:
		/**
		 * @brief Creates and attaches the color textures described by the specification.
		 * @param textureTarget The GL texture target (e.g. GL_TEXTURE_2D or the multisample target).
		 * @param colorSpecs The color attachment specifications to create.
		 */
		void attachColorAttachments(const GLenum textureTarget, const std::vector<FramebufferTextureSpecification>& colorSpecs)
		{
			if (colorSpecs.empty()) return;

			bool isMultisampled = m_Specs.samples > 1;
			m_ColorAttachments.resize(colorSpecs.size());

			for (size_t i = 0; i < colorSpecs.size(); ++i) {
				SamplerState sampler;
				sampler.type = isMultisampled ? TextureType::Cubemap : TextureType::Texture2D;
				sampler.minFilter = (colorSpecs[i].textureFormat == FramebufferTextureFormat::None) ? FilterModeMin::Nearest : FilterModeMin::Linear;
				sampler.magFilter = (colorSpecs[i].textureFormat == FramebufferTextureFormat::None) ? FilterModeMag::Nearest : FilterModeMag::Linear;
				sampler.wrapS = WrapMode::ClampToEdge;
				sampler.wrapT = WrapMode::ClampToEdge;

				m_ColorAttachments[i].width = m_Specs.width;
				m_ColorAttachments[i].height = m_Specs.height;
				m_ColorAttachments[i].sampler = sampler;

				glGenTextures(1, &m_ColorAttachments[i].textureID);
				glBindTexture(textureTarget, m_ColorAttachments[i].textureID);

				GLenum internalFormat = GL_RGBA8;
				GLenum format = GL_RGBA;
				GLenum dataType = GL_UNSIGNED_BYTE;

				if (colorSpecs[i].textureFormat == FramebufferTextureFormat::RGBA16F) {
					internalFormat = GL_RGBA16F;
					dataType = GL_FLOAT;
				}
				else if (colorSpecs[i].textureFormat == FramebufferTextureFormat::None) {
					internalFormat = GL_R8;
					format = GL_RED;
				}

				if (isMultisampled) {
					glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, m_Specs.samples, internalFormat, m_Specs.width, m_Specs.height, GL_TRUE);
				}
				else {
					glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, m_Specs.width, m_Specs.height, 0, format, dataType, nullptr);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, sampler.minFilter == FilterModeMin::Nearest ? GL_NEAREST : GL_LINEAR);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, sampler.magFilter == FilterModeMag::Nearest ? GL_NEAREST : GL_LINEAR);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
				}

				glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + static_cast<GLenum>(i), textureTarget, m_ColorAttachments[i].textureID, 0);
			}
		}

		/**
		 * @brief Creates and attaches the depth (or depth/stencil) attachment if requested.
		 * @param isMultisampled Whether to create a multisampled depth attachment.
		 * @param depthSpec The depth attachment specification.
		 * @param hasDepth Whether a depth attachment is required at all.
		 */
		void attachDepthAttachment(const bool isMultisampled, const FramebufferTextureSpecification& depthSpec, bool hasDepth)
		{
			if (!hasDepth) return;

			if (isMultisampled) {
				glGenRenderbuffers(1, &m_DepthAttachment);
				glBindRenderbuffer(GL_RENDERBUFFER, m_DepthAttachment);
				glRenderbufferStorageMultisample(GL_RENDERBUFFER, static_cast<GLsizei>(m_Specs.samples), GL_DEPTH24_STENCIL8, m_Specs.width, m_Specs.height);
				glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_DepthAttachment);
			}
			else {
				glGenTextures(1, &m_DepthAttachment);
				glBindTexture(GL_TEXTURE_2D, m_DepthAttachment);

				if (depthSpec.textureFormat == FramebufferTextureFormat::DEPTH32F) {
					glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, m_Specs.width, m_Specs.height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
					glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_DepthAttachment, 0);
				}
				else {
					glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, m_Specs.width, m_Specs.height, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, nullptr);
					glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, m_DepthAttachment, 0);
				}
			}
		}

		/** @brief Configures the GL draw-buffer list to match the color attachments. */
		void configureDrawBuffers()
		{
			if (m_ColorAttachments.size() > 1) {
				GLenum buffers[4] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3 };
				glDrawBuffers(static_cast<GLsizei>(m_ColorAttachments.size()), buffers);
			}
			else if (m_ColorAttachments.empty()) {
				glDrawBuffer(GL_NONE);
				glReadBuffer(GL_NONE);
			}
		}

		u32 m_RendererID = 0;                    ///< OpenGL framebuffer object ID.
		FramebufferSpecification m_Specs;        ///< Specification this framebuffer was created from.
		std::vector<Texture> m_ColorAttachments; ///< The created color attachment textures.
		u32 m_DepthAttachment = 0;               ///< OpenGL ID of the depth/stencil attachment (0 if none).
	};

	RHIFramebuffer::RHIFramebuffer(const FramebufferSpecification& specs)
		: m_Impl(std::make_unique<FramebufferImpl>(specs)) {
	}

	RHIFramebuffer::~RHIFramebuffer() = default;
	RHIFramebuffer::RHIFramebuffer(RHIFramebuffer&& other) noexcept = default;
	RHIFramebuffer& RHIFramebuffer::operator=(RHIFramebuffer&& other) noexcept = default;

	void RHIFramebuffer::resize(const ivec2& newSize) { m_Impl->resize(newSize); }
	const FramebufferSpecification& RHIFramebuffer::getSpecification() const { return m_Impl->getSpecification(); }
	const Texture& RHIFramebuffer::getColorAttachmentTexture(const i32 index) const { return m_Impl->getColorAttachmentTexture(index); }
	u32 RHIFramebuffer::getFramebufferID() const { return m_Impl->getFramebufferID(); }
}
