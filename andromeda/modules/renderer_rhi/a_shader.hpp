#pragma once

/**
 * @file a_shader.hpp
 * @brief RAII wrapper around a shader program handle created through the graphics context.
 */

#include <string>
#include "a_primitives.hpp"
#include "a_IGraphicsContext.hpp"
namespace Andromeda::RendererRHI {

	/**
	 * @class Shader
	 * @brief Owns a compiled shader program and releases it through the graphics context on destruction.
	 *
	 * @details Stores the source paths and the API-agnostic @c ShaderProgramHandle. Copy is disabled
	 *          to enforce single ownership of the GPU program (the destructor frees it).
	 */
	class Shader
	{
    public:
        ShaderProgramHandle m_Handle;   ///< Handle to the underlying GPU shader program.
        std::string m_VertexPath;       ///< Path to the vertex shader source.
        std::string m_FragmentPath;     ///< Path to the fragment shader source.

        /**
         * @brief Constructs the wrapper from shader source paths and a graphics context.
         * @param vertexPath Path to the vertex shader source.
         * @param fragmentPath Path to the fragment shader source.
         * @param context The graphics context used to create and later destroy the program.
         */
        Shader(const char* vertexPath, const char* fragmentPath, IGraphicsContext* context)
            : m_VertexPath(vertexPath), m_FragmentPath(fragmentPath), m_Context(context) {
        }

        /** @brief Destroys the owned shader program via the graphics context, if one was created. */
        ~Shader() {
            if (m_Handle.apiID != 0 && m_Context) {
                m_Context->destroyShaderProgram(m_Handle);
            }
        }

        Shader(const Shader&) = delete;
        Shader& operator=(const Shader&) = delete;

    private:
        IGraphicsContext* m_Context = nullptr; ///< Non-owning context used to free the program on destruction.
    };
}