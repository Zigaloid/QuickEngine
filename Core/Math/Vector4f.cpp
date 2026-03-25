#include "Vector4f.h"

// Global operator for scalar * vector (commutative scalar multiplication)
Vector4f operator*(float scalar, const Vector4f& vector) {
    return vector * scalar;
}