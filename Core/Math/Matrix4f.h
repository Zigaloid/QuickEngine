#pragma once

#include "Vector3f.h"

#include <cmath>
#include <array>
#include <iostream>
#include <vector>
#include <tuple>
#include <corecrt_math_defines.h>

/** @brief Column-major 4x4 float matrix for 3D transformations. */
class Matrix4f
{
private:
    // Column-major storage (OpenGL compatible):
    // m_matrix[0-3] = column 0, m_matrix[4-7] = column 1, etc.
    std::array<float, 16> m_matrix;

public:
    // ── Constructors ─────────────────────────────────────────────────────────

    Matrix4f()
    {
        Identity();
    }

    explicit Matrix4f(const std::array<float, 16>& data) : m_matrix(data) {}

    Matrix4f(float m00, float m01, float m02, float m03,
             float m10, float m11, float m12, float m13,
             float m20, float m21, float m22, float m23,
             float m30, float m31, float m32, float m33)
    {
        m_matrix[0]  = m00; m_matrix[4]  = m01; m_matrix[8]  = m02; m_matrix[12] = m03;
        m_matrix[1]  = m10; m_matrix[5]  = m11; m_matrix[9]  = m12; m_matrix[13] = m13;
        m_matrix[2]  = m20; m_matrix[6]  = m21; m_matrix[10] = m22; m_matrix[14] = m23;
        m_matrix[3]  = m30; m_matrix[7]  = m31; m_matrix[11] = m32; m_matrix[15] = m33;
    }

    // ── Data Access ──────────────────────────────────────────────────────────

    void SetColumn(int col, const Vector3f& v)
    {
        m_matrix[col * 4 + 0] = v.x;
        m_matrix[col * 4 + 1] = v.y;
        m_matrix[col * 4 + 2] = v.z;
    }

    /// Returns the xyz components of a matrix column (ignores the w row).
    Vector3f GetColumn(int col) const
    {
        return Vector3f(m_matrix[col * 4 + 0],
                        m_matrix[col * 4 + 1],
                        m_matrix[col * 4 + 2]);
    }

    /// Overwrites only the translation components (column 3, rows 0-2).
    void SetTranslation(const Vector3f& t)
    {
        m_matrix[12] = t.x;
        m_matrix[13] = t.y;
        m_matrix[14] = t.z;
    }

    const auto& GetData()      const { return m_matrix; }
    auto&       GetWriteData()       { return m_matrix; }

    float& operator()(int row, int col)             { return m_matrix[col * 4 + row]; }
    const float& operator()(int row, int col) const { return m_matrix[col * 4 + row]; }

    float*       data()       { return m_matrix.data(); }
    const float* data() const { return m_matrix.data(); }

    // ── Basic Operations ─────────────────────────────────────────────────────

    void Identity()
    {
        m_matrix.fill(0.0f);
        m_matrix[0] = m_matrix[5] = m_matrix[10] = m_matrix[15] = 1.0f;
    }
    void identity() { Identity(); } // backward compat

    static Matrix4f GetIdentity()
    {
        Matrix4f result;
        result.Identity();
        return result;
    }

    void Zero()  { m_matrix.fill(0.0f); }
    void zero()  { Zero(); } // backward compat

    // ── Comparison Operators ─────────────────────────────────────────────────

    bool operator==(const Matrix4f& other) const
    {
        return m_matrix == other.m_matrix;
    }

    bool operator!=(const Matrix4f& other) const
    {
        return m_matrix != other.m_matrix;
    }

    Matrix4f Transpose() const
    {
        Matrix4f result;
        for (int i = 0; i < 4; ++i)
        {
            for (int j = 0; j < 4; ++j)
                result(i, j) = (*this)(j, i);
        }
        return result;
    }

    float Determinant() const
    {
        float a = m_matrix[0],  b = m_matrix[4],  c = m_matrix[8],  d = m_matrix[12];
        float e = m_matrix[1],  f = m_matrix[5],  g = m_matrix[9],  h = m_matrix[13];
        float i = m_matrix[2],  j = m_matrix[6],  k = m_matrix[10], l = m_matrix[14];
        float m = m_matrix[3],  n = m_matrix[7],  o = m_matrix[11], p = m_matrix[15];

        return a * (f * (k * p - l * o) - g * (j * p - l * n) + h * (j * o - k * n)) -
               b * (e * (k * p - l * o) - g * (i * p - l * m) + h * (i * o - k * m)) +
               c * (e * (j * p - l * n) - f * (i * p - l * m) + h * (i * n - j * m)) -
               d * (e * (j * o - k * n) - f * (i * o - k * m) + g * (i * n - j * m));
    }
    float determinant() const { return Determinant(); } // backward compat

    Matrix4f Inverse() const
    {
        Matrix4f result;
        float    det = Determinant();

        if (std::abs(det) < 1e-6f)
        {
            result.Identity();
            return result;
        }

        float invDet = 1.0f / det;

        result.m_matrix[0]  = invDet * (m_matrix[5]  * (m_matrix[10] * m_matrix[15] - m_matrix[11] * m_matrix[14]) - m_matrix[9]  * (m_matrix[6]  * m_matrix[15] - m_matrix[7]  * m_matrix[14]) + m_matrix[13] * (m_matrix[6]  * m_matrix[11] - m_matrix[7]  * m_matrix[10]));
        result.m_matrix[1]  = invDet * -(m_matrix[1] * (m_matrix[10] * m_matrix[15] - m_matrix[11] * m_matrix[14]) - m_matrix[9]  * (m_matrix[2]  * m_matrix[15] - m_matrix[3]  * m_matrix[14]) + m_matrix[13] * (m_matrix[2]  * m_matrix[11] - m_matrix[3]  * m_matrix[10]));
        result.m_matrix[2]  = invDet * (m_matrix[1]  * (m_matrix[6]  * m_matrix[15] - m_matrix[7]  * m_matrix[14]) - m_matrix[5]  * (m_matrix[2]  * m_matrix[15] - m_matrix[3]  * m_matrix[14]) + m_matrix[13] * (m_matrix[2]  * m_matrix[7]  - m_matrix[3]  * m_matrix[6]));
        result.m_matrix[3]  = invDet * -(m_matrix[1] * (m_matrix[6]  * m_matrix[11] - m_matrix[7]  * m_matrix[10]) - m_matrix[5]  * (m_matrix[2]  * m_matrix[11] - m_matrix[3]  * m_matrix[10]) + m_matrix[9]  * (m_matrix[2]  * m_matrix[7]  - m_matrix[3]  * m_matrix[6]));

        result.m_matrix[4]  = invDet * -(m_matrix[4] * (m_matrix[10] * m_matrix[15] - m_matrix[11] * m_matrix[14]) - m_matrix[8]  * (m_matrix[6]  * m_matrix[15] - m_matrix[7]  * m_matrix[14]) + m_matrix[12] * (m_matrix[6]  * m_matrix[11] - m_matrix[7]  * m_matrix[10]));
        result.m_matrix[5]  = invDet * (m_matrix[0]  * (m_matrix[10] * m_matrix[15] - m_matrix[11] * m_matrix[14]) - m_matrix[8]  * (m_matrix[2]  * m_matrix[15] - m_matrix[3]  * m_matrix[14]) + m_matrix[12] * (m_matrix[2]  * m_matrix[11] - m_matrix[3]  * m_matrix[10]));
        result.m_matrix[6]  = invDet * -(m_matrix[0] * (m_matrix[6]  * m_matrix[15] - m_matrix[7]  * m_matrix[14]) - m_matrix[4]  * (m_matrix[2]  * m_matrix[15] - m_matrix[3]  * m_matrix[14]) + m_matrix[12] * (m_matrix[2]  * m_matrix[7]  - m_matrix[3]  * m_matrix[6]));
        result.m_matrix[7]  = invDet * (m_matrix[0]  * (m_matrix[6]  * m_matrix[11] - m_matrix[7]  * m_matrix[10]) - m_matrix[4]  * (m_matrix[2]  * m_matrix[11] - m_matrix[3]  * m_matrix[10]) + m_matrix[8]  * (m_matrix[2]  * m_matrix[7]  - m_matrix[3]  * m_matrix[6]));

        result.m_matrix[8]  = invDet * (m_matrix[4]  * (m_matrix[9]  * m_matrix[15] - m_matrix[11] * m_matrix[13]) - m_matrix[8]  * (m_matrix[5]  * m_matrix[15] - m_matrix[7]  * m_matrix[13]) + m_matrix[12] * (m_matrix[5]  * m_matrix[11] - m_matrix[7]  * m_matrix[9]));
        result.m_matrix[9]  = invDet * -(m_matrix[0] * (m_matrix[9]  * m_matrix[15] - m_matrix[11] * m_matrix[13]) - m_matrix[8]  * (m_matrix[1]  * m_matrix[15] - m_matrix[3]  * m_matrix[13]) + m_matrix[12] * (m_matrix[1]  * m_matrix[11] - m_matrix[3]  * m_matrix[9]));
        result.m_matrix[10] = invDet * (m_matrix[0]  * (m_matrix[5]  * m_matrix[15] - m_matrix[7]  * m_matrix[13]) - m_matrix[4]  * (m_matrix[1]  * m_matrix[15] - m_matrix[3]  * m_matrix[13]) + m_matrix[12] * (m_matrix[1]  * m_matrix[7]  - m_matrix[3]  * m_matrix[5]));
        result.m_matrix[11] = invDet * -(m_matrix[0] * (m_matrix[5]  * m_matrix[11] - m_matrix[7]  * m_matrix[9])  - m_matrix[4]  * (m_matrix[1]  * m_matrix[11] - m_matrix[3]  * m_matrix[9])  + m_matrix[8]  * (m_matrix[1]  * m_matrix[7]  - m_matrix[3]  * m_matrix[5]));

        result.m_matrix[12] = invDet * -(m_matrix[4] * (m_matrix[9]  * m_matrix[14] - m_matrix[10] * m_matrix[13]) - m_matrix[8]  * (m_matrix[5]  * m_matrix[14] - m_matrix[6]  * m_matrix[13]) + m_matrix[12] * (m_matrix[5]  * m_matrix[10] - m_matrix[6]  * m_matrix[9]));
        result.m_matrix[13] = invDet * (m_matrix[0]  * (m_matrix[9]  * m_matrix[14] - m_matrix[10] * m_matrix[13]) - m_matrix[8]  * (m_matrix[1]  * m_matrix[14] - m_matrix[2]  * m_matrix[13]) + m_matrix[12] * (m_matrix[1]  * m_matrix[10] - m_matrix[2]  * m_matrix[9]));
        result.m_matrix[14] = invDet * -(m_matrix[0] * (m_matrix[5]  * m_matrix[14] - m_matrix[6]  * m_matrix[13]) - m_matrix[4]  * (m_matrix[1]  * m_matrix[14] - m_matrix[2]  * m_matrix[13]) + m_matrix[12] * (m_matrix[1]  * m_matrix[6]  - m_matrix[2]  * m_matrix[5]));
        result.m_matrix[15] = invDet * (m_matrix[0]  * (m_matrix[5]  * m_matrix[10] - m_matrix[6]  * m_matrix[9])  - m_matrix[4]  * (m_matrix[1]  * m_matrix[10] - m_matrix[2]  * m_matrix[9])  + m_matrix[8]  * (m_matrix[1]  * m_matrix[6]  - m_matrix[2]  * m_matrix[5]));

        return result;
    }

    // ── Arithmetic Operators ─────────────────────────────────────────────────

    Matrix4f operator+(const Matrix4f& other) const
    {
        Matrix4f result;
        for (int i = 0; i < 16; ++i) result.m_matrix[i] = m_matrix[i] + other.m_matrix[i];
        return result;
    }

    Matrix4f operator-(const Matrix4f& other) const
    {
        Matrix4f result;
        for (int i = 0; i < 16; ++i) result.m_matrix[i] = m_matrix[i] - other.m_matrix[i];
        return result;
    }

    Matrix4f operator*(const Matrix4f& other) const
    {
        Matrix4f result;
        result.Zero();
        for (int col = 0; col < 4; ++col)
        {
            for (int row = 0; row < 4; ++row)
            {
                for (int k = 0; k < 4; ++k)
                    result(row, col) += (*this)(row, k) * other(k, col);
            }
        }
        return result;
    }

    Matrix4f operator*(float scalar) const
    {
        Matrix4f result;
        for (int i = 0; i < 16; ++i) result.m_matrix[i] = m_matrix[i] * scalar;
        return result;
    }

    Matrix4f& operator+=(const Matrix4f& other) { for (int i = 0; i < 16; ++i) m_matrix[i] += other.m_matrix[i]; return *this; }
    Matrix4f& operator-=(const Matrix4f& other) { for (int i = 0; i < 16; ++i) m_matrix[i] -= other.m_matrix[i]; return *this; }
    Matrix4f& operator*=(const Matrix4f& other) { *this = *this * other; return *this; }
    Matrix4f& operator*=(float scalar)           { for (int i = 0; i < 16; ++i) m_matrix[i] *= scalar; return *this; }

    // ── Static Transformation Factories ──────────────────────────────────────

    static Matrix4f Translation(float x, float y, float z)
    {
        Matrix4f result;
        result.Identity();
        result(0, 3) = x;
        result(1, 3) = y;
        result(2, 3) = z;
        return result;
    }

    static Matrix4f Translation(const Vector3f& v) { return Translation(v.GetX(), v.GetY(), v.GetZ()); }
    // Backward compat
    static Matrix4f translation(float x, float y, float z) { return Translation(x, y, z); }
    static Matrix4f translation(const Vector3f& v)          { return Translation(v); }

    static Matrix4f Scale(float x, float y, float z)
    {
        Matrix4f result;
        result.Zero();
        result(0, 0) = x;
        result(1, 1) = y;
        result(2, 2) = z;
        result(3, 3) = 1.0f;
        return result;
    }

    static Matrix4f Scale(const Vector3f& v) { return Scale(v.GetX(), v.GetY(), v.GetZ()); }
    static Matrix4f scale(float x, float y, float z) { return Scale(x, y, z); }  // backward compat
    static Matrix4f scale(const Vector3f& v)          { return Scale(v); }        // backward compat

    static Matrix4f RotationX(float angleRad)
    {
        Matrix4f result;
        result.Identity();
        float c = std::cos(angleRad);
        float s = std::sin(angleRad);
        result(1, 1) =  c;
        result(1, 2) = -s;
        result(2, 1) =  s;
        result(2, 2) =  c;
        return result;
    }
    static Matrix4f rotationX(float angleRad) { return RotationX(angleRad); } // backward compat

    static Matrix4f RotationY(float angleRad)
    {
        Matrix4f result;
        result.Identity();
        float c = std::cos(angleRad);
        float s = std::sin(angleRad);
        result(0, 0) =  c;
        result(0, 2) =  s;
        result(2, 0) = -s;
        result(2, 2) =  c;
        return result;
    }
    static Matrix4f rotationY(float angleRad) { return RotationY(angleRad); } // backward compat

    static Matrix4f RotationZ(float angleRad)
    {
        Matrix4f result;
        result.Identity();
        float c = std::cos(angleRad);
        float s = std::sin(angleRad);
        result(0, 0) =  c;
        result(0, 1) = -s;
        result(1, 0) =  s;
        result(1, 1) =  c;
        return result;
    }
    static Matrix4f rotationZ(float angleRad) { return RotationZ(angleRad); } // backward compat

    static Matrix4f Rotation(float x, float y, float z, float angleRad)
    {
        float c         = std::cos(angleRad);
        float s         = std::sin(angleRad);
        float oneMinusC = 1.0f - c;

        float len = std::sqrt(x * x + y * y + z * z);
        if (len < 1e-6f)
        {
            Matrix4f result;
            result.Identity();
            return result;
        }
        x /= len; y /= len; z /= len;

        Matrix4f result;
        result.Identity();

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

    static Matrix4f Rotation(const Vector3f& axis, float angleRad)
    {
        return Rotation(axis.GetX(), axis.GetY(), axis.GetZ(), angleRad);
    }
    static Matrix4f rotation(float x, float y, float z, float a) { return Rotation(x, y, z, a); } // backward compat
    static Matrix4f rotation(const Vector3f& axis, float a)       { return Rotation(axis, a); }    // backward compat

    // ── TRS ──────────────────────────────────────────────────────────────────

    /** @brief Build a TRS matrix from Vector3f components.
     *  rotationDegrees uses Euler angles in degrees (X, Y, Z), ZYX order. */
    static Matrix4f FromTRS(const Vector3f& translation, const Vector3f& rotationDegrees, const Vector3f& scaleVec)
    {
        const float degToRad = static_cast<float>(M_PI) / 180.0f;
        float rx = rotationDegrees.GetX() * degToRad;
        float ry = rotationDegrees.GetY() * degToRad;
        float rz = rotationDegrees.GetZ() * degToRad;

        Matrix4f S  = Matrix4f::Scale(scaleVec);
        Matrix4f Rx = Matrix4f::RotationX(rx);
        Matrix4f Ry = Matrix4f::RotationY(ry);
        Matrix4f Rz = Matrix4f::RotationZ(rz);
        Matrix4f R  = Rz * Ry * Rx;
        Matrix4f T  = Matrix4f::Translation(translation);

        return T * R * S;
    }
    static Matrix4f fromTRS(const Vector3f& t, const Vector3f& r, const Vector3f& s) { return FromTRS(t, r, s); } // backward compat

    void ToTRS(Vector3f& outTranslation, Vector3f& outRotationDegrees, Vector3f& outScale) const
    {
        Decompose(outTranslation, outRotationDegrees, outScale);
    }
    void toTRS(Vector3f& t, Vector3f& r, Vector3f& s) const { ToTRS(t, r, s); } // backward compat

    std::tuple<Vector3f, Vector3f, Vector3f> ToTRS() const
    {
        Vector3f t, r, s;
        Decompose(t, r, s);
        return std::tuple{ t, r, s };
    }

    // ── Camera / View ─────────────────────────────────────────────────────────

    static Matrix4f LookAt(float eyeX, float eyeY, float eyeZ,
                            float centerX, float centerY, float centerZ,
                            float upX, float upY, float upZ)
    {
        float fx = centerX - eyeX;
        float fy = centerY - eyeY;
        float fz = centerZ - eyeZ;

        float flen = std::sqrt(fx * fx + fy * fy + fz * fz);
        fx /= flen; fy /= flen; fz /= flen;

        float rx = fy * upZ - fz * upY;
        float ry = fz * upX - fx * upZ;
        float rz = fx * upY - fy * upX;

        float rlen = std::sqrt(rx * rx + ry * ry + rz * rz);
        rx /= rlen; ry /= rlen; rz /= rlen;

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

    static Matrix4f LookAt(const Vector3f& eye, const Vector3f& center, const Vector3f& up)
    {
        return LookAt(eye.GetX(), eye.GetY(), eye.GetZ(),
                      center.GetX(), center.GetY(), center.GetZ(),
                      up.GetX(), up.GetY(), up.GetZ());
    }
    static Matrix4f lookAt(float ex, float ey, float ez, float cx, float cy, float cz, float ux, float uy, float uz)
    {
        return LookAt(ex, ey, ez, cx, cy, cz, ux, uy, uz);
    }
    static Matrix4f lookAt(const Vector3f& e, const Vector3f& c, const Vector3f& u) { return LookAt(e, c, u); }

    /// Left-handed look-at matching bx::mtxLookAt default convention,
    /// for use with bgfx view matrices.
    static Matrix4f LookAtLH(const Vector3f& eye, const Vector3f& center, const Vector3f& up)
    {
        const Vector3f f = (center - eye).Normalized();          // +Z forward (left-handed)
        const Vector3f r = up.Cross(f).Normalized();             // right
        const Vector3f u = f.Cross(r);                           // reorthogonalised up

        Matrix4f result;
        result(0, 0) = r.x;  result(0, 1) = r.y;  result(0, 2) = r.z;  result(0, 3) = -r.Dot(eye);
        result(1, 0) = u.x;  result(1, 1) = u.y;  result(1, 2) = u.z;  result(1, 3) = -u.Dot(eye);
        result(2, 0) = f.x;  result(2, 1) = f.y;  result(2, 2) = f.z;  result(2, 3) = -f.Dot(eye);
        result(3, 0) = 0.0f; result(3, 1) = 0.0f; result(3, 2) = 0.0f; result(3, 3) =  1.0f;
        return result;
    }

    // ── Projection ────────────────────────────────────────────────────────────

    static Matrix4f Perspective(float fovYRad, float aspect, float nearPlane, float farPlane)
    {
        Matrix4f result;
        result.Zero();
        float f = 1.0f / std::tan(fovYRad * 0.5f);
        result(0, 0) = f / aspect;
        result(1, 1) = f;
        result(2, 2) = (farPlane + nearPlane) / (nearPlane - farPlane);
        result(2, 3) = (2.0f * farPlane * nearPlane) / (nearPlane - farPlane);
        result(3, 2) = -1.0f;
        return result;
    }
    static Matrix4f perspective(float f, float a, float n, float far_) { return Perspective(f, a, n, far_); } // backward compat

    static Matrix4f Orthographic(float left, float right, float bottom, float top, float nearPlane, float farPlane)
    {
        Matrix4f result;
        result.Zero();
        result(0, 0) = 2.0f / (right - left);
        result(1, 1) = 2.0f / (top - bottom);
        result(2, 2) = -2.0f / (farPlane - nearPlane);
        result(0, 3) = -(right + left) / (right - left);
        result(1, 3) = -(top + bottom) / (top - bottom);
        result(2, 3) = -(farPlane + nearPlane) / (farPlane - nearPlane);
        result(3, 3) = 1.0f;
        return result;
    }
    static Matrix4f orthographic(float l, float r, float b, float t, float n, float f) { return Orthographic(l, r, b, t, n, f); }

    // ── Vector Transformation ─────────────────────────────────────────────────

    void TransformPoint(float& x, float& y, float& z) const
    {
        float ww = 1.0f;
        float tx = x * m_matrix[0] + y * m_matrix[4] + z * m_matrix[8]  + ww * m_matrix[12];
        float ty = x * m_matrix[1] + y * m_matrix[5] + z * m_matrix[9]  + ww * m_matrix[13];
        float tz = x * m_matrix[2] + y * m_matrix[6] + z * m_matrix[10] + ww * m_matrix[14];
        float tw = x * m_matrix[3] + y * m_matrix[7] + z * m_matrix[11] + ww * m_matrix[15];

        if (std::abs(tw) > 1e-6f)
        {
            x = tx / tw;
            y = ty / tw;
            z = tz / tw;
        }
        else
        {
            x = tx; y = ty; z = tz;
        }
    }
    void transformPoint(float& x, float& y, float& z) const { TransformPoint(x, y, z); } // backward compat

    void TransformVector(float& x, float& y, float& z) const
    {
        float tx = x * m_matrix[0] + y * m_matrix[4] + z * m_matrix[8];
        float ty = x * m_matrix[1] + y * m_matrix[5] + z * m_matrix[9];
        float tz = x * m_matrix[2] + y * m_matrix[6] + z * m_matrix[10];
        x = tx; y = ty; z = tz;
    }
    void transformVector(float& x, float& y, float& z) const { TransformVector(x, y, z); } // backward compat

    Vector3f TransformPoint(const Vector3f& point) const
    {
        float x = point.GetX();
        float y = point.GetY();
        float z = point.GetZ();
        TransformPoint(x, y, z);
        return Vector3f(x, y, z);
    }
    Vector3f transformPoint(const Vector3f& p) const { return TransformPoint(p); } // backward compat

    Vector3f TransformVector(const Vector3f& vector) const
    {
        float x = vector.GetX();
        float y = vector.GetY();
        float z = vector.GetZ();
        TransformVector(x, y, z);
        return Vector3f(x, y, z);
    }
    Vector3f transformVector(const Vector3f& v) const { return TransformVector(v); } // backward compat

    void TransformPoints(std::vector<Vector3f>& points) const
    {
        for (Vector3f& point : points)
            point = TransformPoint(point);
    }

    void TransformPoints(Vector3f* points, size_t count) const
    {
        for (size_t i = 0; i < count; ++i)
            points[i] = TransformPoint(points[i]);
    }

    void TransformVectors(std::vector<Vector3f>& vectors) const
    {
        for (Vector3f& vector : vectors)
            vector = TransformVector(vector);
    }

    void TransformVectors(Vector3f* vectors, size_t count) const
    {
        for (size_t i = 0; i < count; ++i)
            vectors[i] = TransformVector(vectors[i]);
    }

    std::vector<Vector3f> GetTransformedPoints(const std::vector<Vector3f>& points) const
    {
        std::vector<Vector3f> result;
        result.reserve(points.size());
        for (const Vector3f& point : points)
            result.push_back(TransformPoint(point));
        return result;
    }

    std::vector<Vector3f> GetTransformedVectors(const std::vector<Vector3f>& vectors) const
    {
        std::vector<Vector3f> result;
        result.reserve(vectors.size());
        for (const Vector3f& vector : vectors)
            result.push_back(TransformVector(vector));
        return result;
    }

    Vector3f operator*(const Vector3f& point) const { return TransformPoint(point); }

    // ── Debug ─────────────────────────────────────────────────────────────────

    void Print() const
    {
        for (int row = 0; row < 4; ++row)
        {
            std::cout << "[";
            for (int col = 0; col < 4; ++col)
            {
                std::cout << (*this)(row, col);
                if (col < 3) std::cout << ", ";
            }
            std::cout << "]\n";
        }
    }
    void print() const { Print(); } // backward compat

    // ── Decomposition ─────────────────────────────────────────────────────────

    Vector3f ExtractTranslation() const
    {
        return Vector3f(m_matrix[12], m_matrix[13], m_matrix[14]);
    }

    Vector3f ExtractScale() const
    {
        Vector3f scaleX(m_matrix[0], m_matrix[1], m_matrix[2]);
        Vector3f scaleY(m_matrix[4], m_matrix[5], m_matrix[6]);
        Vector3f scaleZ(m_matrix[8], m_matrix[9], m_matrix[10]);
        return Vector3f(scaleX.Length(), scaleY.Length(), scaleZ.Length());
    }

    Vector3f ExtractRotationEuler() const
    {
        Vector3f scale = ExtractScale();

        float m00 = m_matrix[0]  / scale.GetX();
        float m01 = m_matrix[4]  / scale.GetY();
        float m02 = m_matrix[8]  / scale.GetZ();
        float m10 = m_matrix[1]  / scale.GetX();
        float m11 = m_matrix[5]  / scale.GetY();
        float m12 = m_matrix[9]  / scale.GetZ();
        float m20 = m_matrix[2]  / scale.GetX();
        float m21 = m_matrix[6]  / scale.GetY();
        float m22 = m_matrix[10] / scale.GetZ();

        float rotX = 0.0f;
        float rotY = 0.0f;
        float rotZ = 0.0f;

        if (m20 < 0.99999f && m20 > -0.99999f)
        {
            rotY = std::asin(-m20);
            rotX = std::atan2(m21, m22);
            rotZ = std::atan2(m10, m00);
        }
        else
        {
            rotZ = 0.0f;
            if (m20 < 0)
            {
                rotY = static_cast<float>(M_PI) / 2.0f;
                rotX = std::atan2(m01, m11);
            }
            else
            {
                rotY = -static_cast<float>(M_PI) / 2.0f;
                rotX = std::atan2(-m01, m11);
            }
        }

        const float radToDeg = 180.0f / static_cast<float>(M_PI);
        return Vector3f(rotX * radToDeg, rotY * radToDeg, rotZ * radToDeg);
    }

    void Decompose(Vector3f& outTranslation, Vector3f& outRotationDegrees, Vector3f& outScale) const
    {
        outTranslation   = ExtractTranslation();
        outScale         = ExtractScale();
        outRotationDegrees = ExtractRotationEuler();
    }
    void decompose(Vector3f& t, Vector3f& r, Vector3f& s) const { Decompose(t, r, s); } // backward compat
};

inline Matrix4f operator*(float scalar, const Matrix4f& matrix)
{
    return matrix * scalar;
}
