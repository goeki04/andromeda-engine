#pragma once

/**
 * @file a_particle.hpp
 * @brief GPU-driven particle system data types (particle payload and emitter).
 */

#include "a_Primitives.hpp"
#include "a_rhi_types.hpp"
#include <variant>
#include "a_rhi_storage_buffer.hpp"
#include "a_IGraphicsContext.hpp"
#include "a_shader_generated_compute.hpp"
#include "a_math.hpp"
#include "a_esphome_sensor_entity.hpp"
#include "scene.hpp"

namespace Andromeda {
	class ResourceManager;
	struct SphereShape {
		float radius = 1.0f; ///< Radius of the sphere shape.
	};

	struct HemisphereShape {
		float radius = 1.0f; ///< Radius of the hemisphere shape.
	};

	struct ConeShape {
		float radius = 1.0f; ///< Radius of the base of the cone.
		float height = 2.0f; ///< Height of the cone.
		float angle = 25.0f; ///< Angle of the cone in degrees.
	};

	struct CylinderShape {
		float radius = 1.0f;
		float height = 2.0f;
	};

	struct BoxShape {
		vec3 dimensions = vec3(1.0f); ///< Dimensions of the box shape along each axis.
	};

	enum class ParticleEmitterShape {
		Sphere, ///< Emit particles from a sphere surface.
		Cone,   ///< Emit particles from a cone surface.
		Cube,   ///< Emit particles from a cube volume.
		Cylinder, ///< Emit particles from a cylinder volume.
		Hemisphere ///< Emit particles from a hemisphere surface.
	};

	struct GroupRange
	{
		Entity entity;
		u32 groupId;
		u32 baseIndex;
		u32 count;
	};

	/**
	 * @struct ParticleEmitter
	 * @brief Owns and drives a pool of particles via a compute (update) and a render shader.
	 *
	 * @details Holds the emission parameters and the two shader programs that make up the
	 *          GPU pipeline: a compute shader that integrates particle motion in a storage
	 *          buffer, and a render shader that draws the resulting particles.
	 */
	struct ParticleEmitter {
	public:
		std::variant<SphereShape, HemisphereShape, ConeShape, BoxShape, CylinderShape> shape; ///< Shape of the emitter.
		/** @brief Allocates the GPU storage buffer and loads/links the compute and render shaders. */
		void initialize(IGraphicsContext& context, ResourceManager* rm, amath::CameraData& data);

		/** @brief Advances the simulation by dispatching the compute shader for this frame (reads the frame's delta time from @c SystemManager::s_deltaTime). */
		void update();

		/** @brief Issues the draw call that renders the current particle state. */
		void render();

		void applyGroup(const ParticleGroup& group, const vec3& worldPos, const GroupRange& range);
		bool isInitialized();
	private:
		static std::string getComputeKernelName(std::variant<SphereShape, HemisphereShape, ConeShape, BoxShape, CylinderShape> shape);
		bool m_IsInitialized = false; ///< Tracks whether the emitter has been initialized.
		ShaderProgramHandle m_ComputeShader; ///< Compute shader that updates particle positions/velocities.
		ShaderProgramHandle m_RenderShader;  ///< Shader program used to render the particles.
		RHIStorageBuffer m_ParticleBuffer; ///< GPU storage buffer that holds the particle pool.
		RHIConstantBuffer m_EmitterSettingsBuffer; ///< Constant buffer that holds the emitter settings.
		RHIConstantBuffer m_CameraDataBuffer; ///< Constant buffer that holds the camera data.
        RHIConstantBuffer m_MaterialBuffer;   ///< Constant buffer that holds the material data for rendering.
        /// Emitter settings that control particle emission and behavior.
        /// Brace-initialized on purpose: the compute kernel reads baseIndex, which nothing
        /// assigns, and an indeterminate value there would index the SSBO out of bounds.
        Generated::Compute::EmitterSettings m_EmitterSettings{};
		amath::CameraData* m_SceneCamera; ///< Pointer to the camera data used for rendering.
		IGraphicsContext* m_Context = nullptr; ///< Pointer to the graphics context for rendering.
		float m_SpawnAccumulator = 0.0f; ///< Accumulates time to determine when to spawn new particles.
        u32 m_Capacity = 0;                    ///< Maximum number of particles that can be emitted.
	};

}
