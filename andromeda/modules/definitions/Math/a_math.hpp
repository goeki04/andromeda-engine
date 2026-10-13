#pragma once

/**
 * @file a_math.hpp
 * @brief Math primitives and camera logic for the Andromeda engine.
 */

#include "a_primitives.hpp"
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/string_cast.hpp>
#include <type_traits>
#include <nlohmann/json.hpp>

 /**
  * @namespace Andromeda::amath
  * @brief Andromeda specific math structures and wrappers.
  */
    namespace Andromeda::amath {
    /**
     * @struct Ray
     * @brief Represents a mathematical ray in 3D space.
     */
    struct Ray {
        vec3 origin;    /**< Start point of the ray. */
        vec3 direction; /**< Direction vector of the ray. */
    };

    /**
     * @struct CameraData
     * @brief Container for camera state, projection, and transformation data.
     */
    struct CameraData {
        vec3 up = vec3(0.0f, 1.0f, 0.0f);            /**< Up vector for view matrix calculation. */
        vec3 cameraPos = vec3(0.0f, 5.0f, 0.0f);     /**< Position of the camera in world space. */
        vec3 cameraTarget = vec3(0.0f);              /**< Point the camera is looking at. */

        float fov = 45.0f;                                /**< Current vertical Field of View. */
        float fovMin = 10.0f, fovMax = 50.0f;             /**< Clamping range for FOV zoom. */

        mat4 projection = { 1.0f };                       /**< Projection matrix. */
        bool canRotate = false;                           /**< Flag for input rotation state. */

        float lastMouseY = 0.0f;                          /**< Previous Y mouse coordinate for delta calculation. */
        float lastMouseX = 0.0f;                          /**< Previous X mouse coordinate for delta calculation. */

        float yaw = -90.0f;                               /**< Horizontal rotation angle in degrees. */
        float pitch = 0.0f;                               /**< Vertical rotation angle in degrees. */
        float sensitivity = 0.1f;                         /**< Mouse sensitivity factor. */

        vec3 target = vec3(0.0f);                         /**< Interpolated or specific look-at target. */
        vec3 forward = vec3(0.0f);                        /**< Forward direction vector derived from rotation. */
        vec3 right = vec3(0.0f);                          /**< Right direction vector derived from rotation. */

        float speed = 15.0f;                              /**< Movement speed. */
        mat4 viewMatrix = { 1.0f };                       /**< Calculated view transformation matrix. */

        bool hasValidPickRay = false;                     /**< True if the cursor-to-world ray is valid. */
        vec2 framebufferSize = vec2(0.0f);                /**< Size of the full window framebuffer. */
        vec2 viewportSize = vec2(0.0f);                   /**< Size of the active rendering viewport. */
        vec2 viewportPos = vec2(0.0f);                    /**< Top-left offset of the viewport. */

        float imGuiMouseX = 0.0f;                         /**< Mouse X position relative to ImGui context. */
        float imGuiMouseY = 0.0f;                         /**< Mouse Y position relative to ImGui context. */

        Ray cursorToWorldRay{ vec3(0.0f), vec3(0.0f) };   /**< Ray projected from mouse position into 3D space. */
    };

    /** @brief Generic matrix/vector inversion wrapper. */
    template<typename T>
    [[nodiscard]] constexpr inline T inverse(const T& matrix) noexcept {
        return glm::inverse(matrix);
    }

    /** @brief Converts degrees to radians. */
    template<typename T>
    [[nodiscard]] constexpr inline T radians(const T degrees) noexcept {
        return glm::radians(degrees);
    }

    /**
     * @brief Converts radians to degrees.
     * * This is the inverse of the radians() function. It is particularly useful
     * for converting internal math results (like quaternion euler angles)
     * into a human-readable format for UI display or logging.
     * * @param radians The angle expressed in radians.
     * @return The angle converted to degrees.
     */
    template<typename T>
    [[nodiscard]] constexpr inline T degrees(const T radians) noexcept {
        return glm::degrees(radians);
    }

    /**
     * @brief Extracts Euler angles from a quaternion.
     * * Converts a rotation quaternion into a 3D vector of Euler angles.
     * The angles are returned in **radians**. To display them in a UI,
     * you should convert the result using amath::degrees().
     * * @param q The source orientation quaternion.
     * @return A vec3 containing the rotation angles (Pitch, Yaw, Roll) in radians.
     * * @note Euler angles can be ambiguous (multiple angles can represent the same
     * orientation). For internal logic, always prefer using the quaternion directly.
     */
    [[nodiscard]] inline vec3 eulerAngles(const quat& q) noexcept {
        return glm::eulerAngles(q);
    }

    /** @brief Returns a unit vector in the same direction as the input. */
    template<typename T>
    [[nodiscard]] constexpr inline T normalize(const T& value) noexcept {
        return glm::normalize(value);
    }

    /** @brief Computes the cross product of two vectors. */
    template<typename T>
    [[nodiscard]] constexpr inline T cross(const T& x, const T& y) noexcept {
        return glm::cross(x, y);
    }

    /** @brief Creates a perspective projection matrix. */
    template<typename T>
    [[nodiscard]] inline mat4 perspective(T fov, T aspect, T zNear, T zFar) noexcept {
        return glm::perspective(fov, aspect, zNear, zFar);
    }

    /** @brief Creates a view matrix for a camera looking at a specific point. */
    [[nodiscard]] inline mat4 lookAt(const vec3& eye, const vec3& center, const vec3& up) noexcept {
        return glm::lookAt(eye, center, up);
    }

    /** @brief Generic absolute value function. */
    template<typename T>
    [[nodiscard]] constexpr inline T abs(T value) noexcept {
        return glm::abs(value);
    }

    /**
         * @brief Builds a translation matrix by applying a 3D vector to an existing matrix.
         * * @param m The source matrix to be translated.
         * @param value The translation vector (x, y, z).
         * @return The resulting transformation matrix.
         */
    [[nodiscard]] inline mat4 translate(const mat4& m, const vec3& value) noexcept {
        return glm::translate(m, value);
    }

    [[nodiscard]] inline float max(float x, float y) noexcept {
        return glm::max(x,y);
    }

    /**
     * @brief Builds a scale matrix by applying a 3D vector to an existing matrix.
     * * @param m The source matrix to be scaled.
     * @param value The scale factors for each axis (x, y, z).
     * @return The resulting transformation matrix.
     */
    [[nodiscard]] inline mat4 scale(const mat4& m, const vec3& value) noexcept {
        return glm::scale(m, value);
    }

    /**
     * @brief Builds a rotation matrix around a specific axis.
     * * @param m The source matrix to be rotated.
     * @param angle The rotation angle expressed in **radians**.
     * @param value The axis vector to rotate around (should be normalized).
     * @return The resulting transformation matrix.
     * * @note If using Euler angles in degrees, convert them using amath::radians()
     */
    [[nodiscard]] inline mat4 rotate(const mat4& m, const float angle, const vec3& value) noexcept {
        return glm::rotate(m, angle, value);
    }

    /** @brief String conversion for logging; handles arithmetic types and GLM types. */
    template<typename T>
    [[nodiscard]] inline std::string to_string(const T& value) {
        if constexpr (std::is_arithmetic_v<T>) {
            return std::to_string(value);
        }
        else {
            return glm::to_string(value);
        }
    }
     /** * @brief Returns a pointer to the underlying float data of a 4x4 matrix.
     * @note Useful for passing matrix data to APIs like ImGuizmo or OpenGL.
     **/
     [[nodiscard]] inline float* glmValuePtr(mat4& objectMatrix) {
        return glm::value_ptr(objectMatrix);
    }
     /**
     * @brief Converts a rotation quaternion into a 4x4 transformation matrix.
     * * This function creates a rotation matrix that represents the same orientation
     * as the provided quaternion. Using quaternions for rotation avoids the
     * "Gimbal Lock" phenomenon associated with Euler angles.
     * * @param q The source orientation stored as a quaternion.
     * @return A 4x4 rotation matrix representing the quaternion's orientation.
     * * @note Quaternions provide a more stable and efficient way to handle 3D rotations,
     * especially when interpolating between two orientations.
     */
    [[nodiscard]] inline mat4 mat4_cast(const quat& q) {
         return glm::mat4_cast(q);
    }
    /**
     * @brief Calculates intersection point of a ray with the XZ plane.
     * @param ray The ray to test.
     * @param planeY Height of the XZ plane.
     * @param outHitPoint Output for the intersection coordinates.
     * @return true if an intersection exists (ray is not parallel).
     */
    inline bool RayIntersectsXZPlane(const amath::Ray& ray, float planeY, vec3& outHitPoint)
    {
        if (amath::abs(ray.direction.y) < 1e-6f) {
            return false;
        }
        float t = (planeY - ray.origin.y) / ray.direction.y;
        if (t < 0.0f) {
            return false;
        }
        outHitPoint = ray.origin + t * ray.direction;
        return true;
    }
}