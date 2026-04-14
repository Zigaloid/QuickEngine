#pragma once

#include <cmath>
#include <array>
#include <iostream>
#include <vector>
#include <tuple>
#include "Vector3f.h"

class Matrix4f {
private:
    // Column-major storage (OpenGL compatible)
    // m_matrix[0-3] = column 0, m_matrix[4-7] = column 1, etc.
    std::array<float, 16> m_matrix;

public:
    // Constructors
    Matrix4f() {
        identity();
    }

    Matrix4f(const std::array<float, 16>& data) : m_matrix(data) {}

    Matrix4f(float m00, float m01, float m02, float m03,
        float m10, float m11, float m12, float m13,
        float m20, float m21, float m22, float m23,
        float m30, float m31, float m32, float m33) {
        // Row-major input, stored as column-major
        m_matrix[0] = m00; m_matrix[4] = m01; m_matrix[8] = m02; m_matrix[12] = m03;
        m_matrix[1] = m10; m_matrix[5] = m11; m_matrix[9] = m12; m_matrix[13] = m13;
        m_matrix[2] = m20; m_matrix[6] = m21; m_matrix[10] = m22; m_matrix[14] = m23;
        m_matrix[3] = m30; m_matrix[7] = m31; m_matrix[11] = m32; m_matrix[15] = m33;
    }
    // Add this public method to Matrix4x4
    void setColumn(int col, const Vector3f& v) {
        m_matrix[col * 4 + 0] = v.x;
        m_matrix[col * 4 + 1] = v.y;
        m_matrix[col * 4 + 2] = v.z;
    }
    auto& GetData() const { return m_matrix; }
    auto& GetWriteData() { return m_matrix; }

    // Element access
    float& operator()(int row, int col) {
        return m_matrix[col * 4 + row];
    }

    const float& operator()(int row, int col) const {
        return m_matrix[col * 4 + row];
    }

    float* data() { return m_matrix.data(); }
    const float* data() const { return m_matrix.data(); }

    // Basic operations
    void identity() {
        m_matrix.fill(0.0f);
        m_matrix[0] = m_matrix[5] = m_matrix[10] = m_matrix[15] = 1.0f;
    }

    void zero() {
        m_matrix.fill(0.0f);
    }

    Matrix4f transpose() const {
        Matrix4f result;
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                result(i, j) = (*this)(j, i);
            }
        }
        return result;
    }

    float determinant() const {
        float a = m_matrix[0], b = m_matrix[4], c = m_matrix[8], d = m_matrix[12];
        float e = m_matrix[1], f = m_matrix[5], g = m_matrix[9], h = m_matrix[13];
        float i = m_matrix[2], j = m_matrix[6], k = m_matrix[10], l = m_matrix[14];
        float m = m_matrix[3], n = m_matrix[7], o = m_matrix[11], p = m_matrix[15];

        return a * (f * (k * p - l * o) - g * (j * p - l * n) + h * (j * o - k * n)) -
            b * (e * (k * p - l * o) - g * (i * p - l * m) + h * (i * o - k * m)) +
            c * (e * (j * p - l * n) - f * (i * p - l * m) + h * (i * n - j * m)) -
            d * (e * (j * o - k * n) - f * (i * o - k * m) + g * (i * n - j * m));
    }

    Matrix4f inverse() const {
        Matrix4f result;
        float det = determinant();

        if (std::abs(det) < 1e-6f) {
            // Matrix is singular, return identity
            result.identity();
            return result;
        }

        float invDet = 1.0f / det;

        // Calculate cofactors and transpose
        result.m_matrix[0] = invDet * (m_matrix[5] * (m_matrix[10] * m_matrix[15] - m_matrix[11] * m_matrix[14]) - m_matrix[9] * (m_matrix[6] * m_matrix[15] - m_matrix[7] * m_matrix[14]) + m_matrix[13] * (m_matrix[6] * m_matrix[11] - m_matrix[7] * m_matrix[10]));
        result.m_matrix[1] = invDet * -(m_matrix[1] * (m_matrix[10] * m_matrix[15] - m_matrix[11] * m_matrix[14]) - m_matrix[9] * (m_matrix[2] * m_matrix[15] - m_matrix[3] * m_matrix[14]) + m_matrix[13] * (m_matrix[2] * m_matrix[11] - m_matrix[3] * m_matrix[10]));
        result.m_matrix[2] = invDet * (m_matrix[1] * (m_matrix[6] * m_matrix[15] - m_matrix[7] * m_matrix[14]) - m_matrix[5] * (m_matrix[2] * m_matrix[15] - m_matrix[3] * m_matrix[14]) + m_matrix[13] * (m_matrix[2] * m_matrix[7] - m_matrix[3] * m_matrix[6]));
        result.m_matrix[3] = invDet * -(m_matrix[1] * (m_matrix[6] * m_matrix[11] - m_matrix[7] * m_matrix[10]) - m_matrix[5] * (m_matrix[2] * m_matrix[11] - m_matrix[3] * m_matrix[10]) + m_matrix[9] * (m_matrix[2] * m_matrix[7] - m_matrix[3] * m_matrix[6]));

        result.m_matrix[4] = invDet * -(m_matrix[4] * (m_matrix[10] * m_matrix[15] - m_matrix[11] * m_matrix[14]) - m_matrix[8] * (m_matrix[6] * m_matrix[15] - m_matrix[7] * m_matrix[14]) + m_matrix[12] * (m_matrix[6] * m_matrix[11] - m_matrix[7] * m_matrix[10]));
        result.m_matrix[5] = invDet * (m_matrix[0] * (m_matrix[10] * m_matrix[15] - m_matrix[11] * m_matrix[14]) - m_matrix[8] * (m_matrix[2] * m_matrix[15] - m_matrix[3] * m_matrix[14]) + m_matrix[12] * (m_matrix[2] * m_matrix[11] - m_matrix[3] * m_matrix[10]));
        result.m_matrix[6] = invDet * -(m_matrix[0] * (m_matrix[6] * m_matrix[15] - m_matrix[7] * m_matrix[14]) - m_matrix[4] * (m_matrix[2] * m_matrix[15] - m_matrix[3] * m_matrix[14]) + m_matrix[12] * (m_matrix[2] * m_matrix[7] - m_matrix[3] * m_matrix[6]));
        result.m_matrix[7] = invDet * (m_matrix[0] * (m_matrix[6] * m_matrix[11] - m_matrix[7] * m_matrix[10]) - m_matrix[4] * (m_matrix[2] * m_matrix[11] - m_matrix[3] * m_matrix[10]) + m_matrix[8] * (m_matrix[2] * m_matrix[7] - m_matrix[3] * m_matrix[6]));

        result.m_matrix[8] = invDet * (m_matrix[4] * (m_matrix[9] * m_matrix[15] - m_matrix[11] * m_matrix[13]) - m_matrix[8] * (m_matrix[5] * m_matrix[15] - m_matrix[7] * m_matrix[13]) + m_matrix[12] * (m_matrix[5] * m_matrix[11] - m_matrix[7] * m_matrix[9]));
        result.m_matrix[9] = invDet * -(m_matrix[0] * (m_matrix[9] * m_matrix[15] - m_matrix[11] * m_matrix[13]) - m_matrix[8] * (m_matrix[1] * m_matrix[15] - m_matrix[3] * m_matrix[13]) + m_matrix[12] * (m_matrix[1] * m_matrix[11] - m_matrix[3] * m_matrix[9]));
        result.m_matrix[10] = invDet * (m_matrix[0] * (m_matrix[5] * m_matrix[15] - m_matrix[7] * m_matrix[13]) - m_matrix[4] * (m_matrix[1] * m_matrix[15] - m_matrix[3] * m_matrix[13]) + m_matrix[12] * (m_matrix[1] * m_matrix[7] - m_matrix[3] * m_matrix[5]));
        result.m_matrix[11] = invDet * -(m_matrix[0] * (m_matrix[5] * m_matrix[11] - m_matrix[7] * m_matrix[9]) - m_matrix[4] * (m_matrix[1] * m_matrix[11] - m_matrix[3] * m_matrix[9]) + m_matrix[8] * (m_matrix[1] * m_matrix[7] - m_matrix[3] * m_matrix[5]));

        result.m_matrix[12] = invDet * -(m_matrix[4] * (m_matrix[9] * m_matrix[14] - m_matrix[10] * m_matrix[13]) - m_matrix[8] * (m_matrix[5] * m_matrix[14] - m_matrix[6] * m_matrix[13]) + m_matrix[12] * (m_matrix[5] * m_matrix[10] - m_matrix[6] * m_matrix[9]));
        result.m_matrix[13] = invDet * (m_matrix[0] * (m_matrix[9] * m_matrix[14] - m_matrix[10] * m_matrix[13]) - m_matrix[8] * (m_matrix[1] * m_matrix[14] - m_matrix[2] * m_matrix[13]) + m_matrix[12] * (m_matrix[1] * m_matrix[10] - m_matrix[2] * m_matrix[9]));
        result.m_matrix[14] = invDet * -(m_matrix[0] * (m_matrix[5] * m_matrix[14] - m_matrix[6] * m_matrix[13]) - m_matrix[4] * (m_matrix[1] * m_matrix[14] - m_matrix[2] * m_matrix[13]) + m_matrix[12] * (m_matrix[1] * m_matrix[6] - m_matrix[2] * m_matrix[5]));
        result.m_matrix[15] = invDet * (m_matrix[0] * (m_matrix[5] * m_matrix[10] - m_matrix[6] * m_matrix[9]) - m_matrix[4] * (m_matrix[1] * m_matrix[10] - m_matrix[2] * m_matrix[9]) + m_matrix[8] * (m_matrix[1] * m_matrix[6] - m_matrix[2] * m_matrix[5]));

        return result;
    }

    // Arithmetic operators
    Matrix4f operator+(const Matrix4f& other) const {
        Matrix4f result;
        for (int i = 0; i < 16; ++i) {
            result.m_matrix[i] = m_matrix[i] + other.m_matrix[i];
        }
        return result;
    }

    Matrix4f operator-(const Matrix4f& other) const {
        Matrix4f result;
        for (int i = 0; i < 16; ++i) {
            result.m_matrix[i] = m_matrix[i] - other.m_matrix[i];
        }
        return result;
    }

    Matrix4f operator*(const Matrix4f& other) const {
        Matrix4f result;
        result.zero();

        for (int col = 0; col < 4; ++col) {
            for (int row = 0; row < 4; ++row) {
                for (int k = 0; k < 4; ++k) {
                    result(row, col) += (*this)(row, k) * other(k, col);
                }
            }
        }
        return result;
    }

    Matrix4f operator*(float scalar) const {
        Matrix4f result;
        for (int i = 0; i < 16; ++i) {
            result.m_matrix[i] = m_matrix[i] * scalar;
        }
        return result;
    }

    Matrix4f& operator+=(const Matrix4f& other) {
        for (int i = 0; i < 16; ++i) {
            m_matrix[i] += other.m_matrix[i];
        }
        return *this;
    }

    Matrix4f& operator-=(const Matrix4f& other) {
        for (int i = 0; i < 16; ++i) {
            m_matrix[i] -= other.m_matrix[i];
        }
        return *this;
    }

    Matrix4f& operator*=(const Matrix4f& other) {
        *this = *this * other;
        return *this;
    }

    Matrix4f& operator*=(float scalar) {
        for (int i = 0; i < 16; ++i) {
            m_matrix[i] *= scalar;
        }
        return *this;
    }

    // 3D Transformation methods
    static Matrix4f translation(float x, float y, float z) {
        Matrix4f result;
        result.identity();
        result(0, 3) = x;
        result(1, 3) = y;
        result(2, 3) = z;
        return result;
    }

    // Added overload: translation with Vector3f
    static Matrix4f translation(const Vector3f& v) {
        return translation(v.getX(), v.getY(), v.getZ());
    }

    static Matrix4f scale(float x, float y, float z) {
        Matrix4f result;
        result.zero();
        result(0, 0) = x;
        result(1, 1) = y;
        result(2, 2) = z;
        result(3, 3) = 1.0f;
        return result;
    }

    // Added overload: scale with Vector3f
    static Matrix4f scale(const Vector3f& v) {
        return scale(v.getX(), v.getY(), v.getZ());
    }

    static Matrix4f rotationX(float angleRad) {
        Matrix4f result;
        result.identity();
        float c = std::cos(angleRad);
        float s = std::sin(angleRad);
        result(1, 1) = c;
        result(1, 2) = -s;
        result(2, 1) = s;
        result(2, 2) = c;
        return result;
    }

    static Matrix4f rotationY(float angleRad) {
        Matrix4f result;
        result.identity();
        float c = std::cos(angleRad);
        float s = std::sin(angleRad);
        result(0, 0) = c;
        result(0, 2) = s;
        result(2, 0) = -s;
        result(2, 2) = c;
        return result;
    }

    static Matrix4f rotationZ(float angleRad) {
        Matrix4f result;
        result.identity();
        float c = std::cos(angleRad);
        float s = std::sin(angleRad);
        result(0, 0) = c;
        result(0, 1) = -s;
        result(1, 0) = s;
        result(1, 1) = c;
        return result;
    }

    static Matrix4f rotation(float x, float y, float z, float angleRad) {
        // Rodrigues' rotation formula
        float c = std::cos(angleRad);
        float s = std::sin(angleRad);
        float oneMinusC = 1.0f - c;

        // Normalize axis
        float len = std::sqrt(x * x + y * y + z * z);
        if (len < 1e-6f) {
            Matrix4f result;
            result.identity();
            return result;
        }
        x /= len; y /= len; z /= len;

        Matrix4f result;
        result.identity();

        result(0, 0) = c + x * x * oneMinusC;
        result(0, 1) = x * y * oneMinusC - z * s;
        result(0, 2) = x * z * oneMinusC + y * s;

        result(1, 0) = y * x * oneMinusC + z * s;
        result(1, 1) = c + y * y * oneMinusC;
        result(1, 2) = y * z * oneMinusC - x * s;

        result(2, 0) = z * x * oneMinusC - y * s;
        result(2, 1) = z * y * oneMinusC + x * s;
        result(2, 2) = c + z * z * oneMinusC;

        return result;
    }

    // Added overload: rotation with Vector3f axis
    static Matrix4f rotation(const Vector3f& axis, float angleRad) {
        return rotation(axis.getX(), axis.getY(), axis.getZ(), angleRad);
    }

    // Build a TRS matrix from Vector3f components.
    // rotationDegrees is interpreted as Euler angles in degrees (X, Y, Z),
    // using ZYX order (R = Rz * Ry * Rx). Resulting matrix is T * R * S.
    static Matrix4f fromTRS(const Vector3f& translation, const Vector3f& rotationDegrees, const Vector3f& scaleVec) {
        const float DEG_TO_RAD = static_cast<float>(M_PI) / 180.0f;
        float rx = rotationDegrees.getX() * DEG_TO_RAD;
        float ry = rotationDegrees.getY() * DEG_TO_RAD;
        float rz = rotationDegrees.getZ() * DEG_TO_RAD;

        Matrix4f S = Matrix4f::scale(scaleVec);
        Matrix4f Rx = Matrix4f::rotationX(rx);
        Matrix4f Ry = Matrix4f::rotationY(ry);
        Matrix4f Rz = Matrix4f::rotationZ(rz);

        // ZYX Euler: apply Rx, then Ry, then Rz -> combined R = Rz * Ry * Rx
        Matrix4f R = Rz * Ry * Rx;

        Matrix4f T = Matrix4f::translation(translation);

        // Compose: translate * rotate * scale
        return T * R * S;
    }

    // Convert this matrix into TRS components (compatible with fromTRS).
    // Fills output references with translation, rotation (degrees, X,Y,Z ZYX convention), and scale.
    void toTRS(Vector3f& outTranslation, Vector3f& outRotationDegrees, Vector3f& outScale) const {
        decompose(outTranslation, outRotationDegrees, outScale);
    }

    // Convenience overload returning a tuple (translation, rotationDegrees, scale)
    std::tuple<Vector3f, Vector3f, Vector3f> toTRS() const {
        Vector3f t, r, s;
        decompose(t, r, s);
        return std::tuple{ t, r, s };
    }

    // Camera/View matrices
    static Matrix4f lookAt(float eyeX, float eyeY, float eyeZ,
        float centerX, float centerY, float centerZ,
        float upX, float upY, float upZ) {
        // Calculate forward vector (from eye to center)
        float fx = centerX - eyeX;
        float fy = centerY - eyeY;
        float fz = centerZ - eyeZ;

        // Normalize forward
        float flen = std::sqrt(fx * fx + fy * fy + fz * fz);
        fx /= flen; fy /= flen; fz /= flen;

        // Calculate right vector (forward x up)
        float rx = fy * upZ - fz * upY;
        float ry = fz * upX - fx * upZ;
        float rz = fx * upY - fy * upX;

        // Normalize right
        float rlen = std::sqrt(rx * rx + ry * ry + rz * rz);
        rx /= rlen; ry /= rlen; rz /= rlen;

        // Calculate up vector (right x forward)
        float ux = ry * fz - rz * fy;
        float uy = rz * fx - rx * fz;
        float uz = rx * fy - ry * fx;

        Matrix4f result;
        result(0, 0) = rx;  result(0, 1) = ux;  result(0, 2) = -fx; result(0, 3) = -(rx * eyeX + ux * eyeY - fx * eyeZ);
        result(1, 0) = ry;  result(1, 1) = uy;  result(1, 2) = -fy; result(1, 3) = -(ry * eyeX + uy * eyeY - fy * eyeZ);
        result(2, 0) = rz;  result(2, 1) = uz;  result(2, 2) = -fz; result(2, 3) = -(rz * eyeX + uz * eyeY - fz * eyeZ);
        result(3, 0) = 0;   result(3, 1) = 0;   result(3, 2) = 0;   result(3, 3) = 1;

        return result;
    }

    // Added overload: lookAt with Vector3f parameters
    static Matrix4f lookAt(const Vector3f& eye, const Vector3f& center, const Vector3f& up) {
        return lookAt(eye.getX(), eye.getY(), eye.getZ(),
            center.getX(), center.getY(), center.getZ(),
            up.getX(), up.getY(), up.getZ());
    }

    // Projection matrices
    static Matrix4f perspective(float fovYRad, float aspect, float nearPlane, float farPlane) {
        Matrix4f result;
        result.zero();

        float f = 1.0f / std::tan(fovYRad * 0.5f);

        result(0, 0) = f / aspect;
        result(1, 1) = f;
        result(2, 2) = (farPlane + nearPlane) / (nearPlane - farPlane);
        result(2, 3) = (2.0f * farPlane * nearPlane) / (nearPlane - farPlane);
        result(3, 2) = -1.0f;

        return result;
    }

    static Matrix4f orthographic(float left, float right, float bottom, float top, float nearPlane, float farPlane) {
        Matrix4f result;
        result.zero();

        result(0, 0) = 2.0f / (right - left);
        result(1, 1) = 2.0f / (top - bottom);
        result(2, 2) = -2.0f / (farPlane - nearPlane);
        result(0, 3) = -(right + left) / (right - left);
        result(1, 3) = -(top + bottom) / (top - bottom);
        result(2, 3) = -(farPlane + nearPlane) / (farPlane - nearPlane);
        result(3, 3) = 1.0f;

        return result;
    }

    // Vector transformation (treats vector as column vector)
    void transformPoint(float& x, float& y, float& z) const {
        float w = 1.0f;
        float tx = x * m_matrix[0] + y * m_matrix[4] + z * m_matrix[8] + w * m_matrix[12];
        float ty = x * m_matrix[1] + y * m_matrix[5] + z * m_matrix[9] + w * m_matrix[13];
        float tz = x * m_matrix[2] + y * m_matrix[6] + z * m_matrix[10] + w * m_matrix[14];
        float tw = x * m_matrix[3] + y * m_matrix[7] + z * m_matrix[11] + w * m_matrix[15];

        if (std::abs(tw) > 1e-6f) {
            x = tx / tw;
            y = ty / tw;
            z = tz / tw;
        }
        else {
            x = tx;
            y = ty;
            z = tz;
        }
    }

    void transformVector(float& x, float& y, float& z) const {
        float tx = x * m_matrix[0] + y * m_matrix[4] + z * m_matrix[8];
        float ty = x * m_matrix[1] + y * m_matrix[5] + z * m_matrix[9];
        float tz = x * m_matrix[2] + y * m_matrix[6] + z * m_matrix[10];
        x = tx;
        y = ty;
        z = tz;
    }

    // Vector3f transformation methods
    Vector3f transformPoint(const Vector3f& point) const {
        float x = point.getX();
        float y = point.getY();
        float z = point.getZ();
        transformPoint(x, y, z);
        return Vector3f(x, y, z);
    }

    Vector3f transformVector(const Vector3f& vector) const {
        float x = vector.getX();
        float y = vector.getY();
        float z = vector.getZ();
        transformVector(x, y, z);
        return Vector3f(x, y, z);
    }

    // Transform array of Vector3f points
    void transformPoints(std::vector<Vector3f>& points) const {
        for (Vector3f& point : points) {
            point = transformPoint(point);
        }
    }

    void transformPoints(Vector3f* points, size_t count) const {
        for (size_t i = 0; i < count; ++i) {
            points[i] = transformPoint(points[i]);
        }
    }

    // Transform array of Vector3f vectors
    void transformVectors(std::vector<Vector3f>& vectors) const {
        for (Vector3f& vector : vectors) {
            vector = transformVector(vector);
        }
    }

    void transformVectors(Vector3f* vectors, size_t count) const {
        for (size_t i = 0; i < count; ++i) {
            vectors[i] = transformVector(vectors[i]);
        }
    }

    // Non-modifying versions that return new arrays
    std::vector<Vector3f> getTransformedPoints(const std::vector<Vector3f>& points) const {
        std::vector<Vector3f> result;
        result.reserve(points.size());
        for (const Vector3f& point : points) {
            result.push_back(transformPoint(point));
        }
        return result;
    }

    std::vector<Vector3f> getTransformedVectors(const std::vector<Vector3f>& vectors) const {
        std::vector<Vector3f> result;
        result.reserve(vectors.size());
        for (const Vector3f& vector : vectors) {
            result.push_back(transformVector(vector));
        }
        return result;
    }

    // Convenience operator overloads for Vector3f
    Vector3f operator*(const Vector3f& point) const {
        return transformPoint(point);
    }
    void print() const {
        for (int row = 0; row < 4; ++row) {
            std::cout << "[";
            for (int col = 0; col < 4; ++col) {
                std::cout << (*this)(row, col);
                if (col < 3) std::cout << ", ";
            }
            std::cout << "]\n";
        }
    }

    // Matrix decomposition methods
    Vector3f extractTranslation() const {
        return Vector3f(m_matrix[12], m_matrix[13], m_matrix[14]);
    }

    Vector3f extractScale() const {
        // Calculate the length of each basis vector
        Vector3f scaleX(m_matrix[0], m_matrix[1], m_matrix[2]);
        Vector3f scaleY(m_matrix[4], m_matrix[5], m_matrix[6]);
        Vector3f scaleZ(m_matrix[8], m_matrix[9], m_matrix[10]);

        return Vector3f(scaleX.length(), scaleY.length(), scaleZ.length());
    }

    Vector3f extractRotationEuler() const {
        // Extract scale first to normalize the rotation matrix
        Vector3f scale = extractScale();

        // Normalize the rotation matrix by dividing by scale
        float m00 = m_matrix[0] / scale.getX();
        float m01 = m_matrix[4] / scale.getY();
        float m02 = m_matrix[8] / scale.getZ();
        float m10 = m_matrix[1] / scale.getX();
        float m11 = m_matrix[5] / scale.getY();
        float m12 = m_matrix[9] / scale.getZ();
        float m20 = m_matrix[2] / scale.getX();
        float m21 = m_matrix[6] / scale.getY();
        float m22 = m_matrix[10] / scale.getZ();

        // Extract Euler angles from rotation matrix (ZYX order)
        float rotX, rotY, rotZ;

        if (m20 < 0.99999f && m20 > -0.99999f) {
            rotY = std::asin(-m20);
            rotX = std::atan2(m21, m22);
            rotZ = std::atan2(m10, m00);
        }
        else {
            // Gimbal lock case
            rotZ = 0;
            if (m20 < 0) {
                rotY = static_cast<float>(M_PI) / 2.0f;
                rotX = std::atan2(m01, m11);
            }
            else {
                rotY = -static_cast<float>(M_PI) / 2.0f;
                rotX = std::atan2(-m01, m11);
            }
        }

        // Convert from radians to degrees
        const float RAD_TO_DEG = 180.0f / static_cast<float>(M_PI);
        return Vector3f(rotX * RAD_TO_DEG, rotY * RAD_TO_DEG, rotZ * RAD_TO_DEG);
    }

    // Decompose the matrix into translation, rotation (in degrees), and scale
    void decompose(Vector3f& translation, Vector3f& rotationDegrees, Vector3f& scale) const {
        translation = extractTranslation();
        scale = extractScale();
        rotationDegrees = extractRotationEuler();
    }
};

// Non-member operators
inline Matrix4f operator*(float scalar, const Matrix4f& matrix) {
    return matrix * scalar;
}