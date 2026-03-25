#include "CameraController.h"
#include <iostream>
#include <cmath>
#include <algorithm>

CameraController::CameraController()
    : m_camera(nullptr)
    , m_isDragging(false)
    , m_lastMouseX(0.0)
    , m_lastMouseY(0.0)
{
    // Default constructor creates no camera - user must set one
}

CameraController::CameraController(std::unique_ptr<Camera> camera)
    : m_camera(std::move(camera))
    , m_isDragging(false)
    , m_lastMouseX(0.0)
    , m_lastMouseY(0.0)
{
    // Constructor with camera parameter
}

void CameraController::resetCamera() {
    if (!m_camera) return;

    // Reset to default position and orientation
    m_camera->setPosition(Vector3f(0, 3, 3));
    m_camera->setOrientation(Quaternion::identity());
}

void CameraController::focusOnPoint(const Vector3f& point, float distance) {
    if (!m_camera) return;

    // Calculate current direction from camera to point
    Vector3f currentPos = m_camera->getPosition();
    Vector3f toPoint = (point - currentPos).normalized();

    // Position camera at specified distance from the point
    Vector3f newPosition = point - (toPoint * distance);
    m_camera->setPosition(newPosition);

    // Look at the point
    m_camera->lookAt(point);
}

void CameraController::fitBounds(const Vector3f& minBounds, const Vector3f& maxBounds) {
    if (!m_camera) return;

    // Calculate bounding box center and size
    Vector3f center = (minBounds + maxBounds) * 0.5f;
    Vector3f size = maxBounds - minBounds;

    // Calculate required distance to fit bounds in view
    // Note: This assumes your Camera class has a way to get FOV
    // You may need to add a getFOV() method to your Camera class
    float maxDimension = std::max(std::max(size.getX(), size.getY()), size.getZ());
    float fovRadians = 75.0f * (float)M_PI / 180.0f; // Default FOV from camera constructor
    float distance = maxDimension / (2.0f * std::tan(fovRadians / 2.0f));

    // Position camera to view the bounds
    Vector3f cameraPosition = center + Vector3f(0, 0, distance * 1.2f); // Add padding
    m_camera->setPosition(cameraPosition);
    m_camera->lookAt(center);
}

void CameraController::setTopView() {
    if (!m_camera) return;

    Vector3f currentPos = m_camera->getPosition();
    Vector3f center = Vector3f(0, 0, 0); // Or use current focus point

    // Position camera above the center point
    float distance = 10.0f;
    m_camera->setPosition(Vector3f(center.getX(), center.getY() + distance, center.getZ()));
    m_camera->lookAt(center, Vector3f(0, 0, -1)); // Look down with -Z as up vector
}

void CameraController::setFrontView() {
    if (!m_camera) return;

    Vector3f center = Vector3f(0, 0, 0);
    float distance = 10.0f;

    // Position camera in front looking back towards center
    m_camera->setPosition(Vector3f(center.getX(), center.getY(), center.getZ() + distance));
    m_camera->lookAt(center);
}

void CameraController::setSideView() {
    if (!m_camera) return;

    Vector3f center = Vector3f(0, 0, 0);
    float distance = 10.0f;

    // Position camera to the side looking towards center
    m_camera->setPosition(Vector3f(center.getX() + distance, center.getY(), center.getZ()));
    m_camera->lookAt(center);
}

void CameraController::setIsometricView() {
    if (!m_camera) return;

    Vector3f center = Vector3f(0, 0, 0);
    float distance = 10.0f;

    // Calculate isometric position (35.264° pitch, 45° yaw from center)
    float isometricPitch = 35.264f * (float)M_PI / 180.0f;
    float isometricYaw = 45.0f * (float)M_PI / 180.0f;

    Vector3f isometricPos;
    isometricPos.set(
        center.getX() + distance * std::cos(isometricPitch) * std::sin(isometricYaw),
        center.getY() + distance * std::sin(isometricPitch),
        center.getZ() + distance * std::cos(isometricPitch) * std::cos(isometricYaw)
    );

    m_camera->setPosition(isometricPos);
    m_camera->lookAt(center);
}

void CameraController::setOrbitCamera(const Vector3f& target, float distance, float azimuth, float elevation) {
    if (!m_camera) return;

    // Convert spherical coordinates to Cartesian
    float x = target.getX() + distance * std::cos(elevation) * std::cos(azimuth);
    float y = target.getY() + distance * std::sin(elevation);
    float z = target.getZ() + distance * std::cos(elevation) * std::sin(azimuth);

    m_camera->setPosition(Vector3f(x, y, z));
    m_camera->lookAt(target);
}

void CameraController::updateAspectRatio(float aspectRatio) {
    if (!m_camera) return;

    // Note: Your Camera class doesn't seem to have a setAspectRatio method
    // You may need to add this to the Camera class if you want to support
    // dynamic aspect ratio changes
    // m_camera->setAspectRatio(aspectRatio);
}

void CameraController::orbitAroundPoint(const Vector3f& point, float deltaAzimuth, float deltaElevation) {
    if (!m_camera) return;

    Vector3f currentPos = m_camera->getPosition();
    Vector3f toPoint = point - currentPos;
    float distance = toPoint.length();

    // Convert current position to spherical coordinates relative to point
    Vector3f offset = currentPos - point;
    float currentAzimuth = std::atan2(offset.getZ(), offset.getX());
    float currentElevation = std::asin(offset.getY() / distance);

    // Apply deltas
    float newAzimuth = currentAzimuth + deltaAzimuth;
    float newElevation = std::max(-static_cast<float>((float)M_PI) / 2.0f + 0.01f,
        std::min(static_cast<float>((float)M_PI) / 2.0f - 0.01f, currentElevation + deltaElevation));

    // Set new position
    setOrbitCamera(point, distance, newAzimuth, newElevation);
}

void CameraController::updateFPSCamera(float deltaYaw, float deltaPitch, float deltaTime) {
    if (!m_camera) return;

    m_camera->updateFPSCamera(deltaYaw, deltaPitch, deltaTime);
}