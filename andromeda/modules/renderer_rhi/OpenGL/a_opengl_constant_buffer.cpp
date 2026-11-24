/**
 * @file a_opengl_constant_buffer.cpp
 * @brief OpenGL implementation of RHIConstantBuffer (UBO).
 * @details Compiled in only when ANDROMEDA_GRAPHICS_API=OpenGL (see modules/renderer_rhi/CMakeLists.txt).
 *          Defines both @c ConstantBufferImpl and the @c RHIConstantBuffer methods that forward to it,
 *          so nothing outside this file ever needs to know OpenGL is involved.
 */

#include "a_rhi_constant_buffer.hpp"
#include "a_primitives.hpp"
#include <GL/glew.h>

namespace Andromeda {

	/**
	 * @class ConstantBufferImpl
	 * @brief OpenGL-backed uniform buffer object (UBO).
	 */
	class ConstantBufferImpl {
	public:
		GLuint m_Id = 0; ///< OpenGL buffer object id.

		/** @brief Deletes the underlying OpenGL buffer object. */
		~ConstantBufferImpl() {
			if (m_Id != 0) {
				glDeleteBuffers(1, &m_Id);
			}
		}
	};

	RHIConstantBuffer::RHIConstantBuffer() : m_Impl(std::make_unique<ConstantBufferImpl>()) {}
	RHIConstantBuffer::~RHIConstantBuffer() = default;

	RHIConstantBuffer::RHIConstantBuffer(RHIConstantBuffer&& other) noexcept = default;
	RHIConstantBuffer& RHIConstantBuffer::operator=(RHIConstantBuffer&& other) noexcept = default;

	/** @copydoc RHIConstantBuffer::initialize */
	void RHIConstantBuffer::initialize(i32 size) {
		if (m_Impl->m_Id == 0) {
			glGenBuffers(1, &m_Impl->m_Id);
			glBindBuffer(GL_UNIFORM_BUFFER, m_Impl->m_Id);
			glBufferData(GL_UNIFORM_BUFFER, size, nullptr, GL_DYNAMIC_DRAW);
		}
	}

	/** @copydoc RHIConstantBuffer::setData */
	void RHIConstantBuffer::setData(const void* data, i32 size) {
		glBindBuffer(GL_UNIFORM_BUFFER, m_Impl->m_Id);
		glBufferSubData(GL_UNIFORM_BUFFER, 0, size, data);
	}

	/** @copydoc RHIConstantBuffer::bind */
	void RHIConstantBuffer::bind(u32 bindingPoint) {
		glBindBufferBase(GL_UNIFORM_BUFFER, bindingPoint, m_Impl->m_Id);
	}
}
