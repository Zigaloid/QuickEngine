#include "UIElementComponent.h"

#include "CoreSystem/CoreSystem.h"
#include "ComponentSystem/ComponentSystem.h"

#include <bgfx/embedded_shader.h>
#include "bgfx_utils.h"

#include "../ShaderSource/ui/vs_ui.bin.h"
#include "../ShaderSource/ui/fs_ui.bin.h"

#include <iostream>

// ── Component registration ────────────────────────────────────────────────
REGISTER_COMPONENT(CUIElementComponent, "UIElement", "UI");
REGISTER_COMPONENT(CUIImageComponent,   "UIImage",   "UI");

REFL_DEFINE_OBJECT(CUIElementComponent)
REFL_DEFINE_END

REFL_DEFINE_OBJECT(CUIImageComponent)
    REFL_DEFINE_OBJECT_MEMBER(CUIImageComponent, m_textureResource),
REFL_DEFINE_END

// ── Shared static state definitions ───────────────────────────────────────
bgfx::VertexLayout      UIElementVertex::ms_layout;
bgfx::ProgramHandle      CUIElementComponent::s_uiProgram    = BGFX_INVALID_HANDLE;
bgfx::UniformHandle     CUIElementComponent::s_tintUniform  = BGFX_INVALID_HANDLE;
bgfx::UniformHandle     CUIElementComponent::s_texSampler   = BGFX_INVALID_HANDLE;
bgfx::VertexBufferHandle CUIElementComponent::s_quadVbh     = BGFX_INVALID_HANDLE;
bgfx::IndexBufferHandle  CUIElementComponent::s_quadIbh      = BGFX_INVALID_HANDLE;
bool                    CUIElementComponent::s_uiInitialised = false;

static const bgfx::EmbeddedShader s_uiShaders[] =
{
    BGFX_EMBEDDED_SHADER(vs_ui),
    BGFX_EMBEDDED_SHADER(fs_ui),
    BGFX_EMBEDDED_SHADER_END()
};

// ── CUIElementComponent ───────────────────────────────────────────────────

bool CUIElementComponent::OnInitialize()
{
    DECLARE_FUNC_VLOW();
    if (!CRenderComponent::OnInitialize())
        return false;
    return true;
}

void CUIElementComponent::OnUpdate(double deltaTime)
{
    DECLARE_FUNC_MEDIUM();
    CRenderComponent::OnUpdate(deltaTime);

    if (!s_uiInitialised)
        return;

    auto* renderFunctionQueue = Core::CoreSystem::GetRenderFunctionQueue();
    if (renderFunctionQueue)
    {
        renderFunctionQueue->AddFunction([this]()
            {
                Render(Rendering::BgfxUIView::GetUIViewID());
            }, "CUIElementComponent::Render");
    }
}

void CUIElementComponent::OnShutdown()
{
    DECLARE_FUNC_VLOW();
    CRenderComponent::OnShutdown();
}

void CUIElementComponent::Render(bgfx::ViewId viewId)
{
    // Base implementation draws an untextured tinted quad — subclasses bind
    // a texture and then call SubmitQuad().
    if (!s_uiInitialised || viewId == Rendering::BgfxViewIdAllocator::kInvalidViewId)
        return;

    auto modelMatrix = GetModelMatrix();
    if (!modelMatrix)
        return;

    bgfx::setTexture(0, s_texSampler, BGFX_INVALID_HANDLE);
    SubmitQuad(viewId);
}

void CUIElementComponent::SubmitQuad(bgfx::ViewId viewId)
{
    auto modelMatrix = GetModelMatrix();
    if (!modelMatrix)
        return;

    bgfx::setUniform(s_tintUniform, m_tint.data());

    bgfx::setTransform(modelMatrix->GetData().data());
    bgfx::setVertexBuffer(0, s_quadVbh);
    bgfx::setIndexBuffer(s_quadIbh);

    // No culling — UI quads are double-sided. Blend alpha for transparency.
    // Depth write + depth test lets overlapping UI elements resolve order
    // via the model matrix's Z output.
    bgfx::setState(BGFX_STATE_WRITE_RGB
                 | BGFX_STATE_WRITE_A
                 | BGFX_STATE_WRITE_Z
                 | BGFX_STATE_DEPTH_TEST_LESS
                 | BGFX_STATE_BLEND_ALPHA
                 | BGFX_STATE_MSAA);

    bgfx::submit(viewId, s_uiProgram);
}

// ── Shared UI rendering init/shutdown ─────────────────────────────────────

bool CUIElementComponent::InitializeUIRendering()
{
    DECLARE_FUNC_VLOW();
    if (s_uiInitialised)
        return true;

    UIElementVertex::Init();

    const bgfx::RendererType::Enum type = bgfx::getRendererType();
    bgfx::ShaderHandle vsh = bgfx::createEmbeddedShader(s_uiShaders, type, "vs_ui");
    bgfx::ShaderHandle fsh = bgfx::createEmbeddedShader(s_uiShaders, type, "fs_ui");
    s_uiProgram = bgfx::createProgram(vsh, fsh, true);
    if (!bgfx::isValid(s_uiProgram))
    {
        std::cerr << "CUIElementComponent: failed to create UI shader program" << std::endl;
        return false;
    }

    s_tintUniform = bgfx::createUniform("u_tint",     bgfx::UniformType::Vec4);
    s_texSampler  = bgfx::createUniform("s_texColor", bgfx::UniformType::Sampler);

    // Unit quad centred at origin in the local XY plane, Z = 0.
    // Texcoords are (0,0) top-left .. (1,1) bottom-right, matching bgfx's
    // default texture addressing (UV origin in the top-left for DDS uploads).
    static const UIElementVertex quadVerts[4] = {
        { -0.5f,  0.5f, 0.0f, 0.0f, 0.0f }, // top-left
        {  0.5f,  0.5f, 0.0f, 1.0f, 0.0f }, // top-right
        {  0.5f, -0.5f, 0.0f, 1.0f, 1.0f }, // bottom-right
        { -0.5f, -0.5f, 0.0f, 0.0f, 1.0f }, // bottom-left
    };
    static const uint16_t quadIndices[6] = {
        0, 1, 2,
        0, 2, 3,
    };

    s_quadVbh = bgfx::createVertexBuffer(
        bgfx::makeRef(quadVerts, sizeof(quadVerts)),
        UIElementVertex::ms_layout);

    s_quadIbh = bgfx::createIndexBuffer(
        bgfx::makeRef(quadIndices, sizeof(quadIndices)));

    s_uiInitialised = true;
    return true;
}

void CUIElementComponent::ShutdownUIRendering()
{
    DECLARE_FUNC_VLOW();

    if (bgfx::isValid(s_quadIbh))     { bgfx::destroy(s_quadIbh);     s_quadIbh     = BGFX_INVALID_HANDLE; }
    if (bgfx::isValid(s_quadVbh))     { bgfx::destroy(s_quadVbh);     s_quadVbh     = BGFX_INVALID_HANDLE; }
    if (bgfx::isValid(s_texSampler))  { bgfx::destroy(s_texSampler);  s_texSampler  = BGFX_INVALID_HANDLE; }
    if (bgfx::isValid(s_tintUniform)) { bgfx::destroy(s_tintUniform); s_tintUniform = BGFX_INVALID_HANDLE; }
    if (bgfx::isValid(s_uiProgram))   { bgfx::destroy(s_uiProgram);   s_uiProgram   = BGFX_INVALID_HANDLE; }

    s_uiInitialised = false;
}

// ── CUIImageComponent ─────────────────────────────────────────────────────

bool CUIImageComponent::OnInitialize()
{
    DECLARE_FUNC_VLOW();
    if (!CUIElementComponent::OnInitialize())
        return false;
    return true;
}

void CUIImageComponent::OnShutdown()
{
    DECLARE_FUNC_VLOW();
    m_textureResource = CTextureResourceReference();
    CUIElementComponent::OnShutdown();
}

bool CUIImageComponent::IsLoaded() const
{
    if (!m_textureResource.GetResource())
        return false;
    auto texRes = m_textureResource.GetResourceAs<CTextureResource>();
    return texRes && texRes->IsLoaded() && texRes->IsFinalized()
        && bgfx::isValid(texRes->GetTextureHandle());
}

void CUIImageComponent::Render(bgfx::ViewId viewId)
{
    if (!s_uiInitialised || viewId == Rendering::BgfxViewIdAllocator::kInvalidViewId)
        return;
    if (!IsLoaded())
        return;

    auto texRes = m_textureResource.GetResourceAs<CTextureResource>();
    bgfx::setTexture(0, s_texSampler, texRes->GetTextureHandle());

    SubmitQuad(viewId);
}