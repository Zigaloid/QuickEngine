#pragma once

#include <cmath>
#include <iostream>
#include <algorithm>
#include <string>
#include <corecrt_math_defines.h>

class Vector3f {
public:
	float x, y, z;
	// Constructors
	Vector3f() : x(0), y(0), z(0) {}
	Vector3f(float x, float y, float z) : x(x), y(y), z(z) {}
	Vector3f(const Vector3f& other) : x(other.x), y(other.y), z(other.z) {}

	// Getters
	float getX() const { return x; }
	float getY() const { return y; }
	float getZ() const { return z; }

	// Setters
	void setX(float x) { this->x = x; }
	void setY(float y) { this->y = y; }
	void setZ(float z) { this->z = z; }
	void set(float x, float y, float z) { this->x = x; this->y = y; this->z = z; }

	// Assignment operator
	Vector3f& operator=(const Vector3f& other) {
		if (this != &other) {
			x = other.x;
			y = other.y;
			z = other.z;
		}
		return *this;
	}

	// Vector addition
	Vector3f operator+(const Vector3f& other) const {
		return Vector3f(x + other.x, y + other.y, z + other.z);
	}

	Vector3f& operator+=(const Vector3f& other) {
		x += other.x;
		y += other.y;
		z += other.z;
		return *this;
	}

	// Vector subtraction
	Vector3f operator-(const Vector3f& other) const {
		return Vector3f(x - other.x, y - other.y, z - other.z);
	}

	Vector3f& operator-=(const Vector3f& other) {
		x -= other.x;
		y -= other.y;
		z -= other.z;
		return *this;
	}

	// Unary minus (negation)
	Vector3f operator-() const {
		return Vector3f(-x, -y, -z);
	}

	// Scalar multiplication
	Vector3f operator*(float scalar) const {
		return Vector3f(x * scalar, y * scalar, z * scalar);
	}

	Vector3f& operator*=(float scalar) {
		x *= scalar;
		y *= scalar;
		z *= scalar;
		return *this;
	}

	// Scalar division
	Vector3f operator/(float scalar) const {
		if (scalar == 0) {
			throw std::invalid_argument("Division by zero");
		}
		return Vector3f(x / scalar, y / scalar, z / scalar);
	}

	Vector3f& operator/=(float scalar) {
		if (scalar == 0) {
			throw std::invalid_argument("Division by zero");
		}
		x /= scalar;
		y /= scalar;
		z /= scalar;
		return *this;
	}

	// Comparison operators
	bool operator==(const Vector3f& other) const {
		const float epsilon = 1e-6f;
		return std::abs(x - other.x) < epsilon &&
			std::abs(y - other.y) < epsilon &&
			std::abs(z - other.z) < epsilon;
	}

	bool operator!=(const Vector3f& other) const {
		return !(*this == other);
	}

	// Dot product
	float dot(const Vector3f& other) const {
		return x * other.x + y * other.y + z * other.z;
	}

	// Cross product
	Vector3f cross(const Vector3f& other) const {
		return Vector3f(
			y * other.z - z * other.y,
			z * other.x - x * other.z,
			x * other.y - y * other.x
		);
	}

	// Magnitude (length) of the vector
	float magnitude() const {
		return std::sqrt(x * x + y * y + z * z);
	}

	float length() const {
		return std::sqrt(x * x + y * y + z * z);
	}


	// Squared magnitude (more efficient when you don't need the actual magnitude)
	float magnitudeSquared() const {
		return x * x + y * y + z * z;
	}

	// Normalize the vector (make it unit length)
	Vector3f& normalize() {
		float mag = magnitude();
		if (mag == 0) {
			throw std::runtime_error("Cannot normalize zero vector");
		}
		x /= mag;
		y /= mag;
		z /= mag;
		return *this;
	}

	// Get normalized vector without modifying original
	Vector3f normalized() const {
		float mag = magnitude();
		if (mag == 0) {
			throw std::runtime_error("Cannot normalize zero vector");
		}
		return Vector3f(x / mag, y / mag, z / mag);
	}

	// Distance between two points represented as vectors
	float distanceTo(const Vector3f& other) const {
		return (*this - other).magnitude();
	}

	// Squared distance (more efficient when you don't need the actual distance)
	float distanceSquaredTo(const Vector3f& other) const {
		return (*this - other).magnitudeSquared();
	}

	// Angle between two vectors (in radians)
	float angleTo(const Vector3f& other) const {
		float mag1 = magnitude();
		float mag2 = other.magnitude();
		if (mag1 == 0 || mag2 == 0) {
			throw std::runtime_error("Cannot calculate angle with zero vector");
		}
		float cosAngle = dot(other) / (mag1 * mag2);
		// Clamp to avoid floating point errors
		//cosAngle = std::max(-1.0f, std::min(1.0f, cosAngle) );        
		if (cosAngle < -1.0f)
			cosAngle = -1.0f;
		else if (cosAngle > 1.0f)
			cosAngle = 1.0f;

		return std::acos(cosAngle);
	}

	// Linear interpolation between two vectors
	static Vector3f lerp(const Vector3f& a, const Vector3f& b, float t) {
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

	// Project this vector onto another vector
	Vector3f projectOnto(const Vector3f& other) const {
		float otherMagSq = other.magnitudeSquared();
		if (otherMagSq == 0) {
			throw std::runtime_error("Cannot project onto zero vector");
		}
		return other * (dot(other) / otherMagSq);
	}

	// Reflect this vector across a normal vector
	Vector3f reflect(const Vector3f& normal) const {
		return *this - normal * (2.0f * dot(normal));
	}

	// Static utility functions for common vectors
	static Vector3f zero() { return Vector3f(0, 0, 0); }
	static Vector3f one() { return Vector3f(1, 1, 1); }
	static Vector3f right() { return Vector3f(1, 0, 0); }
	static Vector3f up() { return Vector3f(0, 1, 0); }
	static Vector3f forward() { return Vector3f(0, 0, 1); }

	std::string toString() const {
		return "(" + std::to_string(x) + ", " + std::to_string(y) + ", " + std::to_string(z) + ")";
	}
	// Stream output operator
	friend std::ostream& operator<<(std::ostream& os, const Vector3f& v) {
		os << "(" << v.x << ", " << v.y << ", " << v.z << ")";
		return os;
	}
};

Vector3f operator*(float scalar, const Vector3f& vector);