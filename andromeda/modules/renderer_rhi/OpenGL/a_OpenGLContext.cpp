#include "a_OpenGLContext.hpp"
#include "a_OpenGLContext.hpp"
#include "a_OpenGLContext.hpp"
#include "a_OpenGLContext.hpp"
#include "a_Primitives.hpp"
#include "a_logger.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include "a_rhi_constant_buffer.hpp"
#include "a_rhi_framebuffer.hpp"
#include "a_cubemapData.hpp"
#include "a_samplerState.hpp"
#include "a_opengl_texture.hpp"
#include "a_opengl_sampler.hpp"
namespace Andromeda {

    namespace {
        /**
         * @brief Uploads a single raw uniform value to a fixed GL location, dispatched by @c ShaderDataType.
         * @tparam T The uniform's data type; selects which `glUniform*`/`glUniformMatrix*` call to issue.
         * @param loc The GL uniform location to upload to.
         * @param ptr Pointer to the raw source bytes, reinterpreted according to @p T.
         */
        template<Andromeda::ShaderDataType T>
        inline void uploadUniform(GLint loc, const void* ptr) {
            using namespace Andromeda;
            if constexpr (T == ShaderDataType::Float) {
                glUniform1f(loc, *static_cast<const float*>(ptr));
            }
            else if constexpr (T == ShaderDataType::Int) {
                glUniform1i(loc, *static_cast<const int32_t*>(ptr));
            }
            else if constexpr (T == ShaderDataType::Vec2) {
                glUniform2fv(loc, 1, static_cast<const float*>(ptr));
            }
            else if constexpr (T == ShaderDataType::Vec3) {
                glUniform3fv(loc, 1, static_cast<const float*>(ptr));
            }
            else if constexpr (T == ShaderDataType::Vec4) {
                glUniform4fv(loc, 1, static_cast<const float*>(ptr));
            }
            else if constexpr (T == ShaderDataType::Mat4) {
                glUniformMatrix4fv(loc, 1, GL_FALSE, static_cast<const float*>(ptr));
            }
        }

        /**
         * @brief Checks a shader's compile status or a program's link status and throws with the info log on failure.
         * @param objectID The GL shader or program object to check.
         * @param stage Human-readable label used in the error message/log (e.g. "Vertex Shader").
         * @param statusType Which status to check: @c GL_COMPILE_STATUS for a shader, anything else for a program's link status.
         * @throws std::runtime_error If compilation/linking failed; the exception message contains the GL info log.
         */
        void throwShaderLog(GLuint objectID, const char* stage, GLenum statusType) {
            GLint success = 0;
            if (statusType == GL_COMPILE_STATUS) {
                glGetShaderiv(objectID, GL_COMPILE_STATUS, &success);
            }
            else {
                glGetProgramiv(objectID, GL_LINK_STATUS, &success);
            }

            if (success) [[likely]] return;

            GLint len = 0;
            if (statusType == GL_COMPILE_STATUS) {
                glGetShaderiv(objectID, GL_INFO_LOG_LENGTH, &len);
            }
            else {
                glGetProgramiv(objectID, GL_INFO_LOG_LENGTH, &len);
            }

            std::string log;
            log.resize((len > 1) ? len : 1);

            GLsizei outLen = 0;
            if (statusType == GL_COMPILE_STATUS) {
                glGetShaderInfoLog(objectID, (GLsizei)log.size(), &outLen, log.data());
            }
            else {
                glGetProgramInfoLog(objectID, (GLsizei)log.size(), &outLen, log.data());
            }
            log.resize(outLen);
            A_ERROR("{} error:\n{}", stage, log);
            throw std::runtime_error(std::string(stage) + " error:\n" + log);
        }

        /**
         * @brief Compiles a single shader stage from source and validates the result.
         * @param stageType The GL shader stage to create (e.g. @c GL_VERTEX_SHADER, @c GL_COMPUTE_SHADER).
         * @param source The GLSL source code for this stage.
         * @param stageName Human-readable label used in error messages/logs (e.g. "Vertex Shader").
         * @return The compiled GL shader object id.
         * @throws std::runtime_error If compilation failed (via @c throwShaderLog).
         */
        GLuint compileStage(GLenum stageType, const std::string& source, const char* stageName) {
            GLuint shader = glCreateShader(stageType);
            const char* srcPtr = source.c_str();
            glShaderSource(shader, 1, &srcPtr, nullptr);
            glCompileShader(shader);
            throwShaderLog(shader, stageName, GL_COMPILE_STATUS);
            return shader;
        }

        GLuint compileSpirvStage(GLenum stageType, const std::string& spirvCode, const char* entryPoint, const char* stageName) {
			GLuint shader = glCreateShader(stageType);
            glShaderBinary(1,&shader, GL_SHADER_BINARY_FORMAT_SPIR_V, spirvCode.data(), spirvCode.size());
            glSpecializeShader(shader, entryPoint, 0, nullptr, nullptr);
            throwShaderLog(shader, stageName, GL_COMPILE_STATUS);
            return shader;
        }
    }

    ShaderProgramHandle OpenGLContext::createShaderProgram(const std::string& vertSrc, const std::string& fragSrc) {
        ShaderProgramHandle newHandle = {};
        newHandle.apiID = compileOpenGLShader(vertSrc, fragSrc);
        return newHandle;
    }
    ShaderProgramHandle OpenGLContext::createComputeProgram(const std::string& computeSrc)
    {
		ShaderProgramHandle newHandle = {};
        newHandle.apiID = compileOpenGLComputeShader(computeSrc);
        return newHandle;
    }

    ShaderProgramHandle OpenGLContext::createComputeProgram(const std::string& computeSrc, const char* entryPoint, const char* stageName)
    {
        ShaderProgramHandle newHandle = {};
        newHandle.apiID = compileOpenGLSpirvComputeShader(computeSrc,entryPoint,stageName);
        return newHandle;
    }

    void OpenGLContext::destroyShaderProgram(ShaderProgramHandle handle) {
        if (handle.apiID != 0) {
            glDeleteProgram(handle.apiID);
        }
    }

    void OpenGLContext::unbindTexture()
    {
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    void OpenGLContext::bindShaderProgram(ShaderProgramHandle handle) {
        glUseProgram(handle.apiID);
    }

    std::string OpenGLContext::readShaderSource(const char* shaderPath)
    {
        std::ifstream fileStream(shaderPath);
        if (!fileStream.is_open()) {
            A_CRITICAL("Could not open shader file at path: {}", shaderPath);
            throw std::runtime_error("Failed to open shader file: " + std::string(shaderPath));
        }
        std::stringstream buffer;
        buffer << fileStream.rdbuf();
        std::string shaderSource = buffer.str();
        return shaderSource;
    }

    std::string OpenGLContext::readShaderBinary(const char* shaderPath)
    {
        std::ifstream fileStream(shaderPath, std::ios::binary);
        if (!fileStream.is_open()) {
            A_CRITICAL("Could not open shader file at path: {}", shaderPath);
            throw std::runtime_error("Failed to open shader file: " + std::string(shaderPath));
        }
        std::stringstream buffer;
        buffer << fileStream.rdbuf();
        std::string shaderSource = buffer.str();
        return shaderSource;
    }

    u32 OpenGLContext::compileOpenGLComputeShader(const std::string& computeSrc) {
        std::string  computeSource = readShaderSource(computeSrc.c_str());

		GLuint computeShader = compileStage(GL_COMPUTE_SHADER, computeSource, "Compute Shader");
        
        GLuint program = glCreateProgram();
		glAttachShader(program, computeShader);
        glLinkProgram(program);

        throwShaderLog(program, "Compute Shader Program Linking", GL_LINK_STATUS);
        glDeleteShader(computeShader);
        return program;
    }

    u32 OpenGLContext::compileOpenGLSpirvComputeShader(const std::string& computeSrc, const char* entryPoint, const char* stageName) {
		std::string  spirvSrc = readShaderBinary(computeSrc.c_str());

		GLuint computeShader = compileSpirvStage(GL_COMPUTE_SHADER, spirvSrc, entryPoint, stageName);
		GLuint program = glCreateProgram();
		glAttachShader(program, computeShader);
		glLinkProgram(program);

		throwShaderLog(program, "Compute Shader Program Linking", GL_LINK_STATUS);
		glDeleteShader(computeShader);
		return program;
    }

    void OpenGLContext::dispatchCompute(ShaderProgramHandle handle, u32 groupCountX, u32 groupCountY, u32 groupCountZ)
    {
		bindShaderProgram(handle);
		glDispatchCompute(groupCountX, groupCountY, groupCountZ);
        glMemoryBarrier(GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT);
    }

    u32 OpenGLContext::compileOpenGLShader(const std::string& vertSrc, const std::string& fragSrc)
    {
        std::string vertexSource = readShaderSource(vertSrc.c_str());
        std::string fragmentSource = readShaderSource(fragSrc.c_str());

        GLuint vertexShader = compileStage(GL_VERTEX_SHADER, vertexSource, "Vertex Shader");
        GLuint fragmentShader = compileStage(GL_FRAGMENT_SHADER, fragmentSource, "Fragment Shader");

        GLuint program = glCreateProgram();
        glAttachShader(program, vertexShader);
        glAttachShader(program, fragmentShader);
        glLinkProgram(program);

        throwShaderLog(program, "Shader Program Linking", GL_LINK_STATUS);
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);

        return program;
    }
    void OpenGLContext::submitUniforms(std::span<const UniformData> uniforms)
    {
        for (const auto& uniform : uniforms) {
            if (uniform.location == -1) [[unlikely]] continue;

            switch (uniform.type) {
            case ShaderDataType::Float: uploadUniform<ShaderDataType::Float>(uniform.location, uniform.dataPtr); break;
            case ShaderDataType::Int:   uploadUniform<ShaderDataType::Int>(uniform.location, uniform.dataPtr);   break;
            case ShaderDataType::Vec2:  uploadUniform<ShaderDataType::Vec2>(uniform.location, uniform.dataPtr);  break;
            case ShaderDataType::Vec3:  uploadUniform<ShaderDataType::Vec3>(uniform.location, uniform.dataPtr);  break;
            case ShaderDataType::Vec4:  uploadUniform<ShaderDataType::Vec4>(uniform.location, uniform.dataPtr);  break;
            case ShaderDataType::Mat4:  uploadUniform<ShaderDataType::Mat4>(uniform.location, uniform.dataPtr);  break;
            }
        }
    }

    void OpenGLContext::bindTextures(std::span<const TextureBinding> textures)
    {
        for (const auto& tex : textures) {
            glBindTextureUnit(tex.slot, tex.apiID);
        }
    }

    std::vector<ReflectedUniform> OpenGLContext::getProgramUniforms(ShaderProgramHandle handle)
    {
        std::vector<ReflectedUniform> result;

        GLint numUniforms = 0;
        glGetProgramiv(handle.apiID, GL_ACTIVE_UNIFORMS, &numUniforms);

        GLint maxNameLength = 0;
        glGetProgramiv(handle.apiID, GL_ACTIVE_UNIFORM_MAX_LENGTH, &maxNameLength);
        std::vector<GLchar> nameBuffer(maxNameLength);

        for (GLint i = 0; i < numUniforms; ++i) {
            GLsizei length = 0;
            GLint size = 0;
            GLenum type = 0;

            glGetActiveUniform(handle.apiID, i, maxNameLength, &length, &size, &type, nameBuffer.data());
            std::string name(nameBuffer.data(), length);

            GLint location = glGetUniformLocation(handle.apiID, name.c_str());
            if (location == -1) continue;

            ShaderDataType sType;
            switch (type) {
            case GL_FLOAT:        sType = ShaderDataType::Float; break;
            case GL_INT:          sType = ShaderDataType::Int; break;
            case GL_FLOAT_VEC2:   sType = ShaderDataType::Vec2; break;
            case GL_FLOAT_VEC3:   sType = ShaderDataType::Vec3; break;
            case GL_FLOAT_VEC4:   sType = ShaderDataType::Vec4; break;
            case GL_FLOAT_MAT4:   sType = ShaderDataType::Mat4; break;
            case GL_SAMPLER_2D:   sType = ShaderDataType::Int; break;
            default: continue;
            }

            result.push_back({ name, static_cast<u32>(location), sType });
        }

        return result;
    }
    void OpenGLContext::setRenderPassSpecs(const RenderPassSpecs& specs)
    {
        if (m_IsFirstContextInit || specs.rasterizerMode != m_CurrentSpecs.rasterizerMode) {
            glPolygonMode(GL_FRONT_AND_BACK, specs.rasterizerMode == RasterizerMode::Wireframe ? GL_LINE : GL_FILL);
            m_CurrentSpecs.rasterizerMode = specs.rasterizerMode;
        }

        if (m_IsFirstContextInit || specs.cullMode != m_CurrentSpecs.cullMode) {
            if (specs.cullMode == CullMode::None) {
                glDisable(GL_CULL_FACE);
            }
            else {
                glEnable(GL_CULL_FACE);
                glCullFace(specs.cullMode == CullMode::Back ? GL_BACK : GL_FRONT);
            }
            m_CurrentSpecs.cullMode = specs.cullMode;
        }

        if (m_IsFirstContextInit || specs.depthTest != m_CurrentSpecs.depthTest) {
            if (specs.depthTest) {
                glEnable(GL_DEPTH_TEST);
            }
            else {
                glDisable(GL_DEPTH_TEST);
            }
            m_CurrentSpecs.depthTest = specs.depthTest;
        }

        if (specs.depthTest && (m_IsFirstContextInit || specs.depthFunction != m_CurrentSpecs.depthFunction)) {
            switch (specs.depthFunction) {
            case DepthFunc::Less:   glDepthFunc(GL_LESS);   break;
            case DepthFunc::Equal:  glDepthFunc(GL_EQUAL);  break;
            case DepthFunc::LEqual: glDepthFunc(GL_LEQUAL); break;
            }
            m_CurrentSpecs.depthFunction = specs.depthFunction;
        }

        if (m_IsFirstContextInit || specs.blendMode != m_CurrentSpecs.blendMode) {
            if (specs.blendMode == BlendMode::AlphaBlend) {
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            }
            else {
                glDisable(GL_BLEND);
            }
            m_CurrentSpecs.blendMode = specs.blendMode;
        }

        if (m_IsFirstContextInit || specs.depthWrite != m_CurrentSpecs.depthWrite) {
            glDepthMask(specs.depthWrite ? GL_TRUE : GL_FALSE);
            m_CurrentSpecs.depthWrite = specs.depthWrite;
        }

        m_IsFirstContextInit = false;
    }
    void OpenGLContext::drawIndexed(u32 vao, u32 indexCount, u32 indexOffset)
    {
        glBindVertexArray(vao);
        // The offset is a byte offset into the bound element buffer, not an index count.
        const auto byteOffset = static_cast<GLintptr>(indexOffset) * static_cast<GLintptr>(sizeof(u32));
        glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(indexCount), GL_UNSIGNED_INT,
                       reinterpret_cast<const void*>(byteOffset));
        glBindVertexArray(0);
    }

    void OpenGLContext::bindFramebuffer(std::shared_ptr<RHIFramebuffer> framebuffer)
    {
        if (!framebuffer) {
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            return;
        }

        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer->getFramebufferID());

        const auto& specs = framebuffer->getSpecification();
        glViewport(0, 0, static_cast<GLsizei>(specs.width), static_cast<GLsizei>(specs.height));
    }

    void OpenGLContext::unbindFramebuffer()
    {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    std::shared_ptr<RHIFramebuffer> OpenGLContext::createFramebuffer(const FramebufferSpecification& specs)
    {
        return std::make_shared<RHIFramebuffer>(specs);
    }

    void OpenGLContext::deleteVertexArrays(u32 vao)
    {
        if (vao != 0) {
            glDeleteVertexArrays(1, &vao);
        }
    }

    i32 OpenGLContext::getUniformLocation(ShaderProgramHandle shader, const std::string& name) {
        return glGetUniformLocation(shader.apiID, name.c_str());
    }

    void OpenGLContext::bindShaderTexture(u32 slot,const Texture& tex) {
        glActiveTexture(GL_TEXTURE0 + slot);
        glBindTexture(GL_TEXTURE_2D, tex.textureID);
    }

    void OpenGLContext::bindToTarget(const Texture& tex) {
        glBindTexture(GL_TEXTURE_2D, tex.textureID);
    }

    void OpenGLContext::attachCubemapFace(u32 faceIndex, u32 cubemapTexID) {
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + faceIndex, cubemapTexID, 0);
    }

    void OpenGLContext::blitFramebuffer(std::shared_ptr<RHIFramebuffer> source, std::shared_ptr<RHIFramebuffer> target, bool copyDepth)
    {
        assert(source && "RHI Error: Source Framebuffer for blit is null!");

        const auto& srcSpecs = source->getSpecification();

        u32 targetID = 0;
        u32 targetWidth = srcSpecs.width;
        u32 targetHeight = srcSpecs.height;

        if (target) {
            targetID = target->getFramebufferID();
            const auto& dstSpecs = target->getSpecification();
            targetWidth = dstSpecs.width;
            targetHeight = dstSpecs.height;
        }

        glBindFramebuffer(GL_READ_FRAMEBUFFER, source->getFramebufferID());
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, targetID);

        GLbitfield mask = GL_COLOR_BUFFER_BIT;
        GLenum filter = GL_NEAREST;

        if (copyDepth) {
            mask |= GL_DEPTH_BUFFER_BIT;
            filter = GL_NEAREST;
        }

        glBlitFramebuffer(
            0, 0, srcSpecs.width, srcSpecs.height,
            0, 0, targetWidth, targetHeight,
            mask, filter
        );
        glBindFramebuffer(GL_FRAMEBUFFER, targetID);
    }

    u32 OpenGLContext::createEmptyVAO()
    {
        u32 vao = 0;
        glGenVertexArrays(1, &vao);
        return vao;
    }

    void OpenGLContext::clear(ClearFlags flags, const vec4& color)
    {
        if ((static_cast<uint8_t>(flags) & static_cast<uint8_t>(ClearFlags::Color))) {
            glClearColor(color.r, color.g, color.b, color.a);
        }

        GLbitfield glFlags = 0;
        if ((static_cast<uint8_t>(flags) & static_cast<uint8_t>(ClearFlags::Color)))   glFlags |= GL_COLOR_BUFFER_BIT;
        if ((static_cast<uint8_t>(flags) & static_cast<uint8_t>(ClearFlags::Depth)))   glFlags |= GL_DEPTH_BUFFER_BIT;
        if ((static_cast<uint8_t>(flags) & static_cast<uint8_t>(ClearFlags::Stencil))) glFlags |= GL_STENCIL_BUFFER_BIT;

        if (glFlags != 0) {
            glClear(glFlags);
        }
    }

    void OpenGLContext::clear(ClearFlags flags)
    {
        GLbitfield glFlags = 0;
        if ((flags & ClearFlags::Color) != ClearFlags::None)   glFlags |= GL_COLOR_BUFFER_BIT;
        if ((flags & ClearFlags::Depth) != ClearFlags::None)   glFlags |= GL_DEPTH_BUFFER_BIT;
        if ((flags & ClearFlags::Stencil) != ClearFlags::None) glFlags |= GL_STENCIL_BUFFER_BIT;

        if (glFlags != 0) {
            glClear(glFlags);
        }
    }

    void OpenGLContext::initRenderContext()
    {
        glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
        glEnable(GL_PROGRAM_POINT_SIZE);
    }

    std::shared_ptr<RHIConstantBuffer> OpenGLContext::createConstantBuffer(u32 size)
    {
        auto buffer = std::make_shared<RHIConstantBuffer>();
        buffer->initialize(size);
        return buffer;
    }

    void OpenGLContext::setViewport(i32 vpPosX, i32 vpPosY, u32 vpWidth, u32 vpHeight)
    {
        glViewport(static_cast<GLint>(vpPosX), static_cast<GLint>(vpPosY), 
                   static_cast<GLsizei>(vpWidth), static_cast<GLsizei>(vpHeight));
    }

    void OpenGLContext::drawArrays(u32 vao, u32 vertexCount)
    {
        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLES, 0, vertexCount);
        glBindVertexArray(0);
    }

    void OpenGLContext::setParameter(ShaderProgramHandle shader, const std::string& name, const mat4& matrix) {
        i32 location = glGetUniformLocation(shader.apiID, name.c_str());
        if (location != -1) {
            glUniformMatrix4fv(location, 1, GL_FALSE, &matrix[0][0]);
        }
    }

    void OpenGLContext::setParameter(ShaderProgramHandle shader, const std::string& name, const vec3& vector) {
        i32 location = glGetUniformLocation(shader.apiID, name.c_str());
        if (location != -1) {
            glUniform3fv(location, 1, &vector[0]);
        }
    }

    void OpenGLContext::setParameter(ShaderProgramHandle shader, const std::string& name, i32 value) {
        i32 location = glGetUniformLocation(shader.apiID, name.c_str());
        if (location != -1) {
            glUniform1i(location, value);
        }
    }

    void OpenGLContext::bindSamplerState(u32 textureID, const SamplerState& state)
    {
       GLenum target = GLSampler::GetTarget(state.type);
       glBindTexture(target, textureID);
       GLSampler sampler(state);
       sampler.Apply();
       glBindTexture(target, 0);
    }

    void OpenGLContext::bindTextureCube(u32 slot,const CubemapData& tex) {
        glActiveTexture(GL_TEXTURE0 + slot);
        glBindTexture(GL_TEXTURE_CUBE_MAP, tex.textureID);
    }

    void OpenGLContext::framebufferTexture2D(u32 faceIndex,const CubemapData& tex, u32 mip)
    {
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + faceIndex, tex.textureID, mip);
    }

    void OpenGLContext::framebufferTexture2D(const Texture& tex, u32 mip)
    {
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex.textureID, mip);
    }

    void OpenGLContext::allocateTexture(Texture& texture)
    {
        glGenTextures(1, &texture.textureID);
        SamplerState& sampler = texture.sampler;
        GLenum target = GLSampler::textureTypeToGLTextureTarget(sampler.type);
        glBindTexture(target, texture.textureID);

        GLTextureFormat format = GLtexture::ConvertFormat(texture.internalFormat);
        glTexImage2D(target, 0, format.internalFormat,
            static_cast<GLsizei>(texture.width), static_cast<GLsizei>(texture.height),
            0, format.dataFormat, format.dataType, nullptr);

        glBindTexture(target, 0);
    }

    Texture OpenGLContext::generateTexture(u32 width, u32 height,SamplerState& sampler, TextureFormat format)
    {
        Texture texture;
        texture.width = width;
        texture.height = height;
        texture.sampler = sampler;
        texture.internalFormat = format;

        return texture;
    }

    void OpenGLContext::generateMipmap(TextureType type)
    {
        GLuint target = GLSampler::textureTypeToGLTextureTarget(type);
        glGenerateMipmap(target);
    }
    GLenum OpenGLContext::translateDrawModeToOpenGL(DrawMode mode)
    {
        switch (mode)
        {
        case DrawMode::Points:
            return GL_POINTS;
        case DrawMode::Lines:
            return GL_LINES;
        case DrawMode::Triangles:
            return GL_TRIANGLES;
        default:
            A_WARN("Unknown DrawMode. Falling back to GL_TRIANGLES.");
            return GL_TRIANGLES;
        }
    }
    void OpenGLContext::drawInstanced(DrawMode mode, u32 vertexCount, u32 instanceCount, u32 firstVertex)
    {
        // Core profile requires a bound VAO for any draw call, even attributeless ones. Lazily
        // create a single empty VAO the first time it's needed and reuse it for every subsequent
        // attributeless instanced draw, instead of leaking the VAO concept into the caller/interface.
        if (m_AttributelessVAO == 0) {
            m_AttributelessVAO = createEmptyVAO();
        }
        glBindVertexArray(m_AttributelessVAO);

        GLenum glMode = translateDrawModeToOpenGL(mode);
        glDrawArraysInstanced(glMode, static_cast<GLint>(firstVertex), static_cast<GLsizei>(vertexCount), static_cast<GLsizei>(instanceCount));

        glBindVertexArray(0);
    }
}