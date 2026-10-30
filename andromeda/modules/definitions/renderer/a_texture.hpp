#pragma once

/**
 * @file a_texture.hpp
 * @brief Backend-agnostic texture description and supported pixel formats.
 */

#include "a_samplerState.hpp"
#include "a_primitives.hpp"
namespace Andromeda {
    /**
     * @enum TextureFormat
     * @brief Hardware-independent pixel storage formats for textures and render targets.
     * @details Suffixes follow common conventions: @c UNORM = normalized 8-bit, @c SRGB = gamma-encoded,
     *          @c FLOAT = floating point (HDR); the @c D... and @c S... entries are depth/stencil formats.
     */
    enum class TextureFormat : u8
    {
        None = 0,           ///< Unspecified / invalid format.

        RGBA8_UNORM,        ///< 8-bit per channel RGBA, linear.
        RGBA8_SRGB,         ///< 8-bit per channel RGBA, sRGB (gamma) encoded color.
        RG8_UNORM,          ///< 8-bit per channel two-channel (red/green).
        R8_UNORM,           ///< 8-bit single-channel (red).

        RGBA16_FLOAT,       ///< 16-bit float per channel RGBA (HDR).
        RG16_FLOAT,         ///< 16-bit float two-channel.
        RGBA32_FLOAT,       ///< 32-bit float per channel RGBA (high precision).

        D24_UNORM_S8_UINT,  ///< 24-bit depth + 8-bit stencil.
        D32_FLOAT           ///< 32-bit float depth.
    };

	/**
	 * @class Texture
	 * @brief Lightweight handle/description of a GPU texture (dimensions, format, sampler and API ID).
	 */
	class Texture {
	public:
		u32 width = 0, height = 0, channels = 0;            ///< Texture dimensions in pixels and channel count.
        TextureFormat internalFormat = TextureFormat::None; ///< GPU storage format of the texture.
		SamplerState sampler;                               ///< Filtering/wrapping configuration for sampling this texture.
		u32 textureID = 0;                                  ///< Backend texture handle (0 if not yet allocated).
	};
}