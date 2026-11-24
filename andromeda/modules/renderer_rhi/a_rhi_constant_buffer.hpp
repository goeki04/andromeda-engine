#pragma once

/**
 * @file a_rhi_constant_buffer.hpp
 * @brief Backend-agnostic constant/uniform buffer (UBO) wrapper, implemented via the Pimpl idiom.
 */

#include "a_primitives.hpp"
#include <memory>
namespace Andromeda {

	/**
	 * @class ConstantBufferImpl
	 * @brief Opaque, backend-specific state of a constant buffer.
	 * @details Deliberately only forward-declared here. Exactly one definition exists in the
	 *          whole program - inside the active backend's .cpp (e.g. @c OpenGL/a_opengl_constant_buffer.cpp),
	 *          chosen at CMake configure time via @c ANDROMEDA_GRAPHICS_API. Because only one
	 *          implementation is ever compiled in, @c RHIConstantBuffer never needs @c virtual /
	 *          a vtable to pick between backends, and this header never has to include GL/Vulkan/
	 *          DirectX headers.
	 */
	class ConstantBufferImpl;

	/**
	 * @class RHIConstantBuffer
	 * @brief GPU constant buffer used to upload uniform block data to shaders.
	 *
	 * @details Move-only RAII wrapper. All methods forward to @c ConstantBufferImpl, which is
	 *          entirely defined inside the selected backend's .cpp file (compilation firewall).
	 */
	class RHIConstantBuffer {
	public:
		RHIConstantBuffer();
		~RHIConstantBuffer();

		RHIConstantBuffer(const RHIConstantBuffer&) = delete;
		RHIConstantBuffer& operator=(const RHIConstantBuffer&) = delete;
		RHIConstantBuffer(RHIConstantBuffer&& other) noexcept;
		RHIConstantBuffer& operator=(RHIConstantBuffer&& other) noexcept;

		/**
		 * @brief Allocates the GPU buffer storage for a given size (no-op if already initialized).
		 * @param size Buffer size in bytes.
		 */
		void initialize(i32 size);

		/**
		 * @brief Uploads (replaces) the buffer's contents.
		 * @param data Pointer to the source data.
		 * @param size Number of bytes to upload.
		 */
		void setData(const void* data, i32 size);

		/**
		 * @brief Binds the buffer to a uniform block binding point so shaders can read it.
		 * @param bindingPoint The binding index referenced by the shader's layout(binding = N).
		 */
		void bind(u32 bindingPoint);

	private:
		std::unique_ptr<ConstantBufferImpl> m_Impl; ///< Backend-specific implementation (Pimpl).
	};
}
