#pragma once

#include <cmath>
#include <iostream>
#include <algorithm>
#include <array>
#include <corecrt_math_defines.h>

class Vector4f {
private:
	float x, y, z, w;

public:
    // Constructors
    Vector4f() : x(0), y(0), z(0), w(0) {}
    Vector4f(float x, float y, float z, float w) : x(x), y(y), z(z), w(w) {}
    Vector4f(const Vector4f& other) : x(other.x), y(other.y), z(other.z), w(other.w) {}
    
    const float* data() const { return &x; }
    // Getters
    float getX() const { return x; }
    float getY() const { return y; }
    float getZ() const { return z; }
    float getW() const { return w; }

    // Setters
    void setX(float x) { this->x = x; }
    void setY(float y) { this->y = y; }
    void setZ(float z) { this->z = z; }
    void setW(float w) { this->w = w; }
    void set(float x, float y, float z, float w) { this->x = x; this->y = y; this->z = z; this->w = w; }

    // Assignment operator
    Vector4f& operator=(const Vector4f& other) {
        if (this != &other) {
            x = other.x;
            y = other.y;
            z = other.z;
            w = other.w;
        }
        return *this;
    }

    // Vector addition
    Vector4f operator+(const Vector4f& other) const {
        return Vector4f(x + other.x, y + other.y, z + other.z, w + other.w);
    }

    Vector4f& operator+=(const Vector4f& other) {
        x += other.x;
        y += other.y;
        z += other.z;
        w += other.w;
        return *this;
    }

    // Vector subtraction
    Vector4f operator-(const Vector4f& other) const {
        return Vector4f(x - other.x, y - other.y, z - other.z, w - other.w);
    }

    Vector4f& operator-=(const Vector4f& other) {
        x -= other.x;
        y -= other.y;
        z -= other.z;
        w -= other.w;
        return *this;
    }

    // Unary minus (negation)
    Vector4f operator-() const {
        return Vector4f(-x, -y, -z, -w);
    }

    // Scalar multiplication
    Vector4f operator*(float scalar) const {
        return Vector4f(x * scalar, y * scalar, z * scalar, w * scalar);
    }

    Vector4f& operator*=(float scalar) {
        x *= scalar;
        y *= scalar;
        z *= scalar;
        w *= scalar;
        return *this;
    }

    // Scalar division
    Vector4f operator/(float scalar) const {
        if (scalar == 0) {
            throw std::invalid_argument("Division by zero");
        }
        return Vector4f(x / scalar, y / scalar, z / scalar, w / scalar);
    }

    Vector4f& operator/=(float scalar) {
        if (scalar == 0) {
            throw std::invalid_argument("Division by zero");
        }
        x /= scalar;
        y /= scalar;
        z /= scalar;
        w /= scalar;
        return *this;
    }

    // Comparison operators
    bool operator==(const Vector4f& other) const {
        const float epsilon = 1e-6f;
        return std::abs(x - other.x) < epsilon &&
            std::abs(y - other.y) < epsilon &&
            std::abs(z - other.z) < epsilon &&
            std::abs(w - other.w) < epsilon;
    }

    bool operator!=(const Vector4f& other) const {
        return !(*this == other);
    }

    // Dot product
    float dot(const Vector4f& other) const {
        return x * other.x + y * other.y + z * other.z + w * other.w;
    }

    // Magnitude (length) of the vector
    float magnitude() const {
        return std::sqrt(x * x + y * y + z * z + w * w);
    }

    // Squared magnitude (more efficient when you don't need the actual magnitude)
    float magnitudeSquared() const {
        return x * x + y * y + z * z + w * w;
    }

    // Normalize the vector (make it unit length)
    Vector4f& normalize() {
        float mag = magnitude();
        if (mag == 0) {
            throw std::runtime_error("Cannot normalize zero vector");
        }
        x /= mag;
        y /= mag;
        z /= mag;
        w /= mag;
        return *this;
    }

    // Get normalized vector without modifying original
    Vector4f normalized() const {
        float mag = magnitude();
        if (mag == 0) {
            throw std::runtime_error("Cannot normalize zero vector");
        }
        return Vector4f(x / mag, y / mag, z / mag, w / mag);
    }

    // Distance between two points represented as vectors
    float distanceTo(const Vector4f& other) const {
        return (*this - other).magnitude();
    }

    // Squared distance (more efficient when you don't need the actual distance)
    float distanceSquaredTo(const Vector4f& other) const {
        return (*this - other).magnitudeSquared();
    }

    // Linear interpolation between two vectors
    static Vector4f lerp(const Vector4f& a, const Vector4f& b, float t) {
        return a + (b - a) * t;
    }

    // Check if vector is zero
    bool isZero() const {
        const float epsilon = 1e-6f;
        return magnitude() < epsilon;
    }

    // Check if vector is unit length
    bool isUnit() const {
        const float epsilon = 1e-6f;
        return std::abs(magnitude() - 1.0f) < epsilon;
    }

    // Static utility functions for common vectors
    static Vector4f zero() { return Vector4f(0, 0, 0, 0); }
    static Vector4f one() { return Vector4f(1, 1, 1, 1); }

    // Stream output operator
    friend std::ostream& operator<<(std::ostream& os, const Vector4f& v) {
        os << "(" << v.x << ", " << v.y << ", " << v.z << ", " << v.w << ")";
        return os;
    }
};

// Global operator for scalar * vector (commutative scalar multiplication)
Vector4f operator*(float scalar, const Vector4f& vector);