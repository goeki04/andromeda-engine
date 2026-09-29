#include "a_particle.hpp"
#include "a_logger.hpp"
#include "a_subsystem_manager.hpp"
#include "a_shader_generated.hpp"
#include "resource_manager.h"


namespace Andromeda {
	/**
	 * @brief Allocates the GPU-side particle pool and emitter settings buffer, and sets default emission parameters.
	 * @details Stores the (already loaded) compute and render shader handles, initializes
	 *          @c m_EmitterSettings with default values (origin position, 1000 max particles,
	 *          zeroed padding), then allocates the SSBO backing the particle pool
	 *          (`maxParticles * sizeof(Generated::Compute::Particle)` bytes) and the UBO
	 *          backing @c EmitterSettings.
	 * @param context The graphics context used to allocate GPU resources.
	 * @param computeShader Handle to the already-loaded particle update compute shader.
	 * @param renderShader Handle to the already-loaded particle render shader.
	 * @param data Reference to the camera data.
	 */
	void ParticleEmitter::initialize(IGraphicsContext& context, ResourceManager* rm, amath::CameraData& data) {

		m_Context = &context;
		if (m_Context == nullptr) {
			A_WARN("Graphics context is null. Cannot initialize ParticleEmitter.");
			return;
		}

		m_IsInitialized = true;
		shape = BoxShape{ vec3(1.0f) };
		std::string kernalName = getComputeKernelName(shape);
		std::string computePath = SHADER_SPV_PATH "particle.hlsl." + kernalName + ".spv";
		m_ComputeShader = rm->loadComputeShaderRHI(&context, "Particle_Compute_" + kernalName, computePath, "Particle Compute(HLSL)");
		m_RenderShader = rm->loadShaderRHI(&context, "Particle_Render", SHADER_PATH "Vfx/particle.vert", SHADER_PATH "Vfx/particle.frag");
        m_EmitterSettings.maxParticles = 1500;
        m_EmitterSettings.emitterPosition = vec3(0.0f);
        m_EmitterSettings.boxExtents = vec3(1.0f);
        m_EmitterSettings.startVelocity = vec3(0.0f, 1.0f, 0.0f);
        m_EmitterSettings.minLifetime = 2.0f;
        m_EmitterSettings.size = 1.0f;
        m_EmitterSettings.color = vec4(1.0f);
		m_ParticleBuffer.create(BufferUsage::DynamicDraw,m_EmitterSettings.maxParticles  * sizeof(Generated::Compute::Particle), nullptr);
		m_ParticleBuffer.clear();

		m_EmitterSettingsBuffer.initialize(sizeof(Generated::Compute::EmitterSettings));
		m_SceneCamera = &data;
		m_CameraDataBuffer.initialize(sizeof(Generated::CameraData));
        m_MaterialBuffer.initialize(sizeof(Generated::ParticleMaterial));
	}
	/**
	 * @brief Advances the particle simulation by dispatching the compute shader for this frame.
	 * @details No-ops if the emitter has no capacity or was never initialized. Otherwise binds
	 *          the particle SSBO (slot 1) and the @c EmitterSettings UBO (slot 2), refreshes
	 *          @c m_EmitterSettings.deltaTime from the current frame time, re-uploads it to
	 *          @c m_EmitterSettingsBuffer, and dispatches one compute work group per 256
	 *          particles (matching `layout(local_size_x = 256, ...)` in particle.comp).
	 *          Reads the frame's delta time from @c SystemManager::s_deltaTime.
	 */
	void ParticleEmitter::update() {
		if (m_EmitterSettings.maxParticles == 0) return;
		if (m_IsInitialized == false) {
			A_WARN("ParticleEmitter not initialized!");
			return;
		}
		m_Context->bindShaderProgram(m_ComputeShader);
		m_ParticleBuffer.bind(1);
		m_EmitterSettingsBuffer.bind(2);
		const float dt = SystemManager::s_paused ? 0.0f : SystemManager::s_deltaTime;
		m_EmitterSettings.deltaTime = dt;
		m_EmitterSettingsBuffer.setData(&m_EmitterSettings, sizeof(Generated::Compute::EmitterSettings));

		u32 threadGroupsX = (m_EmitterSettings.maxParticles + 255) / 256;
		m_Context->dispatchCompute(m_ComputeShader, threadGroupsX, 1, 1);
	}

	/**
	 * @brief Issues the draw call that renders the current particle state.
	 * @details No-ops if the emitter was never initialized or has no capacity. Otherwise binds
	 *          the render shader and the particle SSBO (slot 1) for it to read from, then draws
	 *          one point instance per particle (vertex-less, point-primitive instanced draw).
	 *          Alpha blending (needed for the soft circular falloff in particle.frag's alpha
	 *          channel to actually blend instead of being drawn opaque) is NOT enabled here -
	 *          the caller is responsible for that. @c Renderer::vfxPass() sets
	 *          @c RenderPassSpecs::blendMode = BlendMode::AlphaBlend before calling this, and
	 *          resets it back to the default afterwards.
	 */
	void ParticleEmitter::render() {
        if (!m_IsInitialized || m_EmitterSettings.maxParticles == 0)
		{
			A_WARN("ParticleEmitter not initialized or maxParticles is zero. Cannot render.");
			return;
		}

		m_Context->bindShaderProgram(m_RenderShader);
		m_ParticleBuffer.bind(1);
		m_CameraDataBuffer.bind(2);
		Generated::CameraData cameraData;
		cameraData.view = m_SceneCamera->viewMatrix;
		cameraData.proj = m_SceneCamera->projection;
		m_CameraDataBuffer.setData(&cameraData, sizeof(Generated::CameraData));
        Generated::ParticleMaterial material;
        material.particleColor = m_EmitterSettings.color;
        m_MaterialBuffer.setData(&material, sizeof(Generated::ParticleMaterial));
        m_MaterialBuffer.bind(4);
		m_Context->drawInstanced(DrawMode::Points, 1, m_EmitterSettings.maxParticles, 0);
	}

	void ParticleEmitter::applyGroup(const ParticleGroup& group, const vec3& worldPos, const GroupRange& range)
	{
        const u32 desired = range.count;
        m_EmitterSettings.activeParticleCount = desired;
        if (desired > m_Capacity) { 
            m_Capacity = desired;
            m_ParticleBuffer.create(BufferUsage::DynamicDraw, m_Capacity * sizeof(Generated::Compute::Particle),
                                    nullptr);
            m_ParticleBuffer.clear();
        }
        m_EmitterSettings.maxParticles = m_Capacity;
		m_EmitterSettings.size = group.size;
		m_EmitterSettings.color = vec4(group.particleColor,1.0f);
		m_EmitterSettings.emitterPosition = worldPos;
		m_EmitterSettings.minLifetime = group.minLifetime;
		m_EmitterSettings.startVelocity = group.velocity;
		m_EmitterSettings.boxExtents = group.boxExtents;
	}

	bool ParticleEmitter::isInitialized()
	{
		return m_IsInitialized;
	}

	std::string ParticleEmitter::getComputeKernelName(std::variant<SphereShape, HemisphereShape, ConeShape, BoxShape, CylinderShape> shape)
	{
		struct KernelNameVisitor {
			std::string operator()(const BoxShape&)         const { return "BoxParticle"; }
			std::string operator()(const SphereShape&)      const { return "SphereParticle"; }
			std::string operator()(const ConeShape&)        const { return "ConeParticle"; }
			std::string operator()(const HemisphereShape&)  const { return "HemisphereParticle"; }
			std::string operator()(const CylinderShape&)    const { return "CylinderParticle"; }
		};
		std::string kernelName = std::visit(KernelNameVisitor{}, shape);

		return kernelName;
	}
}
