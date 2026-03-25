#include <gtest/gtest.h>
#include <corecrt_math_defines.h>
#include "Math/Matrix4f.h"
#include "Math/Vector3f.h"
#include <cmath>

TEST(Matrix4x4Test, DefaultConstructorIsIdentity) {
    Matrix4f m;
    EXPECT_FLOAT_EQ(m(0, 0), 1.0f);
    EXPECT_FLOAT_EQ(m(1, 1), 1.0f);
    EXPECT_FLOAT_EQ(m(2, 2), 1.0f);
    EXPECT_FLOAT_EQ(m(3, 3), 1.0f);
    for (int row = 0; row < 4; ++row)
        for (int col = 0; col < 4; ++col)
            if (row != col)
                EXPECT_FLOAT_EQ(m(row, col), 0.0f);
}

TEST(Matrix4x4Test, ZeroMatrix) {
    Matrix4f m;
    m.zero();
    for (int row = 0; row < 4; ++row)
        for (int col = 0; col < 4; ++col)
            EXPECT_FLOAT_EQ(m(row, col), 0.0f);
}

TEST(Matrix4x4Test, ElementAccessAndAssignment) {
    Matrix4f m;
    m(2, 1) = 42.0f;
    EXPECT_FLOAT_EQ(m(2, 1), 42.0f);
}

TEST(Matrix4x4Test, AdditionAndSubtraction) {
    Matrix4f a;
    a(0, 0) = 1.0f; a(1, 1) = 2.0f; a(2, 2) = 3.0f; a(3, 3) = 4.0f;
    Matrix4f b;
    b(0, 0) = 4.0f; b(1, 1) = 3.0f; b(2, 2) = 2.0f; b(3, 3) = 1.0f;
    Matrix4f sum = a + b;
    EXPECT_FLOAT_EQ(sum(0, 0), 5.0f);
    EXPECT_FLOAT_EQ(sum(1, 1), 5.0f);
    EXPECT_FLOAT_EQ(sum(2, 2), 5.0f);
    EXPECT_FLOAT_EQ(sum(3, 3), 5.0f);

    Matrix4f diff = a - b;
    EXPECT_FLOAT_EQ(diff(0, 0), -3.0f);
    EXPECT_FLOAT_EQ(diff(1, 1), -1.0f);
    EXPECT_FLOAT_EQ(diff(2, 2), 1.0f);
    EXPECT_FLOAT_EQ(diff(3, 3), 3.0f);
}

TEST(Matrix4x4Test, ScalarMultiplication) {
    Matrix4f m;
    m(0, 0) = 2.0f;
    m(1, 1) = 3.0f;
    Matrix4f result = m * 2.0f;
    EXPECT_FLOAT_EQ(result(0, 0), 4.0f);
    EXPECT_FLOAT_EQ(result(1, 1), 6.0f);

    result = 3.0f * m;
    EXPECT_FLOAT_EQ(result(0, 0), 6.0f);
    EXPECT_FLOAT_EQ(result(1, 1), 9.0f);
}

TEST(Matrix4x4Test, MatrixMultiplication) {
    Matrix4f a = Matrix4f::scale(2.0f, 3.0f, 4.0f);
    Matrix4f b = Matrix4f::translation(5.0f, 6.0f, 7.0f);
    Matrix4f c = b * a;
    Vector3f v(1.0f, 1.0f, 1.0f);
    Vector3f transformed = c.transformPoint(v);
    // Should scale first, then translate
    EXPECT_FLOAT_EQ(transformed.getX(), 2.0f + 5.0f);
    EXPECT_FLOAT_EQ(transformed.getY(), 3.0f + 6.0f);
    EXPECT_FLOAT_EQ(transformed.getZ(), 4.0f + 7.0f);
}

TEST(Matrix4x4Test, Transpose) {
    Matrix4f m(
        1, 2, 3, 4,
        5, 6, 7, 8,
        9, 10, 11, 12,
        13, 14, 15, 16
    );
    Matrix4f t = m.transpose();
    for (int row = 0; row < 4; ++row)
        for (int col = 0; col < 4; ++col)
            EXPECT_FLOAT_EQ(t(row, col), m(col, row));
}

TEST(Matrix4x4Test, DeterminantAndInverseIdentity) {
    Matrix4f m;
    EXPECT_FLOAT_EQ(m.determinant(), 1.0f);
    Matrix4f inv = m.inverse();
    for (int row = 0; row < 4; ++row)
        for (int col = 0; col < 4; ++col)
            EXPECT_FLOAT_EQ(inv(row, col), m(row, col));
}

TEST(Matrix4x4Test, InverseOfScale) {
    Matrix4f s = Matrix4f::scale(2.0f, 3.0f, 4.0f);
    Matrix4f inv = s.inverse();
    Vector3f v(2.0f, 3.0f, 4.0f);
    Vector3f transformed = inv.transformPoint(s.transformPoint(v));
    EXPECT_NEAR(transformed.getX(), v.getX(), 1e-6f);
    EXPECT_NEAR(transformed.getY(), v.getY(), 1e-6f);
    EXPECT_NEAR(transformed.getZ(), v.getZ(), 1e-6f);
}

TEST(Matrix4x4Test, TransformPointAndVector) {
    Matrix4f t = Matrix4f::translation(1.0f, 2.0f, 3.0f);
    Vector3f p(4.0f, 5.0f, 6.0f);
    Vector3f tp = t.transformPoint(p);
    EXPECT_FLOAT_EQ(tp.getX(), 5.0f);
    EXPECT_FLOAT_EQ(tp.getY(), 7.0f);
    EXPECT_FLOAT_EQ(tp.getZ(), 9.0f);

    Vector3f v(1.0f, 0.0f, 0.0f);
    Matrix4f s = Matrix4f::scale(2.0f, 3.0f, 4.0f);
    Vector3f tv = s.transformVector(v);
    EXPECT_FLOAT_EQ(tv.getX(), 2.0f);
    EXPECT_FLOAT_EQ(tv.getY(), 0.0f);
    EXPECT_FLOAT_EQ(tv.getZ(), 0.0f);
}

TEST(Matrix4x4Test, RotationX) {
    float angle = static_cast<float>(M_PI) / 2.0f;
    Matrix4f rx = Matrix4f::rotationX(angle);
    Vector3f v(0.0f, 1.0f, 0.0f);
    Vector3f rotated = rx.transformVector(v);
    EXPECT_NEAR(rotated.getY(), 0.0f, 1e-6f);
    EXPECT_NEAR(rotated.getZ(), 1.0f, 1e-6f);
}

TEST(Matrix4x4Test, RotationY) {
    float angle = static_cast<float>(M_PI) / 2.0f;
    Matrix4f ry = Matrix4f::rotationY(angle);
    Vector3f v(0.0f, 0.0f, 1.0f);
    Vector3f rotated = ry.transformVector(v);
    EXPECT_NEAR(rotated.getX(), 1.0f, 1e-6f);
    EXPECT_NEAR(rotated.getZ(), 0.0f, 1e-6f);
}

TEST(Matrix4x4Test, RotationZ) {
    float angle = static_cast<float>(M_PI) / 2.0f;
    Matrix4f rz = Matrix4f::rotationZ(angle);
    Vector3f v(1.0f, 0.0f, 0.0f);
    Vector3f rotated = rz.transformVector(v);
    EXPECT_NEAR(rotated.getX(), 0.0f, 1e-6f);
    EXPECT_NEAR(rotated.getY(), 1.0f, 1e-6f);
}

TEST(Matrix4x4Test, PerspectiveMatrix) {
    float fov = static_cast<float>(M_PI) / 2.0f;
    float aspect = 1.0f;
    float nearPlane = 1.0f;
    float farPlane = 10.0f;
    Matrix4f persp = Matrix4f::perspective(fov, aspect, nearPlane, farPlane);
    EXPECT_LT(std::abs(persp(3, 2)), 1.1f); // Should be -1
}

TEST(Matrix4x4Test, OrthographicMatrix) {
    Matrix4f ortho = Matrix4f::orthographic(-1, 1, -1, 1, 1, 10);
    EXPECT_FLOAT_EQ(ortho(0, 0), 1.0f);
    EXPECT_FLOAT_EQ(ortho(1, 1), 1.0f);
    EXPECT_FLOAT_EQ(ortho(2, 2), -0.22222222f);
    EXPECT_FLOAT_EQ(ortho(3, 3), 1.0f);
}

TEST(Matrix4x4Test, LookAtMatrix) {
    Matrix4f look = Matrix4f::lookAt(0, 0, 0, 0, 0, -1, 0, 1, 0);
    // Forward vector should be -Z
    EXPECT_NEAR(look(0, 2), 0.0f, 1e-6f);
    EXPECT_NEAR(look(1, 2), 0.0f, 1e-6f);
    EXPECT_NEAR(look(2, 2), 1.0f, 1e-6f);
}