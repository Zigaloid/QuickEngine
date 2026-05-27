#include "LightComponent.h"
#include "TransformComponent.h"

REGISTER_COMPONENT(CLightComponent, "Light", "Graphics");

REFL_DEFINE_OBJECT(CLightComponent)
REFL_DEFINE_VECTOR3_MEMBER(CLightComponent, m_direction),
REFL_DEFINE_VECTOR4_MEMBER(CLightComponent, m_color),
REFL_DEFINE_VECTOR4_MEMBER(CLightComponent, m_ambientColor)
REFL_DEFINE_END

bool CLightComponent::OnInitialize()
{
    m_direction = m_direction.Normalized();
    m_transform = FindParentTransform(this);
    return true;
}

void CLightComponent::OnUpdate(double /*deltaTime*/)
{
    if (m_transform)
        UpdateDirectionFromTransform();
}

void CLightComponent::OnShutdown()
{
    m_transform = nullptr;
}

void CLightComponent::UpdateDirectionFromTransform()
{
    // Column 2 of the transform matrix is the local Z (forward) axis.
    m_direction = -m_transform->GetTransform().GetColumn(1).Normalized();
}