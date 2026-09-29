#pragma once

/**
 * @file a_IGraphicsContext.hpp
 * @brief The render hardware interface (RHI): the abstract graphics backend the engine renders through.
 */

#include <string>
#include "a_primitives.hpp"
#include <span>
#include <vector>
#include "a_rhi_types.hpp"
#include <memory>
#include "a_rhi_framebuffer.hpp"
#include "a_clearFlags.hpp"
#include "a_rhi_constant_buffer.hpp"
#include "a_samplerState.hpp"
#include "a_texture.hpp"
namespace Andromeda {
	struct CubemapData;

	/**
	 * @class IGraphicsContext
	 * @brief Abstract interface over a graphics backend (shaders, compute, buffers, textures, framebuffers, draw calls).
	 *
	 * @details All renderer code issues GPU commands through this interface rather than calling a
	 *          specific API directly, so that backends (currently @c OpenGLContext) can be swapped
	 *          without touching the renderer. Methods come in groups: shader/compute program
	 *          management, drawing and pipeline state, framebuffers, textures/samplers and
	 *          uniform/parameter upload.
	 */
	class IGraphicsContext {
	public:
		virtual ~IGraphicsContext() = default;
        u32 m_FrameCount = 0;
		/**
		 * @brief Compiles and links a shader program from vertex and fragment sources.
		 * @param vertSrc Path to (or source of) the vertex shader.
		 * @param fragSrc Path to (or source of) the fragment shader.
		 * @return A handle to the created program.
		 */
		virtual ShaderProgramHandle createShaderProgram(const std::string& vertSrc, const std::string& fragSrc) = 0;

		/**
		 * @brief Compiles and links a compute shader into a standalone program.
		 * @param computeSrc Path to (or source of) the compute shader.
		 * @return A handle to the created compute program.
		 */
		virtual ShaderProgramHandle createComputeProgram(const std::string& computeSrc) = 0;
		virtual ShaderProgramHandle createComputeProgram(const std::string& computeSrc, const char* entryPoint, const char* stageName) = 0;

		/** @brief Destroys a shader program and frees its GPU resources. */
		virtual void destroyShaderProgram(ShaderProgramHandle handle) = 0;

		/** @brief Makes the given shader program active for subsequent draw/dispatch calls. */
		virtual void bindShaderProgram(ShaderProgramHandle handle) = 0;

		/**
		 * @brief Dispatches a compute shader over a 3D grid of work groups.
		 * @param handle The compute program to run.
		 * @param groupCountX Number of work groups in X.
		 * @param groupCountY Number of work groups in Y.
		 * @param groupCountZ Number of work groups in Z.
		 * @note A memory barrier is required between writing a buffer here and reading it in a later pass.
		 */
		virtual void dispatchCompute(ShaderProgramHandle handle, u32 groupCountX, u32 groupCountY, u32 groupCountZ) = 0;

		/** @brief Applies pipeline state (culling, depth test, blend, raster mode) for the next draws. */
		virtual void setRenderPassSpecs(const RenderPassSpecs& specs) = 0;

		/**
		 * @brief Issues an indexed draw call.
		 * @param vaoID The vertex array to draw from.
		 * @param indexCount Number of indices to render.
		 */
		/** @param indexOffset First index to draw, for rendering one Submesh out of a shared index buffer. */
		virtual void drawIndexed(u32 vaoID, u32 indexCount, u32 indexOffset = 0) = 0;

		/**
		 * @brief Issues a non-indexed draw call.
		 * @param vao The vertex array to draw from.
		 * @param vertexCount Number of vertices to render.
		 */
		virtual void drawArrays(u32 vao, u32 vertexCount) = 0;

		/** @brief Binds a set of textures to their target slots for the active shader. */
		virtual void bindTextures(std::span<const TextureBinding> texture) = 0;
		/**
		 * @brief Reflects the active uniforms of a shader program.
		 * @param handle The program to inspect.
		 * @return Metadata for each discovered uniform.
		 */
		virtual std::vector<ReflectedUniform> getProgramUniforms(ShaderProgramHandle handle) = 0;

		/** @brief Uploads a batch of uniform values to the active program in one call. */
		virtual void submitUniforms(std::span<const UniformData> uniforms) = 0;

		/** @brief Binds a framebuffer as the active render target. */
		virtual void bindFramebuffer(std::shared_ptr<RHIFramebuffer> framebuffer) = 0;

		/** @brief Unbinds the current framebuffer, restoring the default (window) target. */
		virtual void unbindFramebuffer() = 0;

		/**
		 * @brief Copies the contents of one framebuffer into another.
		 * @param source Source framebuffer.
		 * @param target Destination framebuffer (nullptr targets the default framebuffer).
		 * @param copyDepth Whether to also blit the depth buffer.
		 */
		virtual void blitFramebuffer(std::shared_ptr<RHIFramebuffer> source, std::shared_ptr<RHIFramebuffer> target, bool copyDepth = false) = 0;

		/** @brief Creates a framebuffer matching the given specification. */
		virtual std::shared_ptr<RHIFramebuffer> createFramebuffer(const FramebufferSpecification& specs) = 0;

		/** @brief Creates an empty vertex array object (e.g. for attribute-less / SSBO-driven draws). */
		virtual u32 createEmptyVAO() = 0;

		/** @brief Clears the selected buffers to a specific color. */
		virtual void clear(ClearFlags flags, const vec4& color) = 0;

		/** @brief Clears the selected buffers using the current clear state. */
		virtual void clear(ClearFlags flags) = 0;

		/**
		 * @brief Sets the active viewport rectangle.
		 * @param vpPosX X offset in pixels.
		 * @param vpPosY Y offset in pixels.
		 * @param vpWidth Width in pixels.
		 * @param vpHeight Height in pixels.
		 */
		virtual void setViewport(i32 vpPosX, i32 vpPosY, u32 vpWidth, u32 vpHeight) = 0;

		/** @brief Deletes a vertex array object. */
		virtual void deleteVertexArrays(u32 vao) = 0;

		/** @brief Looks up the location of a named uniform within a shader program. */
		virtual i32 getUniformLocation(ShaderProgramHandle shader, const std::string& name) = 0;

		/**
		 * @brief Creates a constant/uniform buffer of the given size.
		 * @param size Buffer size in bytes.
		 * @return A shared pointer to the created buffer.
		 */
		virtual std::shared_ptr<RHIConstantBuffer> createConstantBuffer(u32 size) = 0;

		/** @brief Binds a 2D texture to a shader sampler slot. */
		virtual void bindShaderTexture(u32 slot, const Texture& tex) = 0;

		/** @brief Binds a texture as the active operation target (e.g. for parameter changes). */
		virtual void bindToTarget(const Texture& tex) = 0;

		/** @brief Unbinds the currently bound texture. */
		virtual void unbindTexture() = 0;

		/** @brief Attaches a single cubemap face to the bound framebuffer (used during cubemap baking). */
		virtual void attachCubemapFace(u32 faceIndex, u32 cubemapTexID) = 0;

		/** @brief Binds a cubemap texture to a shader sampler slot. */
		virtual void bindTextureCube(u32 slot,const CubemapData& tex) = 0;

		/** @brief Attaches a specific cubemap face + mip level to the bound framebuffer. */
		virtual void framebufferTexture2D(u32 faceIndex,const CubemapData& tex, u32 mip) = 0;

		/** @brief Attaches a 2D texture mip level to the bound framebuffer. */
		virtual void framebufferTexture2D(const Texture& tex, u32 mip) = 0;

		/** @brief Applies a sampler state (filtering/wrapping) to a texture. */
		virtual void bindSamplerState(u32 textureID, const SamplerState& state) = 0;

		/** @brief Initializes the render context and global backend state. */
		virtual void initRenderContext() = 0;

		/** @brief Allocates GPU memory for a pre-configured texture (must run on the render thread). */
		virtual void allocateTexture(Texture& texture) = 0;

		/**
		 * @brief Creates and allocates a new texture of the given size, sampler and format.
		 * @param width Texture width in pixels.
		 * @param height Texture height in pixels.
		 * @param sampler Sampling configuration.
		 * @param format Storage format (defaults to RGBA8_UNORM).
		 * @return The created texture.
		 */
		virtual Texture generateTexture(u32 width, u32 height, SamplerState& sampler, TextureFormat format = TextureFormat::RGBA8_UNORM) = 0;

		/** @brief Generates the mipmap chain for the currently bound texture of the given type. */
		virtual void generateMipmap(TextureType type) = 0;
		/// <summary>
		/// This should be optimized in future and is DEFINITELY not the way to go. Instead of storing uniform locations we should use SPIRV-Reflect.
		/// </summary>

		/** @brief Sets a mat4 uniform by name on the given shader program. */
		virtual void setParameter(ShaderProgramHandle shader, const std::string& name, const mat4& matrix) = 0;

		/** @brief Sets a vec3 uniform by name on the given shader program. */
		virtual void setParameter(ShaderProgramHandle shader, const std::string& name, const vec3& vector) = 0;

		/** @brief Sets an int uniform by name on the given shader program. */
		virtual void setParameter(ShaderProgramHandle shader, const std::string& name, i32 value) = 0;

		/** @brief Draws multiple instances of a mesh using instancing. */
		virtual void drawInstanced(DrawMode mode, u32 vertexCount, u32 instanceCount, u32 firstVertex) = 0;
	};
}