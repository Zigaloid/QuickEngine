#include "UIImageComponent.h"
#include "UI/UIRenderer.h"
#include "UI/UIGeometry.h"

REFL_DEFINE_OBJECT(CUIImageComponent)
REFL_DEFINE_END

REGISTER_COMPONENT(CUIImageComponent, "UIImageComponent", "UI");

bool CUIImageComponent::OnInitialize()
{
    return CUIElementComponent::OnInitialize();
}

void CUIImageComponent::OnUpdate(double deltaTime)
{
    CUIElementComponent::OnUpdate(deltaTime);
}

void CUIImageComponent::OnShutdown()
{
    CUIElementComponent::OnShutdown();
}

void CUIImageComponent::SetTextureResource(const CTextureResourceReference& textureRef)
{
    m_textureResource = textureRef;
}

void CUIImageComponent::Render(bgfx::ViewId viewId)
{
    if (!IsVisible() || m_alpha <= 0.0f)
        return;

    // For now, render a simple colored quad
    // In Phase 4, we'll add proper texture rendering

    const Matrix4f& worldTransform = GetWorldTransform();
    Vector3f worldPos = worldTransform.TransformPoint(Vector3f(0, 0, 0));

    // Convert alpha to color multiplier
    uint32_t color = UI::ToABGR(m_tint.x, m_tint.y, m_tint.z, m_tint.w * m_alpha);

    // Use UIRenderer singleton to submit geometry
    UI::UIRenderer::Instance().SubmitQuad(viewId, 
        Vector2f(worldPos.x, worldPos.y), 
        m_size, 
        color);
}
