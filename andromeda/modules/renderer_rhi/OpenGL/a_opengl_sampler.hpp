#pragma once

/**
 * @file a_opengl_sampler.hpp
 * @brief OpenGL helper that translates an engine @c SamplerState into glTexParameter calls.
 */

#include "a_samplerState.hpp"
#include <GL/glew.h>

namespace Andromeda {
    /**
     * @struct GLSampler
     * @brief A backend-specific wrapper for applying sampler configurations to OpenGL textures.
     * * The GLSampler struct acts as a bridge between the API-agnostic SamplerState and
     * OpenGL state commands. It stores a desired sampler configuration and allows
     * explicit application of these parameters to a bound OpenGL texture object.
     */
    struct GLSampler {
        /**
         * @brief The configuration state defining the filtering and wrapping behavior.
         */
        SamplerState state;
        /**
         * @brief Constructs a new GLSampler object with the specified sampler state.
         * * @param state The API-agnostic sampler configuration to be applied later.
         */
        GLSampler(const SamplerState& state) : state(state) {}

        /**
         * @brief Maps an engine texture type to its OpenGL texture target.
         * @param type The texture type (2D or cubemap).
         * @return GL_TEXTURE_2D for 2D textures, otherwise GL_TEXTURE_CUBE_MAP.
         */
        static GLenum GetTarget(TextureType type) {
            return (type == TextureType::Texture2D) ? GL_TEXTURE_2D : GL_TEXTURE_CUBE_MAP;
        }

        // ReSharper disable once CppDoxygenUnresolvedReference
        /**
         * @brief Explicitly applies the stored sampler state to the currently bound OpenGL texture.
         * * This method issues the necessary glTexParameteri calls to configure the
         * filtering and wrapping modes for the specified OpenGL texture target.
         * * @param target The OpenGL texture target to apply the parameters to
         * (e.g., GL_TEXTURE_2D, GL_TEXTURE_CUBE_MAP).
         * * @note The caller must ensure that the appropriate texture is currently
         * bound to the target provided.
         */
        void Apply() const {
            GLint minF;

            switch (state.minFilter) {
            case FilterModeMin::Nearest:    minF = GL_NEAREST;                break;
            case FilterModeMin::Linear:     minF = GL_LINEAR;                 break;
            case FilterModeMin::Billinear:  minF = GL_LINEAR_MIPMAP_NEAREST;  break; 
            case FilterModeMin::Trillinear: minF = GL_LINEAR_MIPMAP_LINEAR;   break;
            default:                        minF = GL_LINEAR;                 break;
            }

            GLenum target = GetTarget(state.type);
            glTexParameteri(target, GL_TEXTURE_MIN_FILTER, minF);

            GLint magF = (state.magFilter == FilterModeMag::Nearest) ? GL_NEAREST : GL_LINEAR;
            glTexParameteri(target, GL_TEXTURE_MAG_FILTER, magF);
            auto MapWrap = [](WrapMode mode) {
                return (mode == WrapMode::ClampToEdge) ? GL_CLAMP_TO_EDGE : GL_REPEAT;
                };

            glTexParameteri(target, GL_TEXTURE_WRAP_S, MapWrap(state.wrapS));
            glTexParameteri(target, GL_TEXTURE_WRAP_T, MapWrap(state.wrapT));
            glTexParameteri(target, GL_TEXTURE_WRAP_R, MapWrap(state.wrapR));
        }

        /**
         * @brief Maps an engine texture type to its OpenGL texture target (switch-based variant).
         * @param type The texture type to translate.
         * @return The corresponding GL texture target (defaults to GL_TEXTURE_2D).
         */
        static GLenum textureTypeToGLTextureTarget(TextureType type) {
            switch (type) {
            case TextureType::Texture2D: return GL_TEXTURE_2D;
            case TextureType::Cubemap:   return GL_TEXTURE_CUBE_MAP;
            default:                     return GL_TEXTURE_2D;
            }
        }
    };
}