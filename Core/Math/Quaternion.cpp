#include "Quaternion.h"
#include "Matrix4f.h"

// ── ToMatrix ──────────────────────────────────────────────────────────────────

Matrix4f Quaternion::ToMatrix() const
{
    Quaternion q = Normalized();

    float xx = q.x * q.x;
    float xy = q.x * q.y;
    float xz = q.x * q.z;
    float xw = q.x * q.w;
    float yy = q.y * q.y;
    float yz = q.y * q.z;
    float yw = q.y * q.w;
    float zz = q.z * q.z;
    float zw = q.z * q.w;

    Matrix4f result;
    result(0, 0) = 1.0f - 2.0f * (yy + zz);
    result(0, 1) = 2.0f * (xy - zw);
    result(0, 2) = 2.0f * (xz + yw);
    result(0, 3) = 0.0f;

    result(1, 0) = 2.0f * (xy + zw);
    result(1, 1) = 1.0f - 2.0f * (xx + zz);
    result(1, 2) = 2.0f * (yz - xw);
    result(1, 3) = 0.0f;

    result(2, 0) = 2.0f * (xz - yw);
    result(2, 1) = 2.0f * (yz + xw);
    result(2, 2) = 1.0f - 2.0f * (xx + yy);
    result(2, 3) = 0.0f;

    result(3, 0) = 0.0f;
    result(3, 1) = 0.0f;
    result(3, 2) = 0.0f;
    result(3, 3) = 1.0f;

    return result;
}

// ── FromMatrix ────────────────────────────────────────────────────────────────

Quaternion Quaternion::FromMatrix(const Matrix4f& matrix)
{
    float m00 = matrix(0, 0), m01 = matrix(0, 1), m02 = matrix(0, 2);
    float m10 = matrix(1, 0), m11 = matrix(1, 1), m12 = matrix(1, 2);
    float m20 = matrix(2, 0), m21 = matrix(2, 1), m22 = matrix(2, 2);

    float     trace = m00 + m11 + m22;
    Quaternion result;

    if (trace > 0)
    {
        float s  = std::sqrt(trace + 1.0f) * 2;
        result.w = 0.25f * s;
        result.x = (m21 - m12) / s;
        result.y = (m02 - m20) / s;
        result.z = (m10 - m01) / s;
    }
    else if ((m00 > m11) && (m00 > m22))
    {
        float s  = std::sqrt(1.0f + m00 - m11 - m22) * 2;
        result.w = (m21 - m12) / s;
        result.x = 0.25f * s;
        result.y = (m01 + m10) / s;
        result.z = (m02 + m20) / s;
    }
    else if (m11 > m22)
    {
        float s  = std::sqrt(1.0f + m11 - m00 - m22) * 2;
        result.w = (m02 - m20) / s;
        result.x = (m01 + m10) / s;
        result.y = 0.25f * s;
        result.z = (m12 + m21) / s;
    }
    else
    {
        float s  = std::sqrt(1.0f + m22 - m00 - m11) * 2;
        result.w = (m10 - m01) / s;
        result.x = (m02 + m20) / s;
        result.y = (m12 + m21) / s;
        result.z = 0.25f * s;
    }

    return result;
}
