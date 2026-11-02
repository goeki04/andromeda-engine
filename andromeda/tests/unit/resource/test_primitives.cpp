#include <gtest/gtest.h>
#include <stdexcept>

#include "a_primitiveGenerator.hpp"
#include "a_geometry.hpp"

using namespace Andromeda;

// ---------------------------------------------------------------------------
// PrimitiveGenerator
// ---------------------------------------------------------------------------

TEST(PrimitiveGenerator, CubeHasExpectedTopology) {
    Mesh cube;
    PrimitiveGenerator::generateCube(cube);

    EXPECT_EQ(cube.vertexbuffer.size(), 24u); // 6 faces * 4 vertices
    EXPECT_EQ(cube.indexBuffer.size(), 36u);  // 6 faces * 2 triangles * 3
    for (u32 i : cube.indexBuffer) {
        EXPECT_LT(i, cube.vertexbuffer.size());
    }
}

TEST(PrimitiveGenerator, PlaneHasExpectedTopology) {
    Mesh plane;
    PrimitiveGenerator::generatePlane(plane);

    EXPECT_EQ(plane.vertexbuffer.size(), 4u);
    EXPECT_EQ(plane.indexBuffer.size(), 6u);
}

TEST(PrimitiveGenerator, SphereProducesValidMesh) {
    Mesh sphere;
    PrimitiveGenerator::generateSphere(sphere);

    EXPECT_FALSE(sphere.vertexbuffer.empty());
    EXPECT_FALSE(sphere.indexBuffer.empty());
    EXPECT_EQ(sphere.indexBuffer.size() % 3u, 0u); // whole triangles
    for (u32 i : sphere.indexBuffer) {
        EXPECT_LT(i, sphere.vertexbuffer.size());
    }
}

TEST(PrimitiveGenerator, RegeneratingClearsPreviousData) {
    Mesh m;
    PrimitiveGenerator::generateCube(m);
    PrimitiveGenerator::generateCube(m); // must reset, not append
    EXPECT_EQ(m.vertexbuffer.size(), 24u);
    EXPECT_EQ(m.indexBuffer.size(), 36u);
}

// ---------------------------------------------------------------------------
// Mesh::getAABB
// ---------------------------------------------------------------------------

TEST(MeshAABB, ComputesMinMaxAndCenter) {
    Mesh m;
    m.vertexbuffer.push_back(Vertex{ vec3(-1.0f, -2.0f, -3.0f) });
    m.vertexbuffer.push_back(Vertex{ vec3(4.0f, 5.0f, 6.0f) });

    const ECS::Component::AABB box = m.getAABB();

    EXPECT_FLOAT_EQ(box.min.x, -1.0f);
    EXPECT_FLOAT_EQ(box.min.y, -2.0f);
    EXPECT_FLOAT_EQ(box.min.z, -3.0f);
    EXPECT_FLOAT_EQ(box.max.x, 4.0f);
    EXPECT_FLOAT_EQ(box.max.y, 5.0f);
    EXPECT_FLOAT_EQ(box.max.z, 6.0f);
    EXPECT_FLOAT_EQ(box.center.x, 1.5f); // (-1 + 4) / 2
}

TEST(MeshAABB, EmptyMeshThrows) {
    Mesh empty;
    EXPECT_THROW((void)empty.getAABB(), std::runtime_error);
}
