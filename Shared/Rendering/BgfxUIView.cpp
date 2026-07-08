#include "BgfxUIView.h"

namespace Rendering {

// ── Initialisation / Shutdown ────────────────────────────────────────────

bool BgfxUIView::Initialize(uint16_t width, uint16_t height)
{
    if (m_initialized)
        return true;

    m_width  = width  > 0 ? width  : 1;
    m_height = height > 0 ? height : 1;

    bgfx::setViewName(kUIViewID, "UIView");
    m_initialized = true;
    return true;
}

void BgfxUIView::Shutdown()
{
    m_initialized = false;
}

// ── Per-frame view setup ─────────────────────────────────────────────────

void BgfxUIView::UpdateView(uint16_t width, uint16_t height)
{
    if (!m_initialized)
        return;

    m_width  = width  > 0 ? width  : 1;
    m_height = height > 0 ? height : 1;

    // Full backbuffer rect.
    bgfx::setViewRect(kUIViewID, 0, 0, m_width, m_height);

    // Backbuffer is the render target (no framebuffer binding needed).
    bgfx::setViewFrameBuffer(kUIViewID, BGFX_INVALID_HANDLE);

    // Clear the depth buffer only — the colour from the scene pass is preserved
    // so UI elements overlay the 3D scene.
    bgfx::setViewClear(kUIViewID, BGFX_CLEAR_DEPTH, 0, 1.0f, 0);

    // Identity view and identity projection: the model matrix alone carries
    // vertices into clip-space NDC. The third row of the model matrix thus
    // controls the depth written to the depth buffer.
    bgfx::setViewTransform(kUIViewID, nullptr, nullptr);

    // Ensure the view is submitted even when no UI is drawn this frame.
    bgfx::touch(kUIViewID);
}

void BgfxUIView::UpdateView(uint16_t width, uint16_t height, bgfx::FrameBufferHandle fbh)
{
    if (!m_initialized)
        return;

    m_width  = width  > 0 ? width  : 1;
    m_height = height > 0 ? height : 1;

    // Full framebuffer rect.
    bgfx::setViewRect(kUIViewID, 0, 0, m_width, m_height);

    // Bind the offscreen framebuffer as the render target.
    bgfx::setViewFrameBuffer(kUIViewID, fbh);

    // Clear the depth buffer only — colour from the 3D scene pass is preserved.
    bgfx::setViewClear(kUIViewID, BGFX_CLEAR_DEPTH, 0, 1.0f, 0);

    // Identity view and identity projection.
    bgfx::setViewTransform(kUIViewID, nullptr, nullptr);

    bgfx::touch(kUIViewID);
}

} // namespace Rendering