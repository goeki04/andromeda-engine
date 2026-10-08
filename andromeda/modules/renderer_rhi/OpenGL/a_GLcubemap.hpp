#pragma once

/**
 * @file a_GLcubemap.hpp
 * @brief OpenGL cubemap utilities: uploading cubemaps and baking image-based-lighting (IBL) resources.
 */

#include "a_GLcubemap.hpp"
#include <GL/glew.h>
#include "a_primitives.hpp"
#include "a_samplerState.hpp"
#include "a_IGraphicsContext.hpp"
#include "a_CubemapData.hpp"
#include "a_math.hpp"
#include "a_logger.hpp"
namespace Andromeda {
	/**
	 * @class CubemapGL
	 * @brief Static OpenGL helpers for creating cubemap textures and the IBL baking pipeline.
	 *
	 * @details Provides upload paths for both LDR (six-face) and HDR (equirectangular) sources,
	 *          allocation of empty cubemaps used as render targets during baking, and the
	 *          equirectangular-to-cubemap conversion. Also exposes the fixed projection/view
	 *          matrices used to render each of the six cube faces.
	 */
	class CubemapGL {
	public:
		/**
		 * @brief Uploads cubemap pixel data to the GPU, choosing the HDR or LDR path automatically.
		 * @param data The cubemap data; its CPU pixel buffers are freed after upload.
		 */
		static void CubemapTextureUploadGL(CubemapData& data) {
			if (data.isHDR)
			{
				createCubeMapTextureHDR(data);
			}
			else
			{
				createCubeMapTextureLDR(data);
			}

			data.freePixelData();
			data.pixelData.fill(nullptr);
		}

		/**
		 * @brief Uploads HDR panorama pixel data into a float 2D texture (equirectangular source).
		 * @param data The cubemap data carrying the HDR source image; receives the created texture ID.
		 */
		static void createCubeMapTextureHDR(CubemapData& data)
		{
			GLuint tempID;
			glGenTextures(1, &tempID);
			data.textureID = tempID;
			glBindTexture(GL_TEXTURE_2D,  data.textureID);
			if (data.pixelData[0]) {
				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F,
							 data.width, data.height, 0, GL_RGB, GL_FLOAT,
							 data.pixelData[0]);

				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			}
		}

		/**
		 * @brief Uploads six LDR face images into an OpenGL cubemap texture.
		 * @param data The cubemap data carrying the six face buffers; receives the created texture ID.
		 */
		static void createCubeMapTextureLDR(CubemapData& data)
		{
			GLuint tempID;
			glGenTextures(1, &tempID);
			data.textureID = tempID;
			glBindTexture(GL_TEXTURE_CUBE_MAP,  data.textureID);
			for (u32 i = 0; i < 6; ++i)
			{
				if (data.pixelData[i]) {
					glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB,
								 data.width, data.height, 0, GL_RGB, GL_UNSIGNED_BYTE,
								 data.pixelData[i]);
				}
			}
			glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
		}

		/**
		 * @brief Allocates an empty float cubemap (no mipmaps) for use as a baking render target.
		 * @param context Graphics context used to apply the sampler state.
		 * @param data Cubemap data providing the face dimensions; receives the created texture ID.
		 * @param state Sampler state; must have @c TextureType::Cubemap or the call fails.
		 */
		static void AllocateCubemapTexture(IGraphicsContext* context,CubemapData* data,const SamplerState& state) {
			if (state.type != TextureType::Cubemap) {
				A_ERROR("Allocating cubemap texture failed: Invalid sampler state type");
				return;
			}
			u32 textureID = 0;
			glGenTextures(1, &textureID);
			glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);
			for (unsigned int i = 0; i < 6; ++i)
			{
				glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F,
					data->width, data->height, 0, GL_RGB, GL_FLOAT, nullptr);
			}
			data->textureID = textureID;
			context->bindSamplerState(data->textureID, state);
			glBindTexture(GL_TEXTURE_CUBE_MAP,0);
		}

		/**
		 * @brief Allocates an empty float cubemap with a mipmap chain (e.g. for prefiltered specular maps).
		 * @param context Graphics context used to apply the sampler state.
		 * @param data Cubemap data providing the face dimensions; receives the created texture ID.
		 * @param state Sampler state; must have @c TextureType::Cubemap or the call fails.
		 */
		static void AllocateCubemapTextureWithMipmap(IGraphicsContext* context, CubemapData* data, const SamplerState& state) {
			if (state.type != TextureType::Cubemap) {
				A_ERROR("Allocating cubemap texture failed: Invalid sampler state type");
				return;
			}

			u32 textureID = 0;
			glGenTextures(1, &textureID);
			glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);

			for (u32 i = 0; i < 6; ++i) {
				glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F, data->width, data->height, 0, GL_RGB, GL_FLOAT, nullptr);
			}
			glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
			data->textureID = textureID;
			context->bindSamplerState(data->textureID, state);

			glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
		}

		/** @brief 90° FOV perspective projection used when rendering each cube face during baking. */
		static inline const mat4 cubeProjection = amath::perspective(amath::radians(90.0f), 1.0f, 0.1f, 10.0f);
		/** @brief The six view matrices (one per cube face) used to render into a cubemap. */
		static inline const mat4 cubeViews[] =
		{
		   amath::lookAt(vec3(0.0f, 0.0f, 0.0f), vec3(1.0f,  0.0f,  0.0f), vec3(0.0f, -1.0f,  0.0f)),
		   amath::lookAt(vec3(0.0f, 0.0f, 0.0f), vec3(-1.0f,  0.0f,  0.0f), vec3(0.0f, -1.0f,  0.0f)),
		   amath::lookAt(vec3(0.0f, 0.0f, 0.0f), vec3(0.0f,  1.0f,  0.0f), vec3(0.0f,  0.0f,  1.0f)),
		   amath::lookAt(vec3(0.0f, 0.0f, 0.0f), vec3(0.0f, -1.0f,  0.0f), vec3(0.0f,  0.0f, -1.0f)),
		   amath::lookAt(vec3(0.0f, 0.0f, 0.0f), vec3(0.0f,  0.0f,  1.0f), vec3(0.0f, -1.0f,  0.0f)),
		   amath::lookAt(vec3(0.0f, 0.0f, 0.0f), vec3(0.0f,  0.0f, -1.0f), vec3(0.0f, -1.0f,  0.0f))
		};

		/**
		 * @brief Renders an equirectangular HDR texture into the six faces of a cubemap.
		 * @param context The graphics context used to issue the render commands.
		 * @param shaderHandle The equirectangular-to-cubemap conversion shader.
		 * @param hdrTexture The source equirectangular HDR 2D texture.
		 * @param bakingFBO The framebuffer used as the render target during conversion.
		 * @param envCubemap The destination cubemap texture ID to render the faces into.
		 * @param renderCube Callback that draws the unit cube geometry for each face.
		 */
		static void ConvertEquiretangularToCubemap(IGraphicsContext* context,ShaderProgramHandle shaderHandle, GLuint& hdrTexture,const RHIFramebuffer* bakingFBO ,u32 envCubemap, std::function<void()> renderCube) {

			RenderPassSpecs specs;
			specs.rasterizerMode = RasterizerMode::Fill;
			specs.depthTest = true;
			specs.depthFunction = DepthFunc::LEqual;
			specs.cullMode = CullMode::None;
			context->setRenderPassSpecs(specs);
			context->bindShaderProgram(shaderHandle);
			context->setParameter(shaderHandle, "equirectangularMap", 0);
			context->setParameter(shaderHandle, "proj", cubeProjection);
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, hdrTexture);

			context->setViewport(0,0,512,512);
			context->bindFramebuffer(bakingFBO);
			for (u32 i = 0; i < 6; ++i) {
				context->setParameter(shaderHandle, "view", cubeViews[i]);
				context->attachCubemapFace(i, envCubemap);
				glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
				renderCube();
			}

			context->unbindFramebuffer();
			RenderPassSpecs resetSpecs;
			context->setRenderPassSpecs(resetSpecs);
		}
	};
}