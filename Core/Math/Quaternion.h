#pragma once

#include "Vector3f.h"
#include "Vector4f.h"

#include <cmath>
#include <iostream>
#include <algorithm>
#include <corecrt_math_defines.h>

// Forward declaration
class Matrix4f;

/** @brief Unit quaternion representing a 3D rotation. */
class Quaternion
{
private:
    float w, x, y, z;  // w is the scalar part, (x, y, z) is the vector part

public:
    // ── Constructors ─────────────────────────────────────────────────────────

    Quaternion() : w(1.0f), x(0.0f), y(0.0f), z(0.0f) {}
    Quaternion(float w, float x, float y, float z) : w(w), x(x), y(y), z(z) {}
    Quaternion(const Quaternion& other) : w(other.w), x(other.x), y(other.y), z(other.z) {}

    Quaternion(const Vector3f& axis, float angleRad)
    {
        FromAxisAngle(axis, angleRad);
    }

    Quaternion(float pitch, float yaw, float roll)
    {
        FromEulerAngles(pitch, yaw, roll);
    }

    // ── Accessors ────────────────────────────────────────────────────────────

    float GetW() const { return w; }
    float GetX() const { return x; }
    float GetY() const { return y; }
    float GetZ() const { return z; }

    void SetW(float w) { this->w = w; }
    void SetX(float x) { this->x = x; }
    void SetY(float y) { this->y = y; }
    void SetZ(float z) { this->z = z; }
    void Set(float w, float x, float y, float z) { this->w = w; this->x = x; this->y = y; this->z = z; }

    // ── Assignment ───────────────────────────────────────────────────────────

    Quaternion& operator=(const Quaternion& other)
    {
        if (this != &other)
        {
            w = other.w;
            x = other.x;
            y = other.y;
            z = other.z;
        }
        return *this;
    }

    // ── Arithmetic Operators ─────────────────────────────────────────────────

    Quaternion operator+(const Quaternion& other) const
    {
        return Quaternion(w + other.w, x + other.x, y + other.y, z + other.z);
    }

    Quaternion& operator+=(const Quaternion& other)
    {
        w += other.w; x += other.x; y += other.y; z += other.z;
        return *this;
    }

    Quaternion operator-(const Quaternion& other) const
    {
        return Quaternion(w - other.w, x - other.x, y - other.y, z - other.z);
    }

    Quaternion& operator-=(const Quaternion& other)
    {
        w -= other.w; x -= other.x; y -= other.y; z -= other.z;
        return *this;
    }

    Quaternion operator-() const { return Quaternion(-w, -x, -y, -z); }

    Quaternion operator*(const Quaternion& other) const
    {
        return Quaternion(
            w * other.w - x * other.x - y * other.y - z * other.z,
            w * other.x + x * other.w + y * other.z - z * other.y,
            w * other.y - x * other.z + y * other.w + z * other.x,
            w * other.z + x * other.y - y * other.x + z * other.w
        );
    }

    Quaternion& operator*=(const Quaternion& other) { *this = *this * other; return *this; }

    Quaternion operator*(float scalar) const
    {
        return Quaternion(w * scalar, x * scalar, y * scalar, z * scalar);
    }

    Quaternion& operator*=(float scalar)
    {
        w *= scalar; x *= scalar; y *= scalar; z *= scalar;
        return *this;
    }

    Quaternion operator/(float scalar) const
    {
        if (scalar == 0)
            throw std::invalid_argument("Division by zero");
        return Quaternion(w / scalar, x / scalar, y / scalar, z / scalar);
    }

    Quaternion& operator/=(float scalar)
    {
        if (scalar == 0)
            throw std::invalid_argument("Division by zero");
        w /= scalar; x /= scalar; y /= scalar; z /= scalar;
        return *this;
    }

    // ── Comparison ───────────────────────────────────────────────────────────

    bool operator==(const Quaternion& other) const
    {
        const float epsilon = 1e-6f;
        return std::abs(w - other.w) < epsilon &&
               std::abs(x - other.x) < epsilon &&
               std::abs(y - other.y) < epsilon &&
               std::abs(z - other.z) < epsilon;
    }

    bool operator!=(const Quaternion& other) const { return !(*this == other); }

    // ── Math Operations ───────────────────────────────────────────────────────

    float Dot(const Quaternion& other) const
    {
        return w * other.w + x * other.x + y * other.y + z * other.z;
    }

    float Magnitude() const
    {
        return std::sqrt(w * w + x * x + y * y + z * z);
    }

    float MagnitudeSquared() const
    {
        return w * w + x * x + y * y + z * z;
    }

    Quaternion& Normalize()
    {
        float mag = Magnitude();
        if (mag == 0)
            throw std::runtime_error("Cannot normalize zero quaternion");
        w /= mag; x /= mag; y /= mag; z /= mag;
        return *this;
    }

    Quaternion Normalized() const
    {
        float mag = Magnitude();
        if (mag == 0)
            throw std::runtime_error("Cannot normalize zero quaternion");
        return Quaternion(w / mag, x / mag, y / mag, z / mag);
    }

    Quaternion Conjugate() const { return Quaternion(w, -x, -y, -z); }

    Quaternion Inverse() const
    {
        float magSq = MagnitudeSquared();
        if (magSq == 0)
            throw std::runtime_error("Cannot invert zero quaternion");
        return Conjugate() / magSq;
    }

    bool IsUnit() const
    {
        const float epsilon = 1e-6f;
        return std::abs(Magnitude() - 1.0f) < epsilon;
    }

    bool IsZero() const
    {
        const float epsilon = 1e-6f;
        return Magnitude() < epsilon;
    }

    // ── Conversion ────────────────────────────────────────────────────────────

    void FromAxisAngle(const Vector3f& axis, float angleRad)
    {
        Vector3f normalizedAxis = axis.normalized();
        float    halfAngle      = angleRad * 0.5f;
        float    sinHalf        = std::sin(halfAngle);
        w = std::cos(halfAngle);
        x = normalizedAxis.GetX() * sinHalf;
        y = normalizedAxis.GetY() * sinHalf;
        z = normalizedAxis.GetZ() * sinHalf;
    }

    void ToAxisAngle(Vector3f& axis, float& angleRad) const
    {
        Quaternion q    = Normalized();
        angleRad        = 2.0f * std::acos(std::abs(q.w));
        float sinHalf   = std::sqrt(1.0f - q.w * q.w);
        if (sinHalf < 1e-6f)
        {
            axis = Vector3f(1, 0, 0);
        }
        else
        {
            axis.set(q.x / sinHalf, q.y / sinHalf, q.z / sinHalf);
        }
    }

    void FromEulerAngles(float pitch, float yaw, float roll)
    {
        float cy = std::cos(yaw   * 0.5f);
        float sy = std::sin(yaw   * 0.5f);
        float cp = std::cos(pitch * 0.5f);
        float sp = std::sin(pitch * 0.5f);
        float cr = std::cos(roll  * 0.5f);
        float sr = std::sin(roll  * 0.5f);

        w = cr * cp * cy + sr * sp * sy;
        x = sr * cp * cy - cr * sp * sy;
        y = cr * sp * cy + sr * cp * sy;
        z = cr * cp * sy - sr * sp * cy;
    }

    Vector3f ToEulerAngles() const
    {
        Vector3f angles;

        float sinr_cosp = 2.0f * (w * x + y * z);
        float cosr_cosp = 1.0f - 2.0f * (x * x + y * y);
        angles.setX(std::atan2(sinr_cosp, cosr_cosp));

        float sinp = 2.0f * (w * y - z * x);
        if (std::abs(sinp) >= 1)
            angles.setZ(std::copysign(static_cast<float>(M_PI) / 2.0f, sinp));
        else
            angles.setZ(std::asin(sinp));

        float siny_cosp = 2.0f * (w * z + x * y);
        float cosy_cosp = 1.0f - 2.0f * (y * y + z * z);
        angles.setY(std::atan2(siny_cosp, cosy_cosp));

        return angles;
    }

    // ── Vector Rotation ───────────────────────────────────────────────────────

    Vector3f RotateVector(const Vector3f& vector) const
    {
        Vector3f qvec(x, y, z);
        Vector3f uv  = qvec.cross(vector);
        Vector3f uuv = qvec.cross(uv);
        return vector + ((uv * w) + uuv) * 2.0f;
    }

    Vector3f operator*(const Vector3f& vector) const { return RotateVector(vector); }

    // ── Interpolation ─────────────────────────────────────────────────────────

    static Quaternion Slerp(const Quaternion& a, const Quaternion& b, float t)
    {
        Quaternion qa = a.Normalized();
        Quaternion qb = b.Normalized();

        float dotVal = qa.Dot(qb);

        if (dotVal < 0.0f)
        {
            qb     = -qb;
            dotVal = -dotVal;
        }

        const float DOT_THRESHOLD = 0.9995f;
        if (dotVal > DOT_THRESHOLD)
        {
            Quaternion result = qa + (qb - qa) * t;
            return result.Normalized();
        }

        float theta0 = std::acos(dotVal);
        float theta  = theta0 * t;

        Quaternion q2 = qb - qa * dotVal;
        q2.Normalize();

        return qa * std::cos(theta) + q2 * std::sin(theta);
    }

    static Quaternion Nlerp(const Quaternion& a, const Quaternion& b, float t)
    {
        Quaternion qa = a.Normalized();
        Quaternion qb = b.Normalized();

        if (qa.Dot(qb) < 0.0f)
            qb = -qb;

        return (qa + (qb - qa) * t).Normalized();
    }

    // ── Matrix Conversion ─────────────────────────────────────────────────────

    Matrix4f ToMatrix() const;
    static Quaternion FromMatrix(const Matrix4f& matrix);

    // ── Static Factory Methods ────────────────────────────────────────────────

    static Quaternion Identity()                  { return Quaternion(1.0f, 0.0f, 0.0f, 0.0f); }
    static Quaternion RotationX(float angleRad)   { return Quaternion(Vector3f(1, 0, 0), angleRad); }
    static Quaternion RotationY(float angleRad)   { return Quaternion(Vector3f(0, 1, 0), angleRad); }
    static Quaternion RotationZ(float angleRad)   { return Quaternion(Vector3f(0, 0, 1), angleRad); }

    static Quaternion FromToRotation(const Vector3f& from, const Vector3f& to)
    {
        Vector3f fromNorm = from.normalized();
        Vector3f toNorm   = to.normalized();

        float dotVal = fromNorm.dot(toNorm);

        if (dotVal >= 1.0f - 1e-6f)
            return Identity();

        if (dotVal <= -1.0f + 1e-6f)
        {
            Vector3f axis = fromNorm.cross(Vector3f(1, 0, 0));
            if (axis.magnitude() < 1e-6f)
                axis = fromNorm.cross(Vector3f(0, 1, 0));
            axis.normalize();
            return Quaternion(axis, static_cast<float>(M_PI));
        }

        Vector3f axis = fromNorm.cross(toNorm);
        float    s    = std::sqrt((1.0f + dotVal) * 2.0f);
        float    invs = 1.0f / s;

        return Quaternion(s * 0.5f, axis.GetX() * invs, axis.GetY() * invs, axis.GetZ() * invs);
    }

    static Quaternion LookRotation(const Vector3f& forward, const Vector3f& up = Vector3f(0, 1, 0))
    {
        Vector3f forwardNorm = forward.normalized();
        Vector3f rightNorm   = up.cross(forwardNorm).normalized();
        Vector3f upNorm      = forwardNorm.cross(rightNorm);

        float m00 = rightNorm.GetX();
        float m01 = rightNorm.GetY();
        float m02 = rightNorm.GetZ();
        float m10 = upNorm.GetX();
        float m11 = upNorm.GetY();
        float m12 = upNorm.GetZ();
        float m20 = forwardNorm.GetX();
        float m21 = forwardNorm.GetY();
        float m22 = forwardNorm.GetZ();

        float     trace = m00 + m11 + m22;
        Quaternion q;

        if (trace > 0)
        {
            float s = std::sqrt(trace + 1.0f) * 2;
            q.w = 0.25f * s;
            q.x = (m21 - m12) / s;
            q.y = (m02 - m20) / s;
            q.z = (m10 - m01) / s;
        }
        else if ((m00 > m11) && (m00 > m22))
        {
            float s = std::sqrt(1.0f + m00 - m11 - m22) * 2;
            q.w = (m21 - m12) / s;
            q.x = 0.25f * s;
            q.y = (m01 + m10) / s;
            q.z = (m02 + m20) / s;
        }
        else if (m11 > m22)
        {
            float s = std::sqrt(1.0f + m11 - m00 - m22) * 2;
            q.w = (m02 - m20) / s;
            q.x = (m01 + m10) / s;
            q.y = 0.25f * s;
            q.z = (m12 + m21) / s;
        }
        else
        {
            float s = std::sqrt(1.0f + m22 - m00 - m11) * 2;
            q.w = (m10 - m01) / s;
            q.x = (m02 + m20) / s;
            q.y = (m12 + m21) / s;
            q.z = 0.25f * s;
        }

        return q;
    }

    // ── Direction Helpers ─────────────────────────────────────────────────────

    Vector3f GetForward() const { return RotateVector(Vector3f(0, 0, 1)); }
    Vector3f GetRight()   const { return RotateVector(Vector3f(1, 0, 0)); }
    Vector3f GetUp()      const { return RotateVector(Vector3f(0, 1, 0)); }

    // ── Stream Output ─────────────────────────────────────────────────────────

    friend std::ostream& operator<<(std::ostream& os, const Quaternion& q)
    {
        os << "(" << q.w << ", " << q.x << ", " << q.y << ", " << q.z << ")";
        return os;
    }
};

inline Quaternion operator*(float scalar, const Quaternion& quaternion)
{
    return quaternion * scalar;
}
