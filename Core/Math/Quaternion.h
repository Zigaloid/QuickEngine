#pragma once

#include <cmath>
#include <iostream>
#include <algorithm>
#include <corecrt_math_defines.h>
#include "Vector3f.h"
#include "Vector4f.h"

// Forward declaration for Matrix4x4
class Matrix4f;

class Quaternion {
private:
    float w, x, y, z;  // w is the scalar part, (x, y, z) is the vector part

public:
    // Constructors
    Quaternion() : w(1.0f), x(0.0f), y(0.0f), z(0.0f) {}  // Identity quaternion
    Quaternion(float w, float x, float y, float z) : w(w), x(x), y(y), z(z) {}
    Quaternion(const Quaternion& other) : w(other.w), x(other.x), y(other.y), z(other.z) {}

    // Constructor from axis-angle representation
    Quaternion(const Vector3f& axis, float angleRad) {
        fromAxisAngle(axis, angleRad);
    }

    // Constructor from Euler angles (in radians)
    Quaternion(float pitch, float yaw, float roll) {
        fromEulerAngles(pitch, yaw, roll);
    }

    // Getters
    float getW() const { return w; }
    float getX() const { return x; }
    float getY() const { return y; }
    float getZ() const { return z; }

    // Setters
    void setW(float w) { this->w = w; }
    void setX(float x) { this->x = x; }
    void setY(float y) { this->y = y; }
    void setZ(float z) { this->z = z; }
    void set(float w, float x, float y, float z) { this->w = w; this->x = x; this->y = y; this->z = z; }

    // Assignment operator
    Quaternion& operator=(const Quaternion& other) {
        if (this != &other) {
            w = other.w;
            x = other.x;
            y = other.y;
            z = other.z;
        }
        return *this;
    }

    // Quaternion addition
    Quaternion operator+(const Quaternion& other) const {
        return Quaternion(w + other.w, x + other.x, y + other.y, z + other.z);
    }

    Quaternion& operator+=(const Quaternion& other) {
        w += other.w;
        x += other.x;
        y += other.y;
        z += other.z;
        return *this;
    }

    // Quaternion subtraction
    Quaternion operator-(const Quaternion& other) const {
        return Quaternion(w - other.w, x - other.x, y - other.y, z - other.z);
    }

    Quaternion& operator-=(const Quaternion& other) {
        w -= other.w;
        x -= other.x;
        y -= other.y;
        z -= other.z;
        return *this;
    }

    // Unary minus (negation)
    Quaternion operator-() const {
        return Quaternion(-w, -x, -y, -z);
    }

    // Quaternion multiplication (Hamilton product)
    Quaternion operator*(const Quaternion& other) const {
        return Quaternion(
            w * other.w - x * other.x - y * other.y - z * other.z,
            w * other.x + x * other.w + y * other.z - z * other.y,
            w * other.y - x * other.z + y * other.w + z * other.x,
            w * other.z + x * other.y - y * other.x + z * other.w
        );
    }

    Quaternion& operator*=(const Quaternion& other) {
        *this = *this * other;
        return *this;
    }

    // Scalar multiplication
    Quaternion operator*(float scalar) const {
        return Quaternion(w * scalar, x * scalar, y * scalar, z * scalar);
    }

    Quaternion& operator*=(float scalar) {
        w *= scalar;
        x *= scalar;
        y *= scalar;
        z *= scalar;
        return *this;
    }

    // Scalar division
    Quaternion operator/(float scalar) const {
        if (scalar == 0) {
            throw std::invalid_argument("Division by zero");
        }
        return Quaternion(w / scalar, x / scalar, y / scalar, z / scalar);
    }

    Quaternion& operator/=(float scalar) {
        if (scalar == 0) {
            throw std::invalid_argument("Division by zero");
        }
        w /= scalar;
        x /= scalar;
        y /= scalar;
        z /= scalar;
        return *this;
    }

    // Comparison operators
    bool operator==(const Quaternion& other) const {
        const float epsilon = 1e-6f;
        return std::abs(w - other.w) < epsilon &&
            std::abs(x - other.x) < epsilon &&
            std::abs(y - other.y) < epsilon &&
            std::abs(z - other.z) < epsilon;
    }

    bool operator!=(const Quaternion& other) const {
        return !(*this == other);
    }

    // Dot product
    float dot(const Quaternion& other) const {
        return w * other.w + x * other.x + y * other.y + z * other.z;
    }

    // Magnitude (norm) of the quaternion
    float magnitude() const {
        return std::sqrt(w * w + x * x + y * y + z * z);
    }

    // Squared magnitude
    float magnitudeSquared() const {
        return w * w + x * x + y * y + z * z;
    }

    // Normalize the quaternion
    Quaternion& normalize() {
        float mag = magnitude();
        if (mag == 0) {
            throw std::runtime_error("Cannot normalize zero quaternion");
        }
        w /= mag;
        x /= mag;
        y /= mag;
        z /= mag;
        return *this;
    }

    // Get normalized quaternion without modifying original
    Quaternion normalized() const {
        float mag = magnitude();
        if (mag == 0) {
            throw std::runtime_error("Cannot normalize zero quaternion");
        }
        return Quaternion(w / mag, x / mag, y / mag, z / mag);
    }

    // Quaternion conjugate
    Quaternion conjugate() const {
        return Quaternion(w, -x, -y, -z);
    }

    // Quaternion inverse
    Quaternion inverse() const {
        float magSq = magnitudeSquared();
        if (magSq == 0) {
            throw std::runtime_error("Cannot invert zero quaternion");
        }
        return conjugate() / magSq;
    }

    // Check if quaternion is unit length
    bool isUnit() const {
        const float epsilon = 1e-6f;
        return std::abs(magnitude() - 1.0f) < epsilon;
    }

    // Check if quaternion is zero
    bool isZero() const {
        const float epsilon = 1e-6f;
        return magnitude() < epsilon;
    }

    // Convert from axis-angle representation
    void fromAxisAngle(const Vector3f& axis, float angleRad) {
        Vector3f normalizedAxis = axis.normalized();
        float halfAngle = angleRad * 0.5f;
        float sinHalf = std::sin(halfAngle);
        
        w = std::cos(halfAngle);
        x = normalizedAxis.getX() * sinHalf;
        y = normalizedAxis.getY() * sinHalf;
        z = normalizedAxis.getZ() * sinHalf;
    }

    // Convert to axis-angle representation
    void toAxisAngle(Vector3f& axis, float& angleRad) const {
        Quaternion q = normalized();
        angleRad = 2.0f * std::acos(std::abs(q.w));
        
        float sinHalf = std::sqrt(1.0f - q.w * q.w);
        if (sinHalf < 1e-6f) {
            // Angle is very small, axis is arbitrary
            axis = Vector3f(1, 0, 0);
        } else {
            axis.set(q.x / sinHalf, q.y / sinHalf, q.z / sinHalf);
        }
    }

    // Convert from Euler angles (pitch, yaw, roll in radians)
    void fromEulerAngles(float pitch, float yaw, float roll) {
        float cy = std::cos(yaw * 0.5f);
        float sy = std::sin(yaw * 0.5f);
        float cp = std::cos(pitch * 0.5f);
        float sp = std::sin(pitch * 0.5f);
        float cr = std::cos(roll * 0.5f);
        float sr = std::sin(roll * 0.5f);

        w = cr * cp * cy + sr * sp * sy;
        x = sr * cp * cy - cr * sp * sy;
        y = cr * sp * cy + sr * cp * sy;
        z = cr * cp * sy - sr * sp * cy;
    }

    // Convert to Euler angles (pitch, yaw, roll in radians)
    Vector3f toEulerAngles() const {
        Vector3f angles;
        
        // Roll (x-axis rotation)
        float sinr_cosp = 2 * (w * x + y * z);
        float cosr_cosp = 1 - 2 * (x * x + y * y);
        angles.setX(std::atan2(sinr_cosp, cosr_cosp));

        // Pitch (y-axis rotation)
        float sinp = 2 * (w * y - z * x);
        if (std::abs(sinp) >= 1)
            angles.setZ(std::copysign((float)M_PI / 2, sinp)); // Use 90 degrees if out of range
        else
            angles.setZ(std::asin(sinp));

        // Yaw (z-axis rotation)
        float siny_cosp = 2 * (w * z + x * y);
        float cosy_cosp = 1 - 2 * (y * y + z * z);
        angles.setY(std::atan2(siny_cosp, cosy_cosp));

        return angles;
    }

    // Rotate a vector by this quaternion
    Vector3f rotateVector(const Vector3f& vector) const {
        // Using the formula: v' = q * v * q^-1
        // More efficient version: v' = v + 2 * cross(q.xyz, cross(q.xyz, v) + q.w * v)
        Vector3f qvec(x, y, z);
        Vector3f uv = qvec.cross(vector);
        Vector3f uuv = qvec.cross(uv);
        
        return vector + ((uv * w) + uuv) * 2.0f;
    }

    // Overloaded operator for vector rotation
    Vector3f operator*(const Vector3f& vector) const {
        return rotateVector(vector);
    }

    // Spherical linear interpolation (SLERP)
    static Quaternion slerp(const Quaternion& a, const Quaternion& b, float t) {
        Quaternion qa = a.normalized();
        Quaternion qb = b.normalized();
        
        float dot = qa.dot(qb);
        
        // If the dot product is negative, slerp won't take the shorter path.
        // Note that v1 and -v1 are equivalent when the quaternions represent rotations.
        if (dot < 0.0f) {
            qb = -qb;
            dot = -dot;
        }

        // If the inputs are too close for comfort, linearly interpolate
        const float DOT_THRESHOLD = 0.9995f;
        if (dot > DOT_THRESHOLD) {
            Quaternion result = qa + (qb - qa) * t;
            return result.normalized();
        }

        // Calculate the angle between the quaternions
        float theta_0 = std::acos(dot);
        float theta = theta_0 * t;

        Quaternion q2 = qb - qa * dot;
        q2.normalize();

        return qa * std::cos(theta) + q2 * std::sin(theta);
    }

    // Normalized linear interpolation (faster but less accurate than SLERP)
    static Quaternion nlerp(const Quaternion& a, const Quaternion& b, float t) {
        Quaternion qa = a.normalized();
        Quaternion qb = b.normalized();
        
        float dot = qa.dot(qb);
        if (dot < 0.0f) {
            qb = -qb;
        }
        
        return (qa + (qb - qa) * t).normalized();
    }

    // Convert quaternion to rotation matrix
    Matrix4f toMatrix() const;

    // Create quaternion from rotation matrix
    static Quaternion fromMatrix(const Matrix4f& matrix);

    // Static factory methods for common rotations
    static Quaternion identity() {
        return Quaternion(1.0f, 0.0f, 0.0f, 0.0f);
    }

    static Quaternion rotationX(float angleRad) {
        return Quaternion(Vector3f(1, 0, 0), angleRad);
    }

    static Quaternion rotationY(float angleRad) {
        return Quaternion(Vector3f(0, 1, 0), angleRad);
    }

    static Quaternion rotationZ(float angleRad) {
        return Quaternion(Vector3f(0, 0, 1), angleRad);
    }

    // Create a quaternion that rotates from one vector to another
    static Quaternion fromToRotation(const Vector3f& from, const Vector3f& to) {
        Vector3f fromNorm = from.normalized();
        Vector3f toNorm = to.normalized();
        
        float dot = fromNorm.dot(toNorm);
        
        // Vectors are parallel
        if (dot >= 1.0f - 1e-6f) {
            return identity();
        }
        
        // Vectors are opposite
        if (dot <= -1.0f + 1e-6f) {
            // Find a perpendicular axis
            Vector3f axis = fromNorm.cross(Vector3f(1, 0, 0));
            if (axis.magnitude() < 1e-6f) {
                axis = fromNorm.cross(Vector3f(0, 1, 0));
            }
            axis.normalize();
            return Quaternion(axis, (float)M_PI);
        }
        
        Vector3f axis = fromNorm.cross(toNorm);
        float s = std::sqrt((1.0f + dot) * 2.0f);
        float invs = 1.0f / s;
        
        return Quaternion(s * 0.5f, axis.getX() * invs, axis.getY() * invs, axis.getZ() * invs);
    }

    // Create a look rotation (useful for cameras)
    static Quaternion lookRotation(const Vector3f& forward, const Vector3f& up = Vector3f(0, 1, 0)) {
        Vector3f forwardNorm = forward.normalized();
        Vector3f rightNorm = up.cross(forwardNorm).normalized();
        Vector3f upNorm = forwardNorm.cross(rightNorm);
        
        // Create rotation matrix
        float m00 = rightNorm.getX();
        float m01 = rightNorm.getY();
        float m02 = rightNorm.getZ();
        float m10 = upNorm.getX();
        float m11 = upNorm.getY();
        float m12 = upNorm.getZ();
        float m20 = forwardNorm.getX();
        float m21 = forwardNorm.getY();
        float m22 = forwardNorm.getZ();
        
        // Convert to quaternion using Shepperd's method
        float trace = m00 + m11 + m22;
        Quaternion q;
        
        if (trace > 0) {
            float s = std::sqrt(trace + 1.0f) * 2;  // s=4*qw 
            q.w = 0.25f * s;
            q.x = (m21 - m12) / s;
            q.y = (m02 - m20) / s;
            q.z = (m10 - m01) / s;
        } else if ((m00 > m11) && (m00 > m22)) {
            float s = std::sqrt(1.0f + m00 - m11 - m22) * 2; // s=4*qx
            q.w = (m21 - m12) / s;
            q.x = 0.25f * s;
            q.y = (m01 + m10) / s;
            q.z = (m02 + m20) / s;
        } else if (m11 > m22) {
            float s = std::sqrt(1.0f + m11 - m00 - m22) * 2; // s=4*qy
            q.w = (m02 - m20) / s;
            q.x = (m01 + m10) / s;
            q.y = 0.25f * s;
            q.z = (m12 + m21) / s;
        } else {
            float s = std::sqrt(1.0f + m22 - m00 - m11) * 2; // s=4*qz
            q.w = (m10 - m01) / s;
            q.x = (m02 + m20) / s;
            q.y = (m12 + m21) / s;
            q.z = 0.25f * s;
        }
        
        return q;
    }

    // Get the forward direction vector from this quaternion
    Vector3f getForward() const {
        return rotateVector(Vector3f(0, 0, 1));
    }

    // Get the right direction vector from this quaternion
    Vector3f getRight() const {
        return rotateVector(Vector3f(1, 0, 0));
    }

    // Get the up direction vector from this quaternion
    Vector3f getUp() const {
        return rotateVector(Vector3f(0, 1, 0));
    }

    // Stream output operator
    friend std::ostream& operator<<(std::ostream& os, const Quaternion& q) {
        os << "(" << q.w << ", " << q.x << ", " << q.y << ", " << q.z << ")";
        return os;
    }
};

// Non-member operators
inline Quaternion operator*(float scalar, const Quaternion& quaternion) {
    return quaternion * scalar;
}