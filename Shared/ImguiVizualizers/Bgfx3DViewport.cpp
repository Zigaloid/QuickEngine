#include "Bgfx3DViewport.h"

namespace ImGuiVisualizers {

bool Bgfx3DViewport::Initialize(uint16_t width, uint16_t height)
{
    if (m_initialized)
        return true;

    m_viewId = BgfxViewIdAllocator::Get().Allocate();
    if (m_viewId == BgfxViewIdAllocator::kInvalidViewId)
        return false;

    m_width  = width  > 0 ? width  : 1;
    m_height = height > 0 ? height : 1;

    CreateFramebuffer();
    m_initialized = true;
    return true;
}

void Bgfx3DViewport::Shutdown()
{
    if (!m_initialized)
        return;

    DestroyFramebuffer();

    if (m_viewId != BgfxViewIdAllocator::kInvalidViewId) {
        BgfxViewIdAllocator::Get().Free(m_viewId);
        m_viewId = BgfxViewIdAllocator::kInvalidViewId;
    }

    m_initialized = false;
}

void Bgfx3DViewport::Resize(uint16_t width, uint16_t height)
{
    width  = width  > 0 ? width  : 1;
    height = height > 0 ? height : 1;

    if (width == m_width && height == m_height)
        return;

    m_width  = width;
    m_height = height;

    DestroyFramebuffer();
    CreateFramebuffer();
}

void Bgfx3DViewport::BeginFrame(const float* viewMtx, const float* projMtx,
                                 uint32_t clearColor)
{
    if (!m_initialized)
        return;

    bgfx::setViewName(m_viewId, "3DViewport");
    bgfx::setViewRect(m_viewId, 0, 0, m_width, m_height);
    bgfx::setViewClear(m_viewId,
                       BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH,
                       clearColor,
                       1.0f,
                       0);
    bgfx::setViewFrameBuffer(m_viewId, m_fbh);
    bgfx::setViewTransform(m_viewId, viewMtx, projMtx);

    // Ensure the view is submitted even if nothing is drawn.
    bgfx::touch(m_viewId);
}

// ── Private helpers ─────────────────────────────────────────────────────

void Bgfx3DViewport::CreateFramebuffer()
{
    const uint64_t texFlags = BGFX_TEXTURE_RT
                            | BGFX_SAMPLER_U_CLAMP
                            | BGFX_SAMPLER_V_CLAMP;

    m_colorTex = bgfx::createTexture2D(
        m_width, m_height, false, 1,
        bgfx::TextureFormat::RGBA8,
        texFlags);

    bgfx::TextureHandle depthTex = bgfx::createTexture2D(
        m_width, m_height, false, 1,
        bgfx::TextureFormat::D24S8,
        texFlags);

    bgfx::TextureHandle attachments[2] = { m_colorTex, depthTex };
    m_fbh = bgfx::createFrameBuffer(2, attachments, true);  // auto-destroy depth
}

void Bgfx3DViewport::DestroyFramebuffer()
{
    if (bgfx::isValid(m_fbh)) {
        bgfx::destroy(m_fbh);   // destroys the depth attachment (owned)
        m_fbh = BGFX_INVALID_HANDLE;
    }

    // Color texture is NOT destroyed by the framebuffer (we keep a ref to
    // it for ImGui::Image), so we destroy it explicitly here.
    if (bgfx::isValid(m_colorTex)) {
        bgfx::destroy(m_colorTex);
        m_colorTex = BGFX_INVALID_HANDLE;
    }
}

} // namespace ImGuiVisualizers
