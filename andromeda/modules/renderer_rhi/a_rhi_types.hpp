#pragma once

/**
 * @file a_rhi_types.hpp
 * @brief Backend-agnostic enums and small POD types shared across the RHI (render hardware interface).
 */

#include "a_primitives.hpp"
#include <string>
namespace Andromeda {
    /**
    * @enum ShaderDataType
    * @brief Supported uniform and input data types for shader communication.
    */
    enum class ShaderDataType {
        Mat4, Mat3, Vec4, Vec3, Vec2, Float, Int, Texture2D, TextureCube
    };

    /**
     * @enum CullMode
     * @brief Defines the polygon culling behavior for the rasterizer.
     */
    enum class CullMode {
        None, Back, Front
    };

    /**
	 * @enum FillMode
	 * @brief DrawMode is primarily used for instanced rendering to tell the GPU how to interpret the vertex data.
     */
	enum class DrawMode {
		Triangles, Lines, Points
	};

    /**
     * @enum BufferUsage
     * @brief Hints how a GPU buffer's data will be accessed, mirroring OpenGL's usage tokens.
     * @details The first word describes update frequency (Stream = once and used a few times,
     *          Static = set once and used many times, Dynamic = updated repeatedly), the second
     *          the access pattern (Draw = CPU writes/GPU reads, Read = GPU writes/CPU reads,
     *          Copy = GPU writes/GPU reads).
     */
    enum class BufferUsage {
        StreamDraw, StreamRead, StreamCopy,    ///< Data set once, used at most a few times.
        StaticDraw, StaticRead, StaticCopy,    ///< Data set once, used many times.
        DynamicDraw, DynamicRead, DynamicCopy  ///< Data updated frequently.
    };

    /**
     * @brief Maps a backend-agnostic @c BufferUsage to its raw OpenGL enum value.
     * @param usage The usage hint to translate.
     * @return The corresponding GL usage constant (defaults to GL_DYNAMIC_DRAW for unknown values).
     */
    [[nodiscard]] inline constexpr u32 BufferUsageToGL(BufferUsage usage) noexcept {
        switch (usage) {
        case BufferUsage::StreamDraw:  return 0x88E0; // GL_STREAM_DRAW
        case BufferUsage::StreamRead:  return 0x88E1; // GL_STREAM_READ
        case BufferUsage::StreamCopy:  return 0x88E2; // GL_STREAM_COPY

        case BufferUsage::StaticDraw:  return 0x88E4; // GL_STATIC_DRAW
        case BufferUsage::StaticRead:  return 0x88E5; // GL_STATIC_READ
        case BufferUsage::StaticCopy:  return 0x88E6; // GL_STATIC_COPY

        case BufferUsage::DynamicDraw: return 0x88E8; // GL_DYNAMIC_DRAW
        case BufferUsage::DynamicRead: return 0x88E9; // GL_DYNAMIC_READ
        case BufferUsage::DynamicCopy: return 0x88EA; // GL_DYNAMIC_COPY
        }
        return 0x88E8; // Fallback (GL_DYNAMIC_DRAW)
    }

    /**
     * @enum DepthFunc
     * @brief Defines the comparison function for the depth test.
     */
    enum class DepthFunc { Less, Equal, LEqual };

    /**
     * @enum BlendMode
     * @brief Defines the blending equation used for transparent objects.
     */
    enum class BlendMode { None, AlphaBlend };

    /**
     * @enum RasterizerMode
     * @brief Determines how polygons are rasterized (e.g., solid or wireframe).
     */
    enum class RasterizerMode { Fill, Wireframe };

    /**
     * @struct RenderPassSpecs
     * @brief Encapsulates the global state configuration for a render pass.
     * * Used by the IGraphicsContext to update pipeline state (culling, depth test, etc.)
     * before issuing draw commands.
     */
    struct RenderPassSpecs {
        RasterizerMode rasterizerMode = RasterizerMode::Fill; ///< Whether polygons are rasterized solid or as wireframe.
        CullMode cullMode = CullMode::Back;                   ///< Which polygon winding to discard, if any.
        bool depthTest = true;                                ///< Whether the depth test is enabled for this pass.
        DepthFunc depthFunction = DepthFunc::Less;             ///< Comparison function used by the depth test.
        BlendMode blendMode = BlendMode::None;                ///< Blending equation used for transparent geometry.
        bool depthWrite = true;
    };

    /**
     * @struct ShaderProgramHandle
     * @brief An opaque handle to an underlying GPU shader program.
     */
    struct ShaderProgramHandle {
        u32 apiID = 0; ///< Backend-specific program id (e.g. an OpenGL program object); 0 = not created.
    };

    /**
     * @struct TextureBinding
     * @brief Associates a texture with a specific GPU binding slot.
     */
    struct TextureBinding {
        u32 apiID = 0; ///< Backend-specific texture id.
        u32 slot = 0;  ///< The sampler/binding slot the texture is bound to.
    };

    /**
     * @struct ReflectedUniform
     * @brief Metadata for a uniform variable discovered via shader reflection.
     */
    struct ReflectedUniform {
        std::string name;    ///< The uniform's name as declared in the shader source.
        u32 location;        ///< The GPU-assigned location of the uniform within its program.
        ShaderDataType type; ///< The uniform's platform-agnostic data type.
    };

    /**
     * @struct UniformData
     * @brief Encapsulates uniform data to be uploaded to the GPU.
     */
    struct UniformData {
        u32 location;        ///< The GPU-assigned location to upload this value to.
        u32 size;             ///< Size of the value pointed to by @c dataPtr, in bytes.
        const void* dataPtr; ///< Pointer to the raw bytes to upload; must stay valid for the duration of the upload call.
        ShaderDataType type; ///< The value's platform-agnostic data type.
    };
}