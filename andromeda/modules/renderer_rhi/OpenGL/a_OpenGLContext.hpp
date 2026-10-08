#pragma once
#include "a_IGraphicsContext.hpp"
#include <string>
#include "a_clearFlags.hpp"
#include "a_rhi_constant_buffer.hpp"
#include "a_texture.hpp"
#include "GL/Glew.h"
namespace Andromeda {
    class SamplerState;
    struct CubemapData;
    /**
     * @class OpenGLContext
     * @brief Handles low-level rendering operations and state management for the OpenGL backend.
     * * This class implements the graphics interface for OpenGL, managing shader programs,
     * uniform uploads, texture binding, framebuffer operations, and render pipeline states
     * (e.g., culling, depth testing, blending). It serves as the primary bridge between
     * the engine's abstract rendering commands and OpenGL's API calls.
     */
    class OpenGLContext : public IGraphicsContext {
    public:
    public:
        /**
         * @brief Creates a shader program from vertex and fragment shader source files.
         * @param vertSrc Path to the vertex shader source file.
         * @param fragSrc Path to the fragment shader source file.
         * @return A ShaderProgramHandle identifying the created program.
         */
        ShaderProgramHandle createShaderProgram(const std::string& vertSrc, const std::string& fragSrc) override;

        /**
         * @brief Compiles and links a standalone compute shader program.
         * @param computeSrc Path to the compute shader source file.
         * @return A ShaderProgramHandle identifying the created compute program.
         */
        ShaderProgramHandle createComputeProgram(const std::string& computeSrc) override;
        ShaderProgramHandle createComputeProgram(const std::string& computeSrc, const char* entryPoint, const char* stageName) override;
        /**
         * @brief Compiles vertex and fragment shaders and links them into a program.
         * @param vertSrc Path to the vertex shader.
         * @param fragSrc Path to the fragment shader.
         * @return The OpenGL program ID (apiID).
         */
        u32 compileOpenGLShader(const std::string& vertSrc, const std::string& fragSrc);

        /**
         * @brief Destroys an existing shader program and frees GPU resources.
         * @param handle The handle to the shader program to delete.
         */
        void destroyShaderProgram(ShaderProgramHandle handle) override;

        /** @brief Unbinds the currently bound 2D texture (binds texture id 0 to GL_TEXTURE_2D). */
        void unbindTexture() override;

        /**
         * @brief Reads a shader source file from disk into a string.
         * @param shaderPath The filesystem path to the shader source.
         * @return The shader source code as a string.
         */
        std::string readShaderSource(const char* shaderPath);

        /**
         * @brief Reads a file from disk into a string without newline translation.
         * @details Unlike readShaderSource(), this opens the stream with std::ios::binary,
         *          which is required for precompiled SPIR-V (.spv) files - text-mode reads on
         *          Windows corrupt any raw byte pair that happens to match "\\r\\n".
         * @param shaderPath The filesystem path to the binary file (typically a .spv).
         * @return The raw file contents as a string.
         */
        std::string readShaderBinary(const char* shaderPath);

        /**
         * @brief Reads, compiles and links a compute shader source file into a standalone OpenGL program.
         * @param computeSrc Path to the compute shader source file.
         * @return The OpenGL program ID (apiID).
         */
        u32 compileOpenGLComputeShader(const std::string& computeSrc);

        u32 compileOpenGLSpirvComputeShader(const std::string& computeSrc, const char* entryPoint, const char* stageName);

        /**
         * @brief Dispatches a compute shader with the specified group counts, also sets the memory barrier.
         * @param handle The handle of the compute shader program.
         * @param groupCountX Number of work groups to dispatch in the X dimension.
         * @param groupCountY Number of work groups to dispatch in the Y dimension.
         * @param groupCountZ Number of work groups to dispatch in the Z dimension.
         */
        void dispatchCompute(ShaderProgramHandle handle, u32 groupCountX, u32 groupCountY, u32 groupCountZ) override;

        /**
         * @brief Sets the active shader program for subsequent draw calls.
         * @param handle The handle of the shader program to use.
         */
        void bindShaderProgram(ShaderProgramHandle handle) override;

        /**
         * @brief Submits a collection of uniforms to the currently active shader.
         * @param uniforms A span containing uniform data.
         */
        void submitUniforms(std::span<const UniformData> uniforms) override;

        /**
         * @brief Binds multiple textures to their designated slots.
         * @param textures A span of texture binding structures.
         */
        void bindTextures(std::span<const TextureBinding> textures) override;

        /**
         * @brief Performs shader reflection to retrieve uniform metadata.
         * @param handle The shader program handle to reflect.
         * @return A vector of ReflectedUniform structures.
         */
        std::vector<ReflectedUniform> getProgramUniforms(ShaderProgramHandle handle) override;

        /**
         * @brief Configures render pipeline states like culling, depth testing, and blending.
         * @param specs The RenderPassSpecs defining the desired pipeline state.
         */
        void setRenderPassSpecs(const RenderPassSpecs& specs) override;

        /**
         * @brief Executes an indexed draw command using the specified vertex array.
         * @param vao The ID of the vertex array object.
         * @param indexCount The number of indices to render.
         */
        void drawIndexed(u32 vao, u32 indexCount, u32 indexOffset = 0) override;

        /**
         * @brief Executes a non-indexed draw command using the specified vertex array.
         * @param vao The ID of the vertex array object.
         * @param vertexCount The number of vertices to render.
         */
        void drawArrays(u32 vao, u32 vertexCount) override;

        /**
         * @brief Binds a framebuffer for rendering.
         * @param framebuffer The framebuffer to bind (pass nullptr for the default framebuffer).
         */
        void bindFramebuffer(const RHIFramebuffer* framebuffer) override;

        /**
         * @brief Unbinds the currently bound framebuffer, reverting to the default framebuffer.
         */
        void unbindFramebuffer() override;

        /**
         * @brief Copies a region of the framebuffer from source to target.
         * @param source The source framebuffer.
         * @param target The target framebuffer (pass nullptr to target the default framebuffer).
         * @param copyDepth Whether to include the depth buffer in the blit operation.
         */
        void blitFramebuffer(const RHIFramebuffer* source, const RHIFramebuffer* target, bool copyDepth = false) override;

        /**
         * @brief Generates an empty Vertex Array Object (VAO).
         * @return The VAO handle.
         */
        u32 createEmptyVAO() override;

        /**
         * @brief Clears the specified buffers with a given color.
         * @param flags The buffers to clear (Color, Depth, Stencil).
         * @param color The clear color (vec4).
         */
        void clear(ClearFlags flags, const vec4& color) override;

        /**
         * @brief Clears the specified buffers.
         * @param flags The buffers to clear (Color, Depth, Stencil).
         */
        void clear(ClearFlags flags) override;

        /**
         * @brief Initializes the rendering context and sets global OpenGL defaults.
         */
        void initRenderContext() override;

        /**
         * @brief Creates a constant buffer for uniform data.
         * @param size Size in bytes.
         * @return A shared pointer to the created RHIConstantBuffer.
         */
        std::shared_ptr<RHIConstantBuffer> createConstantBuffer(u32 size) override;

        /**
         * @brief Sets the active viewport dimensions.
         * @param vpPosX X-offset of the viewport.
         * @param vpPosY Y-offset of the viewport.
         * @param vpWidth Width of the viewport.
         * @param vpHeight Height of the viewport.
         */
        void setViewport(i32 vpPosX, i32 vpPosY, u32 vpWidth, u32 vpHeight) override;

        /**
         * @brief Creates a framebuffer object based on the provided specifications.
         * @param specs The framebuffer requirements.
         * @return The created RHIFramebuffer; convertible to a shared_ptr where shared ownership is wanted.
         */
        std::unique_ptr<RHIFramebuffer> createFramebuffer(const FramebufferSpecification& specs) override;

        /**
         * @brief Deletes a vertex array object from the GPU.
         * @param vao The VAO ID to delete.
         */
        void deleteVertexArrays(u32 vao) override;

        /**
         * @brief Retrieves the location of a uniform variable within a shader.
         * @param shader The shader program handle.
         * @param name The name of the uniform.
         * @return The uniform location integer.
         */
        i32 getUniformLocation(ShaderProgramHandle shader, const std::string& name);

        /**
         * @brief Binds a 2D texture to a specific texture unit.
         * @param slot The texture unit index.
         * @param textureID The OpenGL texture ID.
         */
        void bindShaderTexture(u32 slot, const Texture& tex) override;

        /** @brief Binds a texture as the active GL_TEXTURE_2D target (e.g. before setting parameters or as a framebuffer attachment target). */
        void bindToTarget(const Texture& tex) override;

        /**
         * @brief Attaches a cubemap face to the currently bound framebuffer.
         * @param faceIndex The index of the cubemap face (0-5).
         * @param cubemapTexID The OpenGL texture ID of the cubemap.
         */
        void attachCubemapFace(u32 faceIndex, u32 cubemapTexID) override;

        /**
         * @brief Sets a matrix uniform parameter.
         */
        void setParameter(ShaderProgramHandle shader, const std::string& name, const mat4& matrix) override;

        /**
         * @brief Sets a vector uniform parameter.
         */
        void setParameter(ShaderProgramHandle shader, const std::string& name, const vec3& vector) override;

        /**
         * @brief Sets an integer uniform parameter.
         */
        void setParameter(ShaderProgramHandle shader, const std::string& name, i32 value) override;

        /**
         * @brief Applies sampler state parameters to the currently bound texture.
         * @param state The SamplerState configuration.
         */
        void bindSamplerState(u32 textureID, const SamplerState& state) override;

        /**
         * @brief Binds a cubemap texture to a specific texture unit.
         */
        void bindTextureCube(u32 slot,const CubemapData& tex) override;

        /**
         * @brief Attaches a 2D texture level to the currently bound framebuffer.
         */
        void framebufferTexture2D(u32 faceIndex,const CubemapData& tex, u32 mip) override;

        /**
         * @brief Attaches a 2D texture's mip level to the currently bound framebuffer's color attachment 0.
         * @param tex The 2D texture to attach.
         * @param mip The mip level to attach.
         */
        void framebufferTexture2D(const Texture& tex, u32 mip);

         /**
         * @brief Allocates the physical memory (VRAM) on the GPU for a texture.
         *
         * This function takes a prepared texture "blueprint", generates a unique
         * hardware ID, binds the corresponding target, and allocates the memory
         * based on the internal format.
         *
         * @warning Because this communicates directly with the graphics API, this
         * function MUST be called on the main render thread.
         *
         * @param texture Reference to the pre-configured texture object. Upon
         * successful execution, the object receives a valid API-assigned textureID.
         */
        void allocateTexture(Texture& texture) override;
         /**
         * @brief Creates the metadata for a new texture on the CPU (blueprint).
         *
         * This function does NOT communicate with the GPU and does not reserve
         * any VRAM. It merely prepares the data object. Since it is thread-safe,
         * it is perfectly suited for preparing textures asynchronously (in the background).
         *
         * @param width   The width of the texture in pixels.
         * @param height  The height of the texture in pixels.
         * @param sampler The desired sampler state (filtering, wrapping).
         * @param format  The hardware-independent storage format. Default is RGBA8_UNORM
         * (linear color space). For pure color textures, SRGB should
         * be explicitly passed.
         *
         * @return A configured Texture object (ID = 0, ready for allocateTexture).
         */
        Texture generateTexture(u32 width, u32 height, SamplerState& sampler, TextureFormat format = TextureFormat::RGBA8_UNORM) override;
         /**
         * @brief Automatically generates the mipmap levels (LOD chains) for the active texture.
         *
         * Creates halved versions of the image data down to 1x1 pixel. This is
         * essential for performance (texture cache) and to prevent edge flickering
         * (aliasing) on distant objects. The corresponding texture must be bound
         * before calling this function.
         *
         * @param type The architectural texture type (e.g., 2D, Cubemap) so the RHI
         * knows which target (e.g., GL_TEXTURE_2D) to calculate the mipmaps for.
         */
        void generateMipmap(TextureType type) override;

        /**
         * @brief Maps a backend-agnostic @c DrawMode to its OpenGL primitive-type enum.
         * @param mode The primitive topology to translate.
         * @return The corresponding GL_POINTS/GL_LINES/GL_TRIANGLES constant (falls back to GL_TRIANGLES for unknown values).
         */
        inline GLenum translateDrawModeToOpenGL(DrawMode mode);

        /**
         * @brief Issues a non-indexed, instanced draw call.
         * @details Core-profile OpenGL requires some VAO to be bound for any draw call, even one
         *          with zero vertex attributes (e.g. SSBO/@c gl_InstanceID-driven rendering like
         *          the particle system). To keep that OpenGL-only requirement out of the
         *          backend-agnostic @c IGraphicsContext interface, this lazily creates and binds a
         *          single shared, permanently empty VAO (@c m_AttributelessVAO) on first use rather
         *          than taking a VAO handle as a parameter.
         * @param mode Primitive topology to draw.
         * @param vertexCount Number of vertices per instance.
         * @param instanceCount Number of instances to draw.
         * @param firstVertex Index of the first vertex to start drawing from.
         */
		void drawInstanced(DrawMode mode, u32 vertexCount, u32 instanceCount, u32 firstVertex) override;
    private:
        RenderPassSpecs m_CurrentSpecs; ///< Pipeline state (culling, depth test, blend, rasterizer mode) applied by the last setRenderPassSpecs() call.
        bool m_IsFirstContextInit = true; ///< Guards one-time global GL state setup in initRenderContext().
        u32 m_AttributelessVAO = 0; ///< Lazily-created, permanently empty VAO shared by all attributeless instanced draws (see drawInstanced()); 0 until first use.
    };
}