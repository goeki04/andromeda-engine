#include <gtest/gtest.h>

#include "a_math.hpp"
#include "a_components.hpp"

using namespace Andromeda;

// ---------------------------------------------------------------------------
// amath wrappers
// ---------------------------------------------------------------------------

TEST(Math, RadiansDegreesRoundTrip) {
    EXPECT_NEAR(amath::radians(180.0f), 3.14159265f, 1e-4f);
    EXPECT_NEAR(amath::degrees(3.14159265f), 180.0f, 1e-3f);
}

TEST(Math, TranslateSetsTranslationColumn) {
    const mat4 m = amath::translate(mat4(1.0f), vec3(1.0f, 2.0f, 3.0f));
    EXPECT_FLOAT_EQ(m[3].x, 1.0f);
    EXPECT_FLOAT_EQ(m[3].y, 2.0f);
    EXPECT_FLOAT_EQ(m[3].z, 3.0f);
}

TEST(Math, ScaleSetsDiagonal) {
    const mat4 s = amath::scale(mat4(1.0f), vec3(2.0f, 3.0f, 4.0f));
    EXPECT_FLOAT_EQ(s[0].x, 2.0f);
    EXPECT_FLOAT_EQ(s[1].y, 3.0f);
    EXPECT_FLOAT_EQ(s[2].z, 4.0f);
}

// ---------------------------------------------------------------------------
// RayIntersectsXZPlane (custom logic - the main reason to test the math module)
// ---------------------------------------------------------------------------

TEST(Math, RayHitsPlaneFromAbove) {
    amath::Ray ray{ vec3(0.0f, 10.0f, 0.0f), vec3(0.0f, -1.0f, 0.0f) };
    vec3 hit;
    EXPECT_TRUE(amath::RayIntersectsXZPlane(ray, 0.0f, hit));
    EXPECT_NEAR(hit.x, 0.0f, 1e-4f);
    EXPECT_NEAR(hit.y, 0.0f, 1e-4f);
    EXPECT_NEAR(hit.z, 0.0f, 1e-4f);
}

TEST(Math, RayParallelToPlaneMisses) {
    amath::Ray ray{ vec3(0.0f, 10.0f, 0.0f), vec3(1.0f, 0.0f, 0.0f) };
    vec3 hit;
    EXPECT_FALSE(amath::RayIntersectsXZPlane(ray, 0.0f, hit));
}

TEST(Math, RayPointingAwayFromPlaneMisses) {
    amath::Ray ray{ vec3(0.0f, 10.0f, 0.0f), vec3(0.0f, 1.0f, 0.0f) };
    vec3 hit;
    EXPECT_FALSE(amath::RayIntersectsXZPlane(ray, 0.0f, hit));
}

// ---------------------------------------------------------------------------
// Transform::modelMatrix
// ---------------------------------------------------------------------------

TEST(Transform, DefaultModelMatrixIsIdentity) {
    ECS::Component::Transform t; // pos 0, scale 1, identity rotation
    const mat4 m = t.modelMatrix();
    EXPECT_FLOAT_EQ(m[0].x, 1.0f);
    EXPECT_FLOAT_EQ(m[1].y, 1.0f);
    EXPECT_FLOAT_EQ(m[2].z, 1.0f);
    EXPECT_FLOAT_EQ(m[3].w, 1.0f);
    EXPECT_FLOAT_EQ(m[3].x, 0.0f);
    EXPECT_FLOAT_EQ(m[3].y, 0.0f);
    EXPECT_FLOAT_EQ(m[3].z, 0.0f);
}

TEST(Transform, ModelMatrixAppliesPosition) {
    ECS::Component::Transform t;
    t.position = { 5.0f, 6.0f, 7.0f };
    const mat4 m = t.modelMatrix();
    EXPECT_FLOAT_EQ(m[3].x, 5.0f);
    EXPECT_FLOAT_EQ(m[3].y, 6.0f);
    EXPECT_FLOAT_EQ(m[3].z, 7.0f);
}
