#pragma once

/**
 * @file a_rhi_framebuffer.hpp
 * @brief Backend-agnostic framebuffer (render target) wrapper, implemented via the Pimpl idiom,
 *        and its specification types.
 */

#include "a_primitives.hpp"
#include "a_texture.hpp"
#include <vector>
#include <memory>
namespace Andromeda {
	/**
	 * @enum FramebufferTextureFormat
	 * @brief Formats available for framebuffer color and depth/stencil attachments.
	 */
	enum class FramebufferTextureFormat {
		None = 0,        ///< Unspecified / invalid.
		RGBA8,           ///< 8-bit per channel color attachment.
		RGBA16F,         ///< 16-bit float per channel color attachment (HDR).

		DEPTH24Stencil8, ///< Combined 24-bit depth + 8-bit stencil attachment.
		DEPTH32F         ///< 32-bit float depth attachment.
	};

	/**
	 * @struct FramebufferTextureSpecification
	 * @brief Describes a single attachment requested for a framebuffer.
	 */
	struct FramebufferTextureSpecification {
		FramebufferTextureFormat textureFormat = FramebufferTextureFormat::None; ///< Format of this attachment.
		bool generateMipmaps = false; ///< Whether to allocate/generate a mip chain for this attachment.
	};

	/**
	 * @struct FramebufferSpecification
	 * @brief Full description of a framebuffer to create: size, MSAA samples and its attachments.
	 */
	struct FramebufferSpecification {
		u32 width = 0;    ///< Width in pixels.
		u32 height = 0;   ///< Height in pixels.
		u32 samples = 1;  ///< MSAA sample count (1 = no multisampling).
		std::vector<FramebufferTextureSpecification> attachments; ///< Ordered list of color/depth attachments to create.
	};

	/**
	 * @class FramebufferImpl
	 * @brief Opaque, backend-specific state of a framebuffer.
	 * @details Only forward-declared here; defined once, per active backend (e.g.
	 *          @c OpenGL/a_opengl_framebuffer.cpp), selected at CMake configure time via
	 *          @c ANDROMEDA_GRAPHICS_API. Same rationale as @c ConstantBufferImpl: exactly one
	 *          implementation is ever compiled in, so @c RHIFramebuffer needs neither @c virtual
	 *          nor to expose backend headers to its consumers.
	 */
	class FramebufferImpl;

	/**
	 * @class RHIFramebuffer
	 * @brief Render target wrapping one or more attachments.
	 *
	 * @details Move-only RAII wrapper. All methods forward to @c FramebufferImpl, which is
	 *          entirely defined inside the selected backend's .cpp file (compilation firewall).
	 */
	class RHIFramebuffer {
	public:
		/**
		 * @brief Creates a framebuffer from the given specification.
		 * @param specs Size, sample count and attachment formats to build.
		 */
		explicit RHIFramebuffer(const FramebufferSpecification& specs);

		/** @brief Destroys the framebuffer and its attachments. */
		~RHIFramebuffer();

		RHIFramebuffer(const RHIFramebuffer&) = delete;
		RHIFramebuffer& operator=(const RHIFramebuffer&) = delete;
		RHIFramebuffer(RHIFramebuffer&& other) noexcept;
		RHIFramebuffer& operator=(RHIFramebuffer&& other) noexcept;

		/**
		 * @brief Recreates the framebuffer attachments at a new size.
		 * @param newSize The new dimensions in pixels.
		 */
		void resize(const ivec2& newSize);

		/** @brief Returns the specification this framebuffer was created from. */
		const FramebufferSpecification& getSpecification() const;

		/**
		 * @brief Returns the texture backing a given color attachment.
		 * @param index Zero-based color attachment index.
		 * @return The color attachment texture.
		 */
		const Texture& getColorAttachmentTexture(i32 index = 0) const;

		/**
		 * @brief Returns the backend's raw framebuffer handle (e.g. the OpenGL FBO id).
		 * @details Used internally by the active @c IGraphicsContext implementation
		 *          (e.g. to bind/blit); most callers should not need this.
		 */
		u32 getFramebufferID() const;

	private:
		std::unique_ptr<FramebufferImpl> m_Impl; ///< Backend-specific implementation (Pimpl).
	};
}
