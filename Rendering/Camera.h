#pragma once
#include "math\Quaternion.h"
#include "math\Vector3f.h"
#include "math\Matrix4f.h"
#include <iostream>
#include <cmath>

class Camera {
private:
    Vector3f m_position;
    Quaternion m_orientation;
    float m_fov;
    float m_aspectRatio;
    float m_nearPlane;
    float m_farPlane;

public:
    Camera(const Vector3f& pos = Vector3f(0, 0, 0),
        float fovDegrees = 75.0f,
        float aspect = 16.0f / 9.0f,
        float near = 0.1f,
        float far = 1000.0f)
        : m_position(pos), m_orientation(Quaternion::identity()),
        m_fov(fovDegrees* (float)M_PI / 180.0f), m_aspectRatio(aspect),
        m_nearPlane(near), m_farPlane(far) {
    }

    // Set camera position
    void setPosition(const Vector3f& pos) { m_position = pos; }
    Vector3f getPosition() const { return m_position; }

    // Set camera orientation using quaternion
    void setOrientation(const Quaternion& quat) { m_orientation = quat.normalized(); }
    Quaternion getOrientation() const { return m_orientation; }

    // Camera movement methods
    void moveForward(float distance) {
        Vector3f forward = m_orientation.getForward();
        m_position += forward * distance;
    }

    void moveRight(float distance) {
        Vector3f right = m_orientation.getRight();
        m_position += right * distance;
    }

    void moveUp(float distance) {
        Vector3f up = m_orientation.getUp();
        m_position += up * distance;
    }

    // Camera rotation methods using quaternions
    void pitch(float angleRad) {
        Quaternion pitchQuat = Quaternion::rotationX(angleRad);
        m_orientation = m_orientation * pitchQuat;
        m_orientation.normalize();
    }

    void yaw(float angleRad) {
        Quaternion yawQuat = Quaternion::rotationY(angleRad);
        m_orientation = yawQuat * m_orientation;  // World-space rotation
        m_orientation.normalize();
    }

    void roll(float angleRad) {
        Quaternion rollQuat = Quaternion::rotationZ(angleRad);
        m_orientation = m_orientation * rollQuat;
        m_orientation.normalize();
    }

    // Look at a specific point
    void lookAt(const Vector3f& target, const Vector3f& worldUp = Vector3f(0, 1, 0)) {
        Vector3f forward = (target - m_position).normalized();
        m_orientation = Quaternion::lookRotation(-forward, worldUp);
    }

    // Get view matrix
    Matrix4f getViewMatrix() const {
        // Create rotation matrix from quaternion
        Matrix4f rotationMatrix = m_orientation.conjugate().toMatrix();

        // Create translation matrix
        Matrix4f translationMatrix = Matrix4f::translation(-m_position.getX(),
            -m_position.getY(),
            -m_position.getZ());

        // View matrix is rotation * translation
        return rotationMatrix * translationMatrix;
    }

    // Get projection matrix
    Matrix4f getProjectionMatrix() const {
        return Matrix4f::perspective(m_fov, m_aspectRatio, m_nearPlane, m_farPlane);
    }

    // Get combined view-projection matrix
    Matrix4f getViewProjectionMatrix() const {
        return getProjectionMatrix() * getViewMatrix();
    }

    // Get direction vectors
    Vector3f getForward() const { return m_orientation.getForward(); }
    Vector3f getRight() const { return m_orientation.getRight(); }
    Vector3f getUp() const { return m_orientation.getUp(); }

    // FPS-style camera controls
    void updateFPSCamera(float deltaYaw, float deltaPitch, float deltaTime) {
        // Limit pitch to prevent gimbal lock
        Vector3f eulerAngles = m_orientation.toEulerAngles();
        float currentPitch = eulerAngles.getY();
        float newPitch = currentPitch + deltaPitch;

        // Clamp pitch to [-89, 89] degrees
        const float maxPitch = (89.0f * (float)M_PI) / 180.0f;
        newPitch = (std::max)((-maxPitch), (std::min)(maxPitch, newPitch));
        deltaPitch = newPitch - currentPitch;

        // Apply rotations
        yaw(deltaYaw);
        pitch(deltaPitch);
    }

    // Smooth camera interpolation
    void interpolateToTarget(const Vector3f& targetPos, const Quaternion& targetRot, float t) {
        m_position = Vector3f::lerp(m_position, targetPos, t);
        m_orientation = Quaternion::slerp(m_orientation, targetRot, t);
    }

    // Debug output
    void printDebugInfo() const {
        std::cout << "Camera Position: " << m_position << std::endl;
        std::cout << "Camera Orientation: " << m_orientation << std::endl;
        std::cout << "Forward: " << getForward() << std::endl;
        std::cout << "Right: " << getRight() << std::endl;
        std::cout << "Up: " << getUp() << std::endl;
    }
};

