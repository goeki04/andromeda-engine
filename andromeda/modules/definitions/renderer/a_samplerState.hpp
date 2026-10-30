#pragma once

/**
 * @file a_samplerState.hpp
 * @brief Backend-agnostic texture sampling configuration (filtering, wrapping, texture type).
 */

namespace Andromeda {

	/**
	 * @enum FilterModeMin
	 * @brief Minification filter applied when a texture is sampled smaller than its native size.
	 */
	enum class FilterModeMin {
		Nearest,    ///< Nearest-neighbor, no mipmapping.
        Linear,     ///< Linear, no mipmapping.
		Billinear,  ///< Bilinear filtering with nearest mip selection.
		Trillinear  ///< Trilinear filtering (linear within and between mips).
	};

	/**
	 * @enum FilterModeMag
	 * @brief Magnification filter applied when a texture is sampled larger than its native size.
	 */
	enum class FilterModeMag {
		Nearest, ///< Nearest-neighbor (blocky).
		Linear   ///< Linear (smooth).
	};

	/**
	 * @enum WrapMode
	 * @brief Behavior for texture coordinates outside the [0,1] range.
	 */
	enum class WrapMode {
		ClampToEdge ///< Coordinates are clamped to the edge texels.
	};

	/**
	 * @enum TextureType
	 * @brief The kind of texture a sampler targets.
	 */
	enum class TextureType {
		Texture2D, ///< Standard 2D texture.
		Cubemap    ///< Six-faced cubemap texture.
	};

    /**
     * @struct SamplerState
     * @brief Bundles all sampling parameters (filtering, wrapping, type) for a texture.
     */
    struct SamplerState {
        TextureType type;        ///< Whether the sampled texture is 2D or a cubemap.
        FilterModeMin minFilter; ///< Minification filter.
        FilterModeMag magFilter; ///< Magnification filter.
        WrapMode wrapS;          ///< Wrap mode along the S (U) axis.
        WrapMode wrapT;          ///< Wrap mode along the T (V) axis.
        WrapMode wrapR;          ///< Wrap mode along the R (W) axis (cubemaps/3D).

        /**
         * @brief Constructs a SamplerState with sensible defaults.
         * * Default settings:
         * - type: Texture2D
         * - minFilter: Billinear
         * - magFilter: Linear
         * - wrap modes: ClampToEdge
         */
        SamplerState() :
            type(TextureType::Texture2D),
            minFilter(FilterModeMin::Billinear),
            magFilter(FilterModeMag::Linear),
            wrapS(WrapMode::ClampToEdge),
            wrapT(WrapMode::ClampToEdge),
            wrapR(WrapMode::ClampToEdge) {
        }

        /**
         * @brief Constructs a SamplerState with custom values.
         */
        SamplerState(TextureType type, FilterModeMin minF, FilterModeMag magF, WrapMode s, WrapMode t, WrapMode r) :
            type(type), minFilter(minF), magFilter(magF), wrapS(s), wrapT(t), wrapR(r) {
        }
    };
}