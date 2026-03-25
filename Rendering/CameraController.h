#pragma once

#include <memory>
#include "Camera.h"
#include "math\Vector3f.h"

class CameraController {
public:
    struct ControlSettings {
        float rotationSpeed = 1.0f;
        float panSpeed = 1.0f;
        float zoomSpeed = 1.0f;
        float movementSpeed = 5.0f;
    };

private:
    std::unique_ptr<Camera> m_camera;
    ControlSettings m_settings;

    // Input state
    bool m_isDragging;
    double m_lastMouseX, m_lastMouseY;

public:
    CameraController();
    explicit CameraController(std::unique_ptr<Camera> camera);
    ~CameraController() = default;

    // Camera access
    Camera* getCamera() const { return m_camera.get(); }
    void setCamera(std::unique_ptr<Camera> camera) { m_camera = std::move(camera); }

    // Control settings
    const ControlSettings& getSettings() const { return m_settings; }
    void setSettings(const ControlSettings& settings) { m_settings = settings; }

    float getRotationSpeed() const { return m_settings.rotationSpeed; }
    void setRotationSpeed(float speed) { m_settings.rotationSpeed = speed; }

    float getPanSpeed() const { return m_settings.panSpeed; }
    void setPanSpeed(float speed) { m_settings.panSpeed = speed; }

    float getZoomSpeed() const { return m_settings.zoomSpeed; }
    void setZoomSpeed(float speed) { m_settings.zoomSpeed = speed; }

    float getMovementSpeed() const { return m_settings.movementSpeed; }
    void setMovementSpeed(float speed) { m_settings.movementSpeed = speed; }

    // Camera utility methods
    void resetCamera();
    void focusOnPoint(const Vector3f& point, float distance = 10.0f);
    void fitBounds(const Vector3f& minBounds, const Vector3f& maxBounds);

    // Preset camera positions
    void setTopView();
    void setFrontView();
    void setSideView();
    void setIsometricView();

    // Advanced camera controls
    void setOrbitCamera(const Vector3f& target, float distance, float azimuth, float elevation);
    void orbitAroundPoint(const Vector3f& point, float deltaAzimuth, float deltaElevation);
    void updateFPSCamera(float deltaYaw, float deltaPitch, float deltaTime);

    // Update aspect ratio when viewport changes
    void updateAspectRatio(float aspectRatio);
};