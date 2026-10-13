#pragma once

/**
 * @file a_CubemapData.hpp
 * @brief CPU-side container for the six faces of a cubemap texture (e.g. a skybox or HDR environment map).
 */

#include "a_primitives.hpp"
#include <vector>
#include <array>
extern "C" {
    /** @brief Frees pixel buffers allocated by stb_image (declared here to avoid pulling in the full stb_image.h header). */
    void stbi_image_free(void* retval_from_stbi_load);
}
namespace Andromeda{
    /**
     * @brief Owns the raw, decoded pixel data of a six-faced cubemap on the CPU, before upload to the GPU.
     *
     * @details Pixel buffers are allocated by stb_image when loading the source images from disk and
     *          must be released via @c stbi_image_free(), which is why this type is move-only: copying
     *          would require either deep-copying the raw buffers or risking a double free.
     */
    struct CubemapData{
        /** @brief Raw decoded pixel buffers for the six cubemap faces (+X, -X, +Y, -Y, +Z, -Z), owned by this instance. */
        std::array<void*, 6> pixelData = {
            nullptr
        };
        std::vector<std::string> texturePath; ///< Source file paths of the six face images, in the same order as @c pixelData.
        u32 textureID = 0; ///< GPU texture handle once uploaded (0 if not yet uploaded).
        i32 width = 0;    ///< Width in pixels of each cubemap face (faces are assumed to be square and equally sized).
        i32 height = 0;   ///< Height in pixels of each cubemap face.
        i32 channels = 0; ///< Number of color channels per pixel (e.g. 3 for RGB, 4 for RGBA).

        bool isHDR = false; ///< True if the source images were loaded as high-dynamic-range (float) data rather than 8-bit.

        /** @brief Constructs an empty, unloaded cubemap. */
        CubemapData() = default;
        CubemapData(const CubemapData&) = delete;
        CubemapData& operator=(const CubemapData&) = delete;

        /** @brief Move-constructs from another instance, taking ownership of its pixel buffers. */
        CubemapData(CubemapData&& other) noexcept :
            pixelData(other.pixelData),
            texturePath(other.texturePath),
            textureID(other.textureID),
            width(other.width), height(other.height), channels(other.channels) {
            other.pixelData.fill(nullptr);
        }

        /** @brief Move-assigns from another instance, freeing this instance's existing buffers first. */
        CubemapData& operator=(CubemapData&& other) noexcept {
            if (this != &other) {
                freePixelData();
                pixelData = other.pixelData;
                texturePath = other.texturePath;
                width = other.width;
                textureID = other.textureID;
                height = other.height;
                channels = other.channels;
                other.pixelData.fill(nullptr);
            }
            return *this;
        }

        /**
         * @brief Releases all owned pixel buffers via @c stbi_image_free() and clears the array.
         * @note Safe to call multiple times; null entries are skipped.
         */
        void freePixelData() {
            for (auto*& ptr : pixelData) {
                if (ptr) stbi_image_free(ptr);
            }
        }

        /** @brief Destroys the instance, freeing any owned pixel buffers. */
        ~CubemapData() {
            freePixelData();
        }
    };
}