#pragma once

/**
 * @file renderer.h
 * @brief The rendering subsystem: owns the graphics context, framebuffers, and the multi-pass scene pipeline.
 */

#include "a_graphics_base.hpp"
#include "a_ISubsystem.hpp"
#include "a_components.hpp"
#include "a_rhi_framebuffer.hpp"
#include "a_primitives.hpp"
#include "OpenGL/a_opengl_handles.hpp"
#include "a_IGraphicsContext.hpp"
#include "OpenGL/a_OpenGLContext.hpp"
#include <a_CubemapData.hpp>
#include "a_rhi_constant_buffer.hpp"
#include "a_texture.hpp"
#include "a_particle.hpp"
namespace Andromeda {
	namespace amath {
		struct CameraData;
	}
	struct CubemapData;
	class SceneManager;
	enum class MaterialShaderType : int;
	namespace Window { class WindowManager; }
	class ResourceManager;
	class Mesh;

	/// <summary>
	/// TODO: abstract OpenGL specific code from the renderer into the rhi. Also if theres enough time, implement
	/// a Vulkan renderer. MacOS should also be supported via MoltenVK
	/// </summary>

	/**
	 * @enum AndromedaGXAPI
	 * @brief Identifies the graphics backend the renderer targets.
	 * @note Only @c OpenGL is currently implemented; the others are placeholders for planned backends.
	 */
	enum class AndromedaGXAPI {
		OpenGL,    ///< OpenGL backend (the only implemented one).
		Directx12, ///< Planned Direct3D 12 backend.
		Vulkan,    ///< Planned Vulkan backend.
		Metal      ///< Planned Metal backend (e.g., via MoltenVK on macOS).
	};

	/**
	 * @class Renderer
	 * @brief Drives all frame rendering: PBR scene pass, selection/outline, post-processing, and IBL baking.
	 *
	 * @details Owns the active @c IGraphicsContext (currently an @c OpenGLContext) and a set of
	 *          framebuffers used for the multi-pass pipeline (MSAA scene buffer, resolve buffer,
	 *          selection buffer, post-processing buffer, and an offscreen baking buffer). At startup
	 *          it bakes the image-based-lighting resources (irradiance map, prefiltered environment
	 *          map, and BRDF LUT) from the environment cubemap. Per frame, it renders the scene
	 *          geometry, applies selection highlighting and post-processing, and exposes the final
	 *          texture for the editor viewport.
	 */
	class Renderer : public ISubsystem {
	public:
		ivec2 m_FramebufferSize = glm::ivec2(0, 0); ///< Current size of the main render target, in pixels.
		static constexpr auto glsl_version = "#version 460"; ///< GLSL version string used when initializing ImGui's GL backend.
		static constexpr auto kMaxParticlesPerGroup = 100000; // 100k * 48 byte = ~4.8 MB per group
		/**
		 * @brief Gets the static compile-time string identifier of the subsystem.
		 * @return A string_view containing "Renderer".
		 */
		static constexpr std::string_view GetStaticName() { return "Renderer"; }

		/**
		 * @brief Gets the runtime string identifier of the subsystem.
		 * @return A C-string containing the subsystem's name.
		 */
		const char* getSubsystemName() const override {
			return GetStaticName().data();
		}

		/** @brief Initializes the graphics context, framebuffers, materials and bakes IBL resources. */
		void start() override;

		/** @brief Renders one complete frame (all passes) and handles any pending viewport resize. */
		void update() override;

		/** @brief Bakes the diffuse irradiance cubemap from the environment map (IBL precomputation). */
		void irradianceCubemapBaking();

		/** @brief Bakes the BRDF integration lookup texture used by the PBR shader (IBL precomputation). */
		void brdfLUTBaking();

		/**
		 * @brief Convenience helper to create a framebuffer object from a size and color-format list.
		 * @param size Framebuffer dimensions in pixels.
		 * @param formats The color attachment formats to create.
		 * @param samples MSAA sample count (1 = no multisampling).
		 * @return A shared pointer to the created framebuffer.
		 */
		std::shared_ptr<RHIFramebuffer> helperCreateFBO(ivec2 size, const std::vector<FramebufferTextureFormat>& formats, u32 samples);

		/** @brief Releases all GPU resources (framebuffers, context, materials). */
		void destroy() override;

		/**
		 * @brief Sets the camera whose view/projection drives scene rendering.
		 * @param camData Non-owning pointer to the active camera state.
		 */
		void setActiveCamera(amath::CameraData* camData);

		/** @brief Renders all scene geometry (the main PBR lighting pass). */
		void geometryPass() const;

		/**
		 * @brief Returns the final, post-processed scene texture for display in the editor viewport.
		 * @return The resolved color texture of the last frame.
		 */
		Texture getFinalSceneViewportTexture() const;

		/**
		 * @brief Requests a (debounced) resize of the render targets to a new viewport size.
		 * @param newSize The requested new size in pixels.
		 */
		void onViewportResize(ivec2 newSize);

		/** @brief Applies a pending resize once the debounce timer elapses (avoids reallocating every frame while dragging). */
		void processResizeTimer();

		/**
		 * @brief Renders the selected entity into the selection buffer for outline highlighting.
		 * @param selectedEntity The entity to highlight (or @c INVALID_ENTITY_ID for none).
		 */
		void selectionPass(ECS::Entity selectedEntity) const;

		/** @brief Applies post-processing (e.g. tonemapping/outline composite) to the resolved scene. */
		void postprocessingPass() const;

		/** @brief Binds the MSAA scene framebuffer and clears it to begin the scene pass. */
		void scenePassBegin() const;

		/** @brief Renders procedurally generated geometry (e.g. the editor ground grid). */
		void proceduralPass() const;

		/** @brief Renders visual effects (e.g. particles, post-processing effects). */
		void vfxPass();

		/** @brief Resolves the multisampled scene buffer into the single-sample scene texture. */
		void scenePassEndResolve() const;

		/** @brief Clears the default window framebuffer. */
		void windowClearPass();
		/**
		 * @brief Uploads cubemap pixel data to the GPU, creating the corresponding texture.
		 * @param data The cubemap data to upload; receives the resulting texture ID.
		 */
		void createCubemapTexture(CubemapData& data);
	private:
		AndromedaGXAPI m_Api = AndromedaGXAPI::OpenGL;            ///< Active graphics backend.
		ResourceManager* m_ResourceManager = nullptr;              ///< Cached resource manager (meshes, textures, shaders).
		SceneManager* m_SceneManager = nullptr;                    ///< Cached scene manager (entities to render).
		std::unique_ptr<OpenGLContext> m_GLContext = nullptr;      ///< Owned concrete OpenGL graphics context.
		IGraphicsContext* m_RenderContext = nullptr;              ///< Backend-agnostic view of @c m_GLContext used by the passes.
		amath::CameraData* m_Cam = nullptr;                        ///< Active camera used for view/projection.

		ECS::Entity m_SelectedForHighlighting = ECS::INVALID_ENTITY_ID; ///< Entity currently drawn with a selection outline.
		vec2 m_TexelSize = vec2(0.0, 0.0);                         ///< Reciprocal of the render-target size, for screen-space effects.
		u32 m_CubeVao = 0;                                         ///< VAO of the unit cube used for cubemap baking/skybox.

		bool m_WireframeActive = false;                           ///< Whether geometry is currently drawn in wireframe mode.
		MeshGPUHandle cubemapGpuHandle;                           ///< GPU buffers for the cube mesh used during IBL baking.
		float m_ResizeTimer = 0.0f;                               ///< Debounce countdown for pending viewport resizes.
		bool m_ResizePending = false;                             ///< True while a viewport resize is queued.
		ivec2 m_TargetSize = ivec2(0.0f);                         ///< The pending target size to resize to.

		CubemapData m_EnvironmentCubemap; ///< Source environment cubemap (skybox / IBL input).
		CubemapData m_IrradianceCubemap;  ///< Baked diffuse irradiance map for IBL.
		CubemapData m_PrefilterMap;       ///< Baked prefiltered specular environment map (mip chain) for IBL.
		Texture m_BrdfLUTTexture;         ///< Baked BRDF integration lookup texture for IBL.
		std::shared_ptr<RHIFramebuffer> m_MsaaBuffer;        ///< Multisampled framebuffer the scene is rendered into.
		std::shared_ptr<RHIFramebuffer> m_SceneBuffer;       ///< Resolved (single-sample) scene color buffer.
		std::shared_ptr<RHIFramebuffer> m_SelectionBuffer;   ///< Buffer holding the selected-entity mask for outlines.
		std::shared_ptr<RHIFramebuffer> m_PostprocessBuffer; ///< Target for the post-processing pass.
		std::shared_ptr<RHIFramebuffer> m_BakingBuffer;      ///< Offscreen buffer used during IBL baking.

		RHIConstantBuffer m_CameraUBO;      ///< UBO holding camera matrices (view/projection).
		RHIConstantBuffer m_ObjectUBO;      ///< UBO holding per-object data (model matrix, etc.).
		RHIConstantBuffer m_ColorUBO;       ///< UBO holding color parameters.
		RHIConstantBuffer m_GridUBO;        ///< UBO holding ground-grid transform data.
		RHIConstantBuffer m_GridParamsUBO;  ///< UBO holding ground-grid appearance parameters.
		RHIConstantBuffer m_OutlineUBO;     ///< UBO holding selection-outline parameters.
		RHIConstantBuffer m_LightUBO;       ///< UBO holding scene light data.
		RHIConstantBuffer m_pbrMaterialUBO; ///< UBO holding PBR material parameters.
		std::vector<GroupRange> m_GroupRanges;
		std::unordered_set<u64> m_UsedKeys;
        std::unordered_map<u64, ParticleEmitter> m_Emitters; ///< Map of particle emitters keyed by their unique IDs. key = entity << 32 | groupId
		/** @brief Internal one-time setup of the render context and global GL state. */
		void initRenderer();
		void particleUpdatePass();
		/** @brief Bakes the prefiltered specular environment map (IBL precomputation). */
        void prefilterCubemapBaking();
        /** @brief Subscribes the renderer to relevant engine events (e.g. wireframe toggle, viewport resize). */
        void registerEvents();
        /** @brief Creates all framebuffers used by the rendering pipeline. */
        void createFramebuffers();
		/** @brief Creates and registers the default materials/shaders used by the renderer. */
		void createMaterials();
	};
}