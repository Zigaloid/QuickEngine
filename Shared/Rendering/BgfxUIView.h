#pragma once

#include <cstdint>
#include <bgfx/bgfx.h>

#include "BgfxViewIdAllocator.h"

namespace Rendering {

/**
 * @brief Owns a dedicated BGFX view used to draw 2D UI elements on top of the
 *        3D scene.
 *
 * The UI view shares the backbuffer with the main scene pass (view 0). It
 * sets an identity view and identity projection, so vertex positions output
 * by the model matrix are already in clip-space NDC. The Z component of the
 * transformed position is written to the depth buffer, which lets multiple
 * overlapping UI elements resolve draw order via depth testing rather than
 * submission order.
 *
 * Each frame the view's depth buffer is cleared (color is preserved so the
 * scene shows through). The view is allocated with a higher view ID than
 * view 0 so bgfx renders it after the scene.
 *
 * Typical usage:
 * @code
 *   BgfxUIView::Instance().Initialize(width, height);
 *   // ... each frame, after bgfx::touch(0):
 *   BgfxUIView::Instance().UpdateView(width, height);
 *   // UIElementComponent subclasses submit into BgfxUIView::GetUIViewID().
 *   bgfx::frame();
 * @endcode
 */
class BgfxUIView
{
public:
    /// Fixed view ID reserved for the UI overlay.  It sits above the
    /// BgfxViewIdAllocator's range (1–199) and below the ImGui overlay
    /// (255) so bgfx always renders the UI pass AFTER every 3D viewport
    /// pass, preventing the 3D scene's colour-clear from overwriting UI
    /// elements.
    static constexpr bgfx::ViewId kUIViewID = 200;

    static BgfxUIView& Instance()
    {
        static BgfxUIView instance;
        return instance;
    }

    ~BgfxUIView() { Shutdown(); }

    BgfxUIView(const BgfxUIView&) = delete;
    BgfxUIView& operator=(const BgfxUIView&) = delete;
    BgfxUIView(BgfxUIView&&) = delete;
    BgfxUIView& operator=(BgfxUIView&&) = delete;

    /**
     * @brief Reserve the fixed UI view ID and name the view.
     * @return true on success.
     */
    bool Initialize(uint16_t width, uint16_t height);

    /**
     * @brief Release the view. Safe to call multiple times.
     */
    void Shutdown();

    /**
     * @brief Configure the view for the current frame (backbuffer target).
     *
     * Sets the viewport rect, switches the view to clear only the depth
     * buffer (color preserved from the scene pass), and installs identity
     * view + projection matrices. Must be called after `bgfx::touch(0)`
     * and before `bgfx::frame()`.
     */
    void UpdateView(uint16_t width, uint16_t height);

    /**
     * @brief Configure the view for the current frame (offscreen framebuffer).
     *
     * Same as the backbuffer overload but binds @p fbh as the render target
     * instead of the default backbuffer. Used by editor 3D viewports so UI
     * elements render inside the viewport panel rather than over the whole
     * editor window.
     */
    void UpdateView(uint16_t width, uint16_t height, bgfx::FrameBufferHandle fbh);

    /** @brief The fixed bgfx view ID used for UI rendering. */
    static bgfx::ViewId GetUIViewID() { return kUIViewID; }

    bool IsValid() const { return m_initialized; }

private:
    BgfxUIView() = default;

    uint16_t     m_width  = 0;
    uint16_t     m_height = 0;
    bool         m_initialized = false;
};

} // namespace Rendering