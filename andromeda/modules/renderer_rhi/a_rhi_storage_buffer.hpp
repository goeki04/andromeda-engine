#pragma once

/**
 * @file a_rhi_storage_buffer.hpp
 * @brief Backend-agnostic shader storage buffer (SSBO) wrapper, implemented via the Pimpl idiom.
 */

#include "a_primitives.hpp"
#include "a_rhi_types.hpp"
#include <memory>
namespace Andromeda {

	/**
	 * @class StorageBufferImpl
	 * @brief Opaque, backend-specific state of a storage buffer.
	 * @details Only forward-declared here; defined once, per active backend (e.g.
	 *          @c OpenGL/a_opengl_storage_buffer.cpp), selected at CMake configure time via
	 *          @c ANDROMEDA_GRAPHICS_API. Same rationale as @c ConstantBufferImpl / @c FramebufferImpl:
	 *          exactly one implementation is ever compiled in, so @c RHIStorageBuffer needs neither
	 *          @c virtual nor to expose backend headers to its consumers.
	 */
	class StorageBufferImpl;

	/**
	 * @class RHIStorageBuffer
	 * @brief GPU-visible, read/write shader storage buffer (SSBO), e.g. for GPU compute particle data.
	 *
	 * @details Move-only RAII wrapper. All methods forward to @c StorageBufferImpl, which is
	 *          entirely defined inside the selected backend's .cpp file (compilation firewall).
	 *          Used for data shared between compute and render passes (e.g. particles).
	 */
	class RHIStorageBuffer {
	public:
		RHIStorageBuffer();
		~RHIStorageBuffer();

		RHIStorageBuffer(const RHIStorageBuffer&) = delete;
		RHIStorageBuffer& operator=(const RHIStorageBuffer&) = delete;
		RHIStorageBuffer(RHIStorageBuffer&& other) noexcept;
		RHIStorageBuffer& operator=(RHIStorageBuffer&& other) noexcept;

		/**
		 * @brief Binds the buffer to an SSBO binding point so shaders can access it.
		 * @param bindingPoint The binding index referenced by the shader's layout(std430, binding = N).
		 */
		void bind(u32 bindingPoint);

		/**
		 * @brief Updates a sub-range of the buffer with new data.
		 * @param data Pointer to the source bytes.
		 * @param size Number of bytes to write.
		 * @param offset Byte offset into the buffer where writing begins.
		 */
		void setData(const void* data, u32 size, u32 offset);

		/**
		 * @brief Allocates the buffer storage with a usage hint and optional initial data.
		 * @param usage Access/update-frequency hint.
		 * @param size Total buffer size in bytes.
		 * @param initialData Optional initial contents (nullptr leaves the storage uninitialized).
		 */
		void create(BufferUsage usage, u32 size, const void* initialData = nullptr);
		/**
		 * @brief Releases the buffer's GPU resources.
		 */
		void clear();
		/** @brief Returns the buffer's allocated size in bytes. */
		u32 getSize() const;

	private:
		std::unique_ptr<StorageBufferImpl> m_Impl; ///< Backend-specific implementation (Pimpl).
	};
}
