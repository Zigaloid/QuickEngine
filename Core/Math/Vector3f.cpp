#include "Vector3f.h"

// Global operator for scalar * vector (commutative scalar multiplication)
Vector3f operator*(float scalar, const Vector3f& vector) {
    return vector * scalar;
}
