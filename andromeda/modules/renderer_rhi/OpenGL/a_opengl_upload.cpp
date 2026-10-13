#include "a_opengl_upload.hpp"
#include "GL/glew.h"
#include "a_opengl_handles.hpp"
#include "a_geometry.hpp"
namespace Andromeda {
    void createMesh(MeshGPUHandle& glMesh,const Mesh& mesh)
    {
        glGenVertexArrays(1, &glMesh.vao);
        glGenBuffers(1, &glMesh.vbo);
        glGenBuffers(1, &glMesh.ebo);
        glBindVertexArray(glMesh.vao);
        glBindBuffer(GL_ARRAY_BUFFER, glMesh.vbo);
        glBufferData(GL_ARRAY_BUFFER,
            mesh.vertexbuffer.size() * sizeof(Vertex),
            mesh.vertexbuffer.empty() ? nullptr : mesh.vertexbuffer.data(),
            GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, glMesh.ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,
            mesh.indexBuffer.size() * sizeof(u32),
            mesh.indexBuffer.empty() ? nullptr : mesh.indexBuffer.data(),
            GL_STATIC_DRAW);
        //positions
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), static_cast<void *>(nullptr));
        //normals
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void *>(offsetof(Vertex, normal)));
        //texCoords
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void *>(offsetof(Vertex, uv)));
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void *>(offsetof(Vertex, color)));
        glBindVertexArray(0);
    }

    void destroyMesh(MeshGPUHandle& glMesh) {
        glDeleteVertexArrays(1, &glMesh.vao);
        glDeleteBuffers(1, &glMesh.vbo);
        glDeleteBuffers(1, &glMesh.ebo);
        glMesh.vao = 0;
        glMesh.vbo = 0;
        glMesh.ebo = 0;
        glMesh.indexCount = 0;
    }
}