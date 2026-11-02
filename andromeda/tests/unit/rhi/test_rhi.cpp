#include <gtest/gtest.h>

#include "a_rhi_types.hpp"

using namespace Andromeda;

// BufferUsageToGL is a pure, backend-agnostic enum -> OpenGL token mapping.
// Constexpr, so we can even verify it at compile time.

static_assert(BufferUsageToGL(BufferUsage::StaticDraw) == 0x88E4u,
              "StaticDraw must map to GL_STATIC_DRAW at compile time");

TEST(RhiTypes, BufferUsageToGLMapsKnownValues) {
    EXPECT_EQ(BufferUsageToGL(BufferUsage::StreamDraw), 0x88E0u);
    EXPECT_EQ(BufferUsageToGL(BufferUsage::StreamRead), 0x88E1u);
    EXPECT_EQ(BufferUsageToGL(BufferUsage::StreamCopy), 0x88E2u);

    EXPECT_EQ(BufferUsageToGL(BufferUsage::StaticDraw), 0x88E4u);
    EXPECT_EQ(BufferUsageToGL(BufferUsage::StaticRead), 0x88E5u);
    EXPECT_EQ(BufferUsageToGL(BufferUsage::StaticCopy), 0x88E6u);

    EXPECT_EQ(BufferUsageToGL(BufferUsage::DynamicDraw), 0x88E8u);
    EXPECT_EQ(BufferUsageToGL(BufferUsage::DynamicRead), 0x88E9u);
    EXPECT_EQ(BufferUsageToGL(BufferUsage::DynamicCopy), 0x88EAu);
}
