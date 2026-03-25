#include <gtest/gtest.h>
#include <corecrt_math_defines.h>
#include "Math/Vector3f.h"

TEST(Vector3fTest, ConstructorsAndGetters) {
    Vector3f v1;
    EXPECT_FLOAT_EQ(v1.getX(), 0.0f);
    EXPECT_FLOAT_EQ(v1.getY(), 0.0f);
    EXPECT_FLOAT_EQ(v1.getZ(), 0.0f);

    Vector3f v2(1.0f, 2.0f, 3.0f);
    EXPECT_FLOAT_EQ(v2.getX(), 1.0f);
    EXPECT_FLOAT_EQ(v2.getY(), 2.0f);
    EXPECT_FLOAT_EQ(v2.getZ(), 3.0f);

    Vector3f v3(v2);
    EXPECT_EQ(v2, v3);
}

TEST(Vector3fTest, Setters) {
    Vector3f v;
    v.setX(4.0f);
    v.setY(5.0f);
    v.setZ(6.0f);
    EXPECT_FLOAT_EQ(v.getX(), 4.0f);
    EXPECT_FLOAT_EQ(v.getY(), 5.0f);
    EXPECT_FLOAT_EQ(v.getZ(), 6.0f);

    v.set(7.0f, 8.0f, 9.0f);
    EXPECT_FLOAT_EQ(v.getX(), 7.0f);
    EXPECT_FLOAT_EQ(v.getY(), 8.0f);
    EXPECT_FLOAT_EQ(v.getZ(), 9.0f);
}

TEST(Vector3fTest, ArithmeticOperators) {
    Vector3f v1(1.0f, 2.0f, 3.0f);
    Vector3f v2(4.0f, 5.0f, 6.0f);

    auto v3 = v1 + v2;
    EXPECT_EQ(v3, Vector3f(5.0f, 7.0f, 9.0f));

    v3 -= v1;
    EXPECT_EQ(v3, v2);

    auto v4 = v2 - v1;
    EXPECT_EQ(v4, Vector3f(3.0f, 3.0f, 3.0f));

    v4 += v1;
    EXPECT_EQ(v4, Vector3f(4.0f, 5.0f, 6.0f));

    auto v5 = -v1;
    EXPECT_EQ(v5, Vector3f(-1.0f, -2.0f, -3.0f));
}

TEST(Vector3fTest, ScalarOperators) {
    Vector3f v(1.0f, -2.0f, 3.0f);

    auto v2 = v * 2.0f;
    EXPECT_EQ(v2, Vector3f(2.0f, -4.0f, 6.0f));

    v2 *= 0.5f;
    EXPECT_EQ(v2, Vector3f(1.0f, -2.0f, 3.0f));

    auto v3 = v / 2.0f;
    EXPECT_EQ(v3, Vector3f(0.5f, -1.0f, 1.5f));

    v3 /= 0.5f;
    EXPECT_EQ(v3, Vector3f(1.0f, -2.0f, 3.0f));

    EXPECT_THROW(v / 0.0f, std::invalid_argument);
    EXPECT_THROW(v /= 0.0f, std::invalid_argument);

    auto v4 = 3.0f * v;
    EXPECT_EQ(v4, Vector3f(3.0f, -6.0f, 9.0f));
}

TEST(Vector3fTest, ComparisonOperators) {
    Vector3f v1(1.0f, 2.0f, 3.0f);
    Vector3f v2(1.0f, 2.0f, 3.0f);
    Vector3f v3(1.0f, 2.0f, 3.00001f);

    EXPECT_TRUE(v1 == v2);
    EXPECT_FALSE(v1 != v2);
    EXPECT_FALSE(v1 == v3);
    EXPECT_TRUE(v1 != v3);
}

TEST(Vector3fTest, DotAndCrossProduct) {
    Vector3f v1(1.0f, 0.0f, 0.0f);
    Vector3f v2(0.0f, 1.0f, 0.0f);

    EXPECT_FLOAT_EQ(v1.dot(v2), 0.0f);
    EXPECT_EQ(v1.cross(v2), Vector3f(0.0f, 0.0f, 1.0f));
}

TEST(Vector3fTest, MagnitudeAndNormalization) {
    Vector3f v(3.0f, 4.0f, 0.0f);
    EXPECT_FLOAT_EQ(v.magnitude(), 5.0f);
    EXPECT_FLOAT_EQ(v.magnitudeSquared(), 25.0f);

    auto vn = v.normalized();
    EXPECT_FLOAT_EQ(vn.magnitude(), 1.0f);

    v.normalize();
    EXPECT_FLOAT_EQ(v.magnitude(), 1.0f);

    Vector3f zero;
    EXPECT_THROW(zero.normalized(), std::runtime_error);
    EXPECT_THROW(zero.normalize(), std::runtime_error);
}

TEST(Vector3fTest, DistanceAndAngle) {
    Vector3f v1(1.0f, 0.0f, 0.0f);
    Vector3f v2(0.0f, 1.0f, 0.0f);

    EXPECT_FLOAT_EQ(v1.distanceTo(v2), std::sqrt(2.0f));
    EXPECT_FLOAT_EQ(v1.distanceSquaredTo(v2), 2.0f);

    EXPECT_NEAR(v1.angleTo(v2), M_PI_2, 1e-6f);

    Vector3f zero;
    EXPECT_THROW(v1.angleTo(zero), std::runtime_error);
}

TEST(Vector3fTest, LerpAndProjection) {
    Vector3f a(0.0f, 0.0f, 0.0f);
    Vector3f b(10.0f, 0.0f, 0.0f);

    auto mid = Vector3f::lerp(a, b, 0.5f);
    EXPECT_EQ(mid, Vector3f(5.0f, 0.0f, 0.0f));

    Vector3f v(2.0f, 3.0f, 0.0f);
    Vector3f onto(1.0f, 0.0f, 0.0f);
    auto proj = v.projectOnto(onto);
    EXPECT_EQ(proj, Vector3f(2.0f, 0.0f, 0.0f));

    Vector3f zero;
    EXPECT_THROW(v.projectOnto(zero), std::runtime_error);
}

TEST(Vector3fTest, Reflect) {
    Vector3f v(1.0f, -1.0f, 0.0f);
    Vector3f normal(0.0f, 1.0f, 0.0f);

    auto reflected = v.reflect(normal);
    EXPECT_EQ(reflected, Vector3f(1.0f, 1.0f, 0.0f));
}

TEST(Vector3fTest, StaticVectors) {
    EXPECT_EQ(Vector3f::zero(), Vector3f(0.0f, 0.0f, 0.0f));
    EXPECT_EQ(Vector3f::one(), Vector3f(1.0f, 1.0f, 1.0f));
    EXPECT_EQ(Vector3f::right(), Vector3f(1.0f, 0.0f, 0.0f));
    EXPECT_EQ(Vector3f::up(), Vector3f(0.0f, 1.0f, 0.0f));
    EXPECT_EQ(Vector3f::forward(), Vector3f(0.0f, 0.0f, 1.0f));
}

TEST(Vector3fTest, IsZeroAndIsUnit) {
    EXPECT_TRUE(Vector3f::zero().isZero());
    EXPECT_FALSE(Vector3f::one().isZero());

    Vector3f v(1.0f, 0.0f, 0.0f);
    EXPECT_TRUE(v.isUnit());
    EXPECT_FALSE(Vector3f(2.0f, 0.0f, 0.0f).isUnit());
}