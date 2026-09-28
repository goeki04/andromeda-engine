/**
 * @file a_opengl_storage_buffer.cpp
 * @brief OpenGL implementation of RHIStorageBuffer (SSBO), e.g. for GPU compute particle data.
 * @details Compiled in only when ANDROMEDA_GRAPHICS_API=OpenGL (see modules/renderer_rhi/CMakeLists.txt).
 *          Defines both @c StorageBufferImpl and the @c RHIStorageBuffer methods that forward to it,
 *          so nothing outside this file ever needs to know OpenGL is involved.
 */

#include "a_rhi_storage_buffer.hpp"
#include "a_Primitives.hpp"
#include "a_logger.hpp"
#include <GL/glew.h>

namespace Andromeda {

	/**
	 * @class StorageBufferImpl
	 * @brief OpenGL-backed shader storage buffer (SSBO), bound to @c GL_SHADER_STORAGE_BUFFER.
	 */
	class StorageBufferImpl {
	public:
		GLuint m_BufferID = 0; ///< OpenGL buffer object ID.
		u32 m_Size = 0;        ///< Allocated size in bytes.

		/** @brief Deletes the underlying OpenGL buffer object. */
		~StorageBufferImpl() {
			if (m_BufferID != 0) {
				glDeleteBuffers(1, &m_BufferID);
			}
		}

		/** @copydoc RHIStorageBuffer::bind */
		void bind(u32 bindingPoint) {
			if (m_BufferID == 0) {
				A_WARN("Attempted to bind an uninitialized RHIStorageBuffer");
				return;
			}
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, bindingPoint, m_BufferID);
		}

		/** @copydoc RHIStorageBuffer::setData */
		void setData(const void* data, u32 size, u32 offset) {
			if (m_BufferID == 0) {
				A_WARN("Attempted to set data on an uninitialized RHIStorageBuffer");
				return;
			}
			glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_BufferID);
			glBufferSubData(GL_SHADER_STORAGE_BUFFER, offset, size, data);
			glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
		}

		/** @copydoc RHIStorageBuffer::create */
		void create(BufferUsage usage, u32 size, const void* initialData) {
			if (m_BufferID != 0) {
				glDeleteBuffers(1, &m_BufferID);
				m_BufferID = 0;
			}
			GLenum glUsage = BufferUsageToGL(usage);
			glGenBuffers(1, &m_BufferID);
			glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_BufferID);
			glBufferData(GL_SHADER_STORAGE_BUFFER, size, initialData, glUsage);
			glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
			m_Size = size;
		}

		void clear() {
			GLuint zero = 0;

			glClearNamedBufferSubData(
				m_BufferID,
				GL_R32UI,
				0,
				m_Size,
				GL_RED_INTEGER,
				GL_UNSIGNED_INT,
				&zero
			);
		}

		/** @copydoc RHIStorageBuffer::getSize */
		u32 getSize() const { return m_Size; }
	};

	RHIStorageBuffer::RHIStorageBuffer() : m_Impl(std::make_unique<StorageBufferImpl>()) {}
	RHIStorageBuffer::~RHIStorageBuffer() = default;

	RHIStorageBuffer::RHIStorageBuffer(RHIStorageBuffer&& other) noexcept = default;
	RHIStorageBuffer& RHIStorageBuffer::operator=(RHIStorageBuffer&& other) noexcept = default;

	void RHIStorageBuffer::bind(u32 bindingPoint) { m_Impl->bind(bindingPoint); }
	void RHIStorageBuffer::setData(const void* data, u32 size, u32 offset) { 
		if (size <= 0) {
            A_WARN("Attempted to set data on a RHIStorageBuffer with size 0");
            return;
		}
		m_Impl->setData(data, size, offset); 
	}
	void RHIStorageBuffer::clear() {
		m_Impl->clear();
	}
	void RHIStorageBuffer::create(BufferUsage usage, u32 size, const void* initialData) { 
		if (size <= 0) {
			A_WARN("Attempted to create a RHIStorageBuffer with size 0");
			return;
		}
		m_Impl->create(usage, size, initialData); 
	}
	u32 RHIStorageBuffer::getSize() const { return m_Impl->getSize(); }
}
