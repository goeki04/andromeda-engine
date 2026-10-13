#pragma once

/**
 * @file a_opengl_upload.hpp
 * @brief Helper for uploading CPU mesh data into OpenGL GPU buffers.
 */

namespace Andromeda {
	struct MeshGPUHandle;
	struct Mesh;

	/**
	 * @brief Uploads a CPU-side @c Mesh into OpenGL buffers (VAO/VBO/EBO) and configures vertex attributes.
	 * @param glMesh Output handle that receives the created OpenGL object IDs and index count.
	 * @param mesh The source mesh (vertex and index data) to upload.
	 */
	void createMesh(MeshGPUHandle& glMesh, const Mesh& mesh);

	void destroyMesh(MeshGPUHandle& glMesh);
}