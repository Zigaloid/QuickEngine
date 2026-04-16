#pragma once

#include <cmath>
#include <iostream>
#include <algorithm>
#include <string>
#include <corecrt_math_defines.h>

/** @brief 3-component float vector for 3D math operations. */
class Vector3f
{
public:
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    // ── Constructors ─────────────────────────────────────────────────────────

    Vector3f() : x(0.0f), y(0.0f), z(0.0f) {}
    Vector3f(float x, float y, float z) : x(x), y(y), z(z) {}
    Vector3f(const Vector3f& other) : x(other.x), y(other.y), z(other.z) {}

    // ── Accessors ────────────────────────────────────────────────────────────

    float GetX() const { return x; }
    float GetY() const { return y; }
    float GetZ() const { return z; }

    // Keep lowercase aliases for backward compatibility (used in Quaternion/Matrix)
    float getX() const { return x; }
    float getY() const { return y; }
    float getZ() const { return z; }

    void SetX(float x) { this->x = x; }
    void SetY(float y) { this->y = y; }
    void SetZ(float z) { this->z = z; }
    void Set(float x, float y, float z) { this->x = x; this->y = y; this->z = z; }

    // Keep lowercase aliases for backward compatibility
    void setX(float x) { this->x = x; }
    void setY(float y) { this->y = y; }
    void setZ(float z) { this->z = z; }
    void set(float x, float y, float z) { this->x = x; this->y = y; this->z = z; }

    // ── Assignment ───────────────────────────────────────────────────────────

    Vector3f& operator=(const Vector3f& other)
    {
        if (this != &other)
        {
            x = other.x;
            y = other.y;
            z = other.z;
        }
        return *this;
    }

    // ── Arithmetic Operators ─────────────────────────────────────────────────

    Vector3f operator+(const Vector3f& other) const { return Vector3f(x + other.x, y + other.y, z + other.z); }
    Vector3f& operator+=(const Vector3f& other) { x += other.x; y += other.y; z += other.z; return *this; }

    Vector3f operator-(const Vector3f& other) const { return Vector3f(x - other.x, y - other.y, z - other.z); }
    Vector3f& operator-=(const Vector3f& other) { x -= other.x; y -= other.y; z -= other.z; return *this; }

    Vector3f operator-() const { return Vector3f(-x, -y, -z); }

    Vector3f operator*(float scalar) const { return Vector3f(x * scalar, y * scalar, z * scalar); }
    Vector3f& operator*=(float scalar) { x *= scalar; y *= scalar; z *= scalar; return *this; }

    Vector3f operator/(float scalar) const
    {
        if (scalar == 0)
            throw std::invalid_argument("Division by zero");
        return Vector3f(x / scalar, y / scalar, z / scalar);
    }

    Vector3f& operator/=(float scalar)
    {
        if (scalar == 0)
            throw std::invalid_argument("Division by zero");
        x /= scalar; y /= scalar; z /= scalar;
        return *this;
    }

    // ── Comparison ───────────────────────────────────────────────────────────

    bool operator==(const Vector3f& other) const
    {
        const float epsilon = 1e-6f;
        return std::abs(x - other.x) < epsilon &&
               std::abs(y - other.y) < epsilon &&
               std::abs(z - other.z) < epsilon;
    }

    bool operator!=(const Vector3f& other) const { return !(*this == other); }

    // ── Math ─────────────────────────────────────────────────────────────────

    float Dot(const Vector3f& other)   const { return x * other.x + y * other.y + z * other.z; }
    float dot(const Vector3f& other)   const { return Dot(other); }  // backward compat

    Vector3f Cross(const Vector3f& other) const
    {
        return Vector3f(
            y * other.z - z * other.y,
            z * other.x - x * other.z,
            x * other.y - y * other.x
        );
    }
    Vector3f cross(const Vector3f& other) const { return Cross(other); } // backward compat

    float Magnitude()        const { return std::sqrt(x * x + y * y + z * z); }
    float magnitude()        const { return Magnitude(); }  // backward compat
    float Length()           const { return Magnitude(); }
    float length()           const { return Magnitude(); }  // backward compat
    float MagnitudeSquared() const { return x * x + y * y + z * z; }
    float magnitudeSquared() const { return MagnitudeSquared(); } // backward compat

    Vector3f& Normalize()
    {
        float mag = Magnitude();
        if (mag == 0)
            throw std::runtime_error("Cannot normalize zero vector");
        x /= mag; y /= mag; z /= mag;
        return *this;
    }
    Vector3f& normalize() { return Normalize(); } // backward compat

    Vector3f Normalized() const
    {
        float mag = Magnitude();
        if (mag == 0)
            throw std::runtime_error("Cannot normalize zero vector");
        return Vector3f(x / mag, y / mag, z / mag);
    }
    Vector3f normalized() const { return Normalized(); } // backward compat

    float DistanceTo(const Vector3f& other)        const { return (*this - other).Magnitude(); }
    float distanceTo(const Vector3f& other)        const { return DistanceTo(other); } // backward compat
    float DistanceSquaredTo(const Vector3f& other) const { return (*this - other).MagnitudeSquared(); }
    float distanceSquaredTo(const Vector3f& other) const { return DistanceSquaredTo(other); } // backward compat

    float AngleTo(const Vector3f& other) const
    {
        float mag1 = Magnitude();
        float mag2 = other.Magnitude();
        if (mag1 == 0 || mag2 == 0)
            throw std::runtime_error("Cannot calculate angle with zero vector");
        float cosAngle = Dot(other) / (mag1 * mag2);
        if (cosAngle < -1.0f) cosAngle = -1.0f;
        else if (cosAngle > 1.0f) cosAngle = 1.0f;
        return std::acos(cosAngle);
    }

    Vector3f ProjectOnto(const Vector3f& other) const
    {
        float otherMagSq = other.MagnitudeSquared();
        if (otherMagSq == 0)
            throw std::runtime_error("Cannot project onto zero vector");
        return other * (Dot(other) / otherMagSq);
    }

    Vector3f Reflect(const Vector3f& normal) const
    {
        return *this - normal * (2.0f * Dot(normal));
    }

    bool IsZero() const
    {
        const float epsilon = 1e-6f;
        return Magnitude() < epsilon;
    }
    bool isZero() const { return IsZero(); } // backward compat

    bool IsUnit() const
    {
        const float epsilon = 1e-6f;
        return std::abs(Magnitude() - 1.0f) < epsilon;
    }

    // ── Static Factories ─────────────────────────────────────────────────────

    static Vector3f Lerp(const Vector3f& a, const Vector3f& b, float t) { return a + (b - a) * t; }
    static Vector3f lerp(const Vector3f& a, const Vector3f& b, float t) { return Lerp(a, b, t); } // backward compat

    static Vector3f Zero()    { return Vector3f(0, 0, 0); }
    static Vector3f One()     { return Vector3f(1, 1, 1); }
    static Vector3f Right()   { return Vector3f(1, 0, 0); }
    static Vector3f Up()      { return Vector3f(0, 1, 0); }
    static Vector3f Forward() { return Vector3f(0, 0, 1); }

    // Keep lowercase for backward compatibility (used heavily in existing code)
    static Vector3f zero()    { return Zero(); }
    static Vector3f one()     { return One(); }
    static Vector3f right()   { return Right(); }
    static Vector3f up()      { return Up(); }
    static Vector3f forward() { return Forward(); }

    // ── String / Stream ──────────────────────────────────────────────────────

    std::string ToString() const
    {
        return "(" + std::to_string(x) + ", " + std::to_string(y) + ", " + std::to_string(z) + ")";
    }

    friend std::ostream& operator<<(std::ostream& os, const Vector3f& v)
    {
        os << "(" << v.x << ", " << v.y << ", " << v.z << ")";
        return os;
    }
};

Vector3f operator*(float scalar, const Vector3f& vector);
