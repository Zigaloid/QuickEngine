#pragma once

#include <cmath>
#include <iostream>
#include <algorithm>
#include <array>
#include <corecrt_math_defines.h>

/** @brief 4-component float vector for 3D math operations (including homogeneous coordinates). */
class Vector4f
{
public:
    float x, y, z, w;
    // ── Constructors ─────────────────────────────────────────────────────────

    Vector4f() : x(0.0f), y(0.0f), z(0.0f), w(0.0f) {}
    Vector4f(float x, float y, float z, float w) : x(x), y(y), z(z), w(w) {}
    Vector4f(const Vector4f& other) : x(other.x), y(other.y), z(other.z), w(other.w) {}

    const float* data() const { return &x; }

    // ── Accessors ────────────────────────────────────────────────────────────

    float GetX() const { return x; }
    float GetY() const { return y; }
    float GetZ() const { return z; }
    float GetW() const { return w; }

    void SetX(float x) { this->x = x; }
    void SetY(float y) { this->y = y; }
    void SetZ(float z) { this->z = z; }
    void SetW(float w) { this->w = w; }
    void Set(float x, float y, float z, float w) { this->x = x; this->y = y; this->z = z; this->w = w; }

    // Keep lowercase aliases for backward compatibility
    float getX() const { return x; }
    float getY() const { return y; }
    float getZ() const { return z; }
    float getW() const { return w; }

    // ── Assignment ───────────────────────────────────────────────────────────

    Vector4f& operator=(const Vector4f& other)
    {
        if (this != &other)
        {
            x = other.x;
            y = other.y;
            z = other.z;
            w = other.w;
        }
        return *this;
    }

    // ── Arithmetic Operators ─────────────────────────────────────────────────

    Vector4f operator+(const Vector4f& other) const { return Vector4f(x + other.x, y + other.y, z + other.z, w + other.w); }
    Vector4f& operator+=(const Vector4f& other) { x += other.x; y += other.y; z += other.z; w += other.w; return *this; }

    Vector4f operator-(const Vector4f& other) const { return Vector4f(x - other.x, y - other.y, z - other.z, w - other.w); }
    Vector4f& operator-=(const Vector4f& other) { x -= other.x; y -= other.y; z -= other.z; w -= other.w; return *this; }

    Vector4f operator-() const { return Vector4f(-x, -y, -z, -w); }

    Vector4f operator*(float scalar) const { return Vector4f(x * scalar, y * scalar, z * scalar, w * scalar); }
    Vector4f& operator*=(float scalar) { x *= scalar; y *= scalar; z *= scalar; w *= scalar; return *this; }

    Vector4f operator/(float scalar) const
    {
        if (scalar == 0)
            throw std::invalid_argument("Division by zero");
        return Vector4f(x / scalar, y / scalar, z / scalar, w / scalar);
    }

    Vector4f& operator/=(float scalar)
    {
        if (scalar == 0)
            throw std::invalid_argument("Division by zero");
        x /= scalar; y /= scalar; z /= scalar; w /= scalar;
        return *this;
    }

    // ── Comparison ───────────────────────────────────────────────────────────

    bool operator==(const Vector4f& other) const
    {
        const float epsilon = 1e-6f;
        return std::abs(x - other.x) < epsilon &&
               std::abs(y - other.y) < epsilon &&
               std::abs(z - other.z) < epsilon &&
               std::abs(w - other.w) < epsilon;
    }

    bool operator!=(const Vector4f& other) const { return !(*this == other); }

    // ── Math ─────────────────────────────────────────────────────────────────

    float Dot(const Vector4f& other) const
    {
        return x * other.x + y * other.y + z * other.z + w * other.w;
    }

    float Magnitude()        const { return std::sqrt(x * x + y * y + z * z + w * w); }
    float MagnitudeSquared() const { return x * x + y * y + z * z + w * w; }

    Vector4f& Normalize()
    {
        float mag = Magnitude();
        if (mag == 0)
            throw std::runtime_error("Cannot normalize zero vector");
        x /= mag; y /= mag; z /= mag; w /= mag;
        return *this;
    }

    Vector4f Normalized() const
    {
        float mag = Magnitude();
        if (mag == 0)
            throw std::runtime_error("Cannot normalize zero vector");
        return Vector4f(x / mag, y / mag, z / mag, w / mag);
    }

    float DistanceTo(const Vector4f& other)        const { return (*this - other).Magnitude(); }
    float DistanceSquaredTo(const Vector4f& other) const { return (*this - other).MagnitudeSquared(); }

    bool IsZero() const
    {
        const float epsilon = 1e-6f;
        return Magnitude() < epsilon;
    }

    bool IsUnit() const
    {
        const float epsilon = 1e-6f;
        return std::abs(Magnitude() - 1.0f) < epsilon;
    }

    // ── Static Factories ─────────────────────────────────────────────────────

    static Vector4f Lerp(const Vector4f& a, const Vector4f& b, float t) { return a + (b - a) * t; }
    static Vector4f Zero() { return Vector4f(0, 0, 0, 0); }
    static Vector4f One()  { return Vector4f(1, 1, 1, 1); }

    // ── Stream Output ─────────────────────────────────────────────────────────

    friend std::ostream& operator<<(std::ostream& os, const Vector4f& v)
    {
        os << "(" << v.x << ", " << v.y << ", " << v.z << ", " << v.w << ")";
        return os;
    }
};

Vector4f operator*(float scalar, const Vector4f& vector);
