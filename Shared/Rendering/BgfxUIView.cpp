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

	// View scaling compensates for the non-square viewport so that 2D
	// rotations appear correct on screen.  Without it a unit square in NDC
	// would map to different pixel sizes in X and Y, shearing any rotated
	// UI element.  The view scales Y so that NDC unit distances produce
	// equal screen-pixel distances in both axes.
	// Applies to both overloads — editor offscreen framebuffer and game
	// backbuffer — so rotation renders consistently everywhere.
	float view[16];
	float aspect = (float)width / (float)height;
	bx::mtxScale(view, 1.0f, aspect, 1.0f);
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

	// View scaling compensates for the non-square viewport (see first overload).
	float view[16];
	float aspect = (float)width / (float)height;
	bx::mtxScale(view, 1.0f, aspect, 1.0f);
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