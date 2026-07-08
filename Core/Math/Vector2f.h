#pragma once
#include <cmath>

/**
 * @brief 2D vector with float components
 */
struct Vector2f
{
    float x = 0.0f;
    float y = 0.0f;

    Vector2f() = default;
    Vector2f(float _x, float _y) : x(_x), y(_y) {}

    // ?? Operators ????????????????????????????????????????????????????????

    Vector2f operator+(const Vector2f& v) const { return Vector2f(x + v.x, y + v.y); }
    Vector2f operator-(const Vector2f& v) const { return Vector2f(x - v.x, y - v.y); }
    Vector2f operator*(float s) const { return Vector2f(x * s, y * s); }
    Vector2f operator/(float s) const { return Vector2f(x / s, y / s); }

    Vector2f& operator+=(const Vector2f& v) { x += v.x; y += v.y; return *this; }
    Vector2f& operator-=(const Vector2f& v) { x -= v.x; y -= v.y; return *this; }
    Vector2f& operator*=(float s) { x *= s; y *= s; return *this; }
    Vector2f& operator/=(float s) { x /= s; y /= s; return *this; }

    bool operator==(const Vector2f& v) const { return x == v.x && y == v.y; }
    bool operator!=(const Vector2f& v) const { return !(*this == v); }

    // ?? Vector operations ????????????????????????????????????????????????

    float Dot(const Vector2f& v) const { return x * v.x + y * v.y; }
    float Cross(const Vector2f& v) const { return x * v.y - y * v.x; }

    float Length() const { return std::sqrt(x * x + y * y); }
    float LengthSquared() const { return x * x + y * y; }

    Vector2f Normalized() const 
    { 
        float len = Length();
        if (len > 0.0f) return Vector2f(x / len, y / len);
        return Vector2f(0.0f, 0.0f);
    }

    void Normalize()
    {
        float len = Length();
        if (len > 0.0f)
        {
            x /= len;
            y /= len;
        }
    }

    // ?? Static utilities ????????????????????????????????????????????????

    static Vector2f Zero() { return Vector2f(0.0f, 0.0f); }
    static Vector2f One() { return Vector2f(1.0f, 1.0f); }
    static Vector2f Right() { return Vector2f(1.0f, 0.0f); }
    static Vector2f Up() { return Vector2f(0.0f, 1.0f); }
};
