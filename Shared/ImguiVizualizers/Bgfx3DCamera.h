#pragma once

#include <bgfx/bgfx.h>
#include <bx/math.h>
#include <cmath>

namespace ImGuiVisualizers {

/**
 * @brief Lightweight orbit camera that produces view / projection matrices
 *        in the format expected by bgfx::setViewTransform().
 *
 * Controls:
 *   - Orbit   : left-drag   (yaw / pitch around the target)
 *   - Pan     : middle-drag (translate the target in the camera plane)
 *   - Zoom    : scroll      (move distance closer/further)
 */
class Bgfx3DCamera {
public:
    Bgfx3DCamera()
        : m_target{0.0f, 0.0f, 0.0f}
        , m_yaw(0.4f)
        , m_pitch(0.3f)
        , m_distance(10.0f)
        , m_fovDeg(60.0f)
        , m_nearPlane(0.1f)
        , m_farPlane(1000.0f)
    {
    }

    void Reset()
    {
        m_target = { 0.0f, 0.0f, 0.0f };
        m_yaw       = 0.4f;
        m_pitch     = 0.3f;
        m_distance  = 10.0f;
        m_fovDeg    = 60.0f;
        m_nearPlane = 0.1f;
        m_farPlane  = 1000.0f;
    }

    // ── Input ───────────────────────────────────────────────────────────

    void Orbit(float deltaYaw, float deltaPitch)
    {
        m_yaw   += deltaYaw;
        m_pitch += deltaPitch;

        // Clamp pitch to avoid flipping
        const float limit = bx::kPi / 2.0f - 0.01f;
        if (m_pitch >  limit) m_pitch =  limit;
        if (m_pitch < -limit) m_pitch = -limit;
    }

    void Pan(float deltaRight, float deltaUp)
    {
        bx::Vec3 eye = CalcEyePosition();

        // Camera forward (target -> eye, normalized)
        bx::Vec3 fwd = bx::normalize(bx::sub(eye, m_target));

        // World up
        const bx::Vec3 worldUp = { 0.0f, 1.0f, 0.0f };

        // Right = worldUp x fwd
        bx::Vec3 right = bx::normalize(bx::cross(worldUp, fwd));

        // Up = fwd x right
        bx::Vec3 up = bx::normalize(bx::cross(fwd, right));

        m_target = bx::add(m_target, bx::add(
            bx::mul(right, deltaRight),
            bx::mul(up, deltaUp)));
    }

    void Zoom(float delta)
    {
        m_distance -= delta;
        if (m_distance < 0.1f) m_distance = 0.1f;
        if (m_distance > 500.0f) m_distance = 500.0f;
    }

    // ── Matrix output ───────────────────────────────────────────────────

    void GetViewMatrix(float* out16) const
    {
        bx::Vec3 eye = CalcEyePosition();
        const bx::Vec3 up = { 0.0f, 1.0f, 0.0f };
        bx::mtxLookAt(out16, eye, m_target, up);
    }

    void GetProjectionMatrix(float* out16, float aspectRatio) const
    {
        bx::mtxProj(out16, m_fovDeg, aspectRatio, m_nearPlane, m_farPlane,
                     bgfx::getCaps()->homogeneousDepth);
    }

    // ── Accessors ───────────────────────────────────────────────────────

    void  SetTarget(float x, float y, float z) { m_target = { x, y, z }; }
    void  SetDistance(float d) { m_distance = d; }
    float GetDistance() const  { return m_distance; }
    void  SetFov(float deg)   { m_fovDeg = deg; }
    float GetFov() const      { return m_fovDeg; }

    void GetEyePosition(float* out3) const
    {
        bx::Vec3 eye = CalcEyePosition();
        out3[0] = eye.x; out3[1] = eye.y; out3[2] = eye.z;
    }

private:
    bx::Vec3 CalcEyePosition() const
    {
        float cp = bx::cos(m_pitch);
        return {
            m_target.x + m_distance * cp * bx::sin(m_yaw),
            m_target.y + m_distance * bx::sin(m_pitch),
            m_target.z + m_distance * cp * bx::cos(m_yaw)
        };
    }

    bx::Vec3 m_target;
    float m_yaw;
    float m_pitch;
    float m_distance;
    float m_fovDeg;
    float m_nearPlane;
    float m_farPlane;
};

} // namespace ImGuiVisualizers
