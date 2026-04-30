#pragma once

#include "Rendering/BgfxGameCamera.h"
#include "Input/MouseInputHandler.h"

/**
 * @brief Drives a Bgfx3DCamera using the core MouseManager input pipeline.
 *
 *  Left-drag  → Orbit
 *  Middle-drag → Pan
 *  Scroll     → Zoom
 */
class GameCameraController : public Input::MouseInputHandler
{
public:
    explicit GameCameraController(BgfxGameCamera& camera)
        : Input::MouseInputHandler("GameCamera", /*priority=*/0, /*active=*/true)
        , m_camera(camera)
    {
    }

    bool HandleMovementInput(double /*x*/, double /*y*/,
                             float deltaX, float deltaY) override
    {
        if (m_leftDown)
            m_camera.Orbit(deltaX * 0.005f, deltaY * 0.005f);

        if (m_middleDown)
            m_camera.Pan(-deltaX * 0.01f, deltaY * 0.01f);

        return false; // don't consume — allow other handlers to see movement
    }

    bool HandleButtonInput(Input::MouseButton button, bool pressed,
                           bool /*justPressed*/, bool /*justReleased*/,
                           double /*x*/, double /*y*/) override
    {
        if (button == Input::MouseButton::Left)   m_leftDown   = pressed;
        if (button == Input::MouseButton::Middle) m_middleDown = pressed;
        return false;
    }

    bool HandleScrollInput(float /*deltaX*/, float deltaY) override
    {
        m_camera.Zoom(deltaY * 0.5f);
        return false;
    }

private:
    BgfxGameCamera& m_camera;
    bool m_leftDown   = false;
    bool m_middleDown = false;
};