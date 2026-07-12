#include "BgfxUIView.h"
#include <bx/math.h>

namespace Rendering {

// ── Initialisation / Shutdown ────────────────────────────────────────────

bool BgfxUIView::Initialize(uint16_t width, uint16_t height)
{
    if (m_initialized)
        return true;

    m_width  = width  > 0 ? width  : 1;
    m_height = height > 0 ? height : 1;

    bgfx::setViewName(kUIViewID, "UIView");
    bgfx::setViewName(kTextViewID, "TextView");
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

    // Aspect-compensated view matrix: scales Y by aspect = width/height so a
    // unit NDC distance maps to the same number of screen pixels in X and Y.
    // This keeps rotated UI quads visually square on non-square viewports.
    // Every NDC<->screen-pixel conversion (gizmo's NdcToScreen/ScreenToNdc,
    // CUITextComponent's pen-position conversion, the selection-highlight
    // NdcToScreenPx lambda) must use the SAME aspect term to stay glued to
    // the rendered quad.
    m_aspect = float(m_width) / float(m_height);
    float view[16];
    bx::mtxScale(view, 1.0f, m_aspect, 1.0f);
    bgfx::setViewTransform(kUIViewID, view, nullptr);

    // Ensure the view is submitted even when no UI is drawn this frame.
    bgfx::touch(kUIViewID);

	// ── Text view setup ─────────────────────────────────────────────────
	bgfx::setViewRect(kTextViewID, 0, 0, m_width, m_height);
	bgfx::setViewFrameBuffer(kTextViewID, BGFX_INVALID_HANDLE);
	bgfx::setViewClear(kTextViewID, BGFX_CLEAR_DEPTH, 0, 1.0f, 0);

	// Orthographic projection so text pen positions are in pixel coordinates:
	// (0,0) = top-left, (width,height) = bottom-right.
	{
		float ortho[16];
		bx::mtxOrtho(ortho, 0.0f, (float)m_width, (float)m_height, 0.0f, 0.0f, 1000.0f,
			0.0f, bgfx::getCaps()->homogeneousDepth);
		bgfx::setViewTransform(kTextViewID, nullptr, ortho);
	}

	bgfx::touch(kTextViewID);
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

    // Aspect-compensated view matrix — see first overload for rationale.
    m_aspect = float(m_width) / float(m_height);
    float view[16];
    bx::mtxScale(view, 1.0f, m_aspect, 1.0f);
    bgfx::setViewTransform(kUIViewID, view, nullptr);

    bgfx::touch(kUIViewID);

	// ── Text view setup (offscreen framebuffer) ──────────────────────────
	bgfx::setViewRect(kTextViewID, 0, 0, m_width, m_height);
	bgfx::setViewFrameBuffer(kTextViewID, fbh);
	bgfx::setViewClear(kTextViewID, BGFX_CLEAR_DEPTH, 0, 1.0f, 0);

	{
		float ortho[16];
		bx::mtxOrtho(ortho, 0.0f, (float)m_width, (float)m_height, 0.0f, 0.0f, 1000.0f,
			0.0f, bgfx::getCaps()->homogeneousDepth);
		bgfx::setViewTransform(kTextViewID, nullptr, ortho);
	}

	bgfx::touch(kTextViewID);
}

} // namespace Rendering