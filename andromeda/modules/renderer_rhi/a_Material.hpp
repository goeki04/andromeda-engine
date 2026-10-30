#pragma once

/**
 * @file a_Material.hpp
 * @brief Data-oriented material: shader + a flat byte buffer of uniforms + textures, optimized for cache-friendly binding.
 */

#include "a_IGraphicsContext.hpp"
#include <unordered_map>
#include <vector>
#include <cstring>
#include <string>
#include "a_rhi_constant_buffer.hpp"
namespace Andromeda {

    /**
     * @class Material
     * @brief Represents a data-oriented material system within the Andromeda Engine.
     * * This class bundles shader parameters (uniforms) and textures for a renderable object.
     * In contrast to traditional object-oriented approaches, this class stores all
     * shader data (such as floats, vectors, and matrices) contiguously inside a single,
     * flat byte buffer (@ref m_CpuStorage).
     * * @note String lookups via the internal translation table (@ref m_UniformTable) occur
     * exclusively when setting parameters. The performance-critical render path (@ref bind)
     * is completely free of hash maps and string lookups, guaranteeing maximum CPU cache locality.
     */
    class Material {
    public:
        std::any m_UboStructure; ///< Type-erased copy of the material's UBO struct (e.g. for editor reflection/editing).
        /**
         * @struct PropertyLayout
         * @brief Describes the memory metadata of a single shader variable inside the byte buffer.
         */
        struct PropertyLayout {
            u32 location;        /**< The GPU-assigned location ID of the uniform variable. */
            ShaderDataType type; /**< The platform-agnostic data type of the variable. */
            u32 offset;          /**< The starting address (in bytes) within the central storage (@ref m_CpuStorage). */
            u32 size;            /**< The total size of the data in bytes. */
        };

        /** @brief The API-agnostic handle of the assigned shader program. */
        ShaderProgramHandle m_ShaderHandle{};
        std::shared_ptr<RHIConstantBuffer> m_MaterialUBO; ///< Optional uniform buffer holding the material's UBO struct on the GPU.
        u32 m_UboBindingSlot = 0;                       ///< Binding point the material UBO is bound to.
        /**
         * @brief Constructs a new Material and reflects the active shader uniforms from the GPU.
         * * During initialization, the constructor queries the GPU via the graphics context for
         * all active uniform variables (Shader Reflection). These are mapped once into the
         * internal lookup table.
         * * @param shaderHandle The handle of the compiled and linked shader program.
         * @param ctx Pointer to the active graphics context used for shader reflection.
         */
        Material(ShaderProgramHandle shaderHandle, IGraphicsContext* ctx)
            : m_ShaderHandle(shaderHandle)
        {
            auto reflected = ctx->getProgramUniforms(shaderHandle);
            for (const auto& uniform : reflected) {
                m_UniformTable[uniform.name] = { uniform.location, uniform.type };
            }
        }

        /**
         * @brief Assigns the material's uniform-block data, creating and binding the backing UBO on first use.
         * @tparam T The UBO struct type.
         * @param data The uniform block values to upload.
         * @param bindingSlot The binding point to bind the UBO to.
         * @param ctx The graphics context used to allocate the buffer.
         */
        template<typename T>
        void setUBOData(const T& data, u32 bindingSlot, IGraphicsContext* ctx) {
            m_UboBindingSlot = bindingSlot;
            m_UboStructure = data;
            if (!m_MaterialUBO) {
                m_MaterialUBO = ctx->createConstantBuffer(sizeof(T));
            }

            m_MaterialUBO->setData(&data, sizeof(T));
            m_MaterialUBO->bind(bindingSlot);
        }

        /**
         * @brief Updates the contents of an already-created material UBO.
         * @tparam T The UBO struct type.
         * @param data The new uniform block values (no-op if the UBO has not been created yet).
         */
        template<typename T>
        void updateUBOData(const T& data) {
            m_UboStructure = data;
            if (m_MaterialUBO) {
                m_MaterialUBO->setData(&data, sizeof(T));
            }
        }

        /**
         * @brief Sets or updates a shader parameter within the contiguous memory buffer.
         * * The function looks up the GPU location by the variable's name. If the parameter already
         * exists, its raw bytes are directly overwritten at its current offset. If it is a new
         * parameter, the buffer is dynamically resized and the data is appended to the end.
         * * @tparam T The C++ data type of the value (e.g., float, glm::vec3, glm::mat4).
         * @param name The exact name of the uniform variable within the GLSL shader code.
         * @param value The actual data value to be passed to the shader.
         * @param type The corresponding platform-agnostic shader data type.
         * * @note If a variable is declared in the shader but never used inside its main() function,
         * the graphics driver's optimizer will strip it out. In this case, this function will
         * bail out early since the variable won't exist in the reflection table.
         */
        template<typename T>
        void setParameter(const std::string& name, const T& value, ShaderDataType type) {
            auto it = m_UniformTable.find(name);
            if (it == m_UniformTable.end()) [[unlikely]] return;

            u32 loc = it->second.location;
            u32 dataSize = sizeof(T);

            // Case 1: Parameter already exists -> Overwrite raw bytes at the existing offset
            for (auto& prop : m_Properties) {
                if (prop.location == loc) {
                    std::memcpy(m_CpuStorage.data() + prop.offset, &value, dataSize);
                    return;
                }
            }

            // Case 2: Parameter is new -> Append bytes to the end of the flat vector
            u32 currentOffset = static_cast<u32>(m_CpuStorage.size());
            m_CpuStorage.resize(currentOffset + dataSize);
            std::memcpy(m_CpuStorage.data() + currentOffset, &value, dataSize);

            m_Properties.push_back({ loc, type, currentOffset, dataSize });
        }

        /**
         * @brief Adds a texture binding to the material.
         * @param texture The texture asset structure containing the API handle.
         */
        void addTexture(TextureBinding texture) {
            m_Textures.push_back(texture);
        }

        /**
         * @brief Drops all texture bindings.
         * @details Needed before re-applying a material: addTexture appends, so rebuilding one
         *          in place would otherwise leave it with two sets of bindings after the first
         *          reload and three after the second.
         */
        void clearTextures() {
            m_Textures.clear();
        }

        /**
         * @brief Binds the material and uploads all parameters to the GPU in a highly efficient batch.
         * * This method is called every frame inside the render loop. It builds a flat submission
         * queue (@ref UniformData) directly on the CPU stack, containing pointers into the
         * contiguous byte buffer. The data is then submitted to the RHI via a single batch call.
         * * @param ctx Pointer to the active graphics context (RHI) handling the data upload.
         */
        void bind(IGraphicsContext* ctx) const {
            ctx->bindShaderProgram(m_ShaderHandle);

            if (m_MaterialUBO) {
                m_MaterialUBO->bind(m_UboBindingSlot);
            }

            // Stack allocation for the submission queue (Avoids heap allocation overhead in the render loop)
            std::vector<UniformData> submitQueue;
            submitQueue.reserve(m_Properties.size());

            // Sequential, highly cache-friendly loop through the layout metadata
            for (const auto& prop : m_Properties) {
                submitQueue.push_back({
                    prop.location,
                    prop.size,
                    m_CpuStorage.data() + prop.offset, // Direct memory pointer to the raw data bytes
                    prop.type
                    });
            }

            // A single virtual function call for the entire object uniform block payload
            ctx->submitUniforms(submitQueue);
            ctx->bindTextures(m_Textures);
        }

        /** @brief Returns the layout metadata for all active uniform properties. */
        const std::vector<PropertyLayout>& getProperties() const { return m_Properties; }

        /**
         * @brief Reverse-looks-up a uniform's name from its GPU location.
         * @param location The uniform location to resolve.
         * @return The uniform's name, or an empty string if not found.
         */
        const std::string getUniformNameByLocation(u32 location) const {
            for (const auto& [name, meta] : m_UniformTable) {
                if (meta.location == location) return name;
            }
            return "";
        }

        /**
         * @brief Returns a pointer to a property's raw bytes inside the CPU storage buffer.
         * @param prop The property layout describing the offset.
         * @return Pointer into @c m_CpuStorage at the property's offset (e.g. for editing in the inspector).
         */
        void* getPropertyPointer(const PropertyLayout& prop) {
            return m_CpuStorage.data() + prop.offset;
        }

    private:
        /**
         * @struct InternalMetaData
         * @brief Internal helper structure to store mapped reflection metadata.
         */
        struct InternalMetaData {
            u32 location;        /**< The GPU-assigned location of the variable. */
            ShaderDataType type; /**< The data type of the variable. */
        };

        /** @brief Translation table mapping human-readable shader strings to GPU locations. Used only in constructor and setParameter. */
        std::unordered_map<std::string, InternalMetaData> m_UniformTable;

        /** @brief Contiguous memory chunk in RAM holding all uniform bytes sequentially. */
        std::vector<u8> m_CpuStorage;

        /** @brief List of active properties and their offsets inside m_CpuStorage. Optimized for linear cache iterations. */
        std::vector<PropertyLayout> m_Properties;

        /** @brief List of assigned textures bound to this material. */
        std::vector<TextureBinding> m_Textures;
    };
}