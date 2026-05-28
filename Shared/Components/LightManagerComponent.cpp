#include "LightManagerComponent.h"
#include "CoreSystem/CoreSystem.h"

REGISTER_COMPONENT(CLightManagerComponent, "LightManager", "Graphics");

REFL_DEFINE_OBJECT(CLightManagerComponent)
REFL_DEFINE_END

bool CLightManagerComponent::OnInitialize()
{
    RefreshFromScene();
    return true;
}

void CLightManagerComponent::OnUpdate(double /*deltaTime*/)
{
    RefreshFromScene();
}

void CLightManagerComponent::OnShutdown()
{
    m_primaryLight = nullptr;
    Component::OnShutdown();
}

CLightManagerComponent* CLightManagerComponent::Get()
{
    return Core::CoreSystem::GetFirstActiveComponentOfType<CLightManagerComponent>();
}

void CLightManagerComponent::RefreshFromScene()
{
    // Walk up to the scene root so we can search the full hierarchy.
    ComponentSystem::Component* root = this;
    while (root->GetParent())
        root = root->GetParent();

    auto lights = root->FindDescendants<CLightComponent>();

    m_primaryLight = nullptr;
    for (CLightComponent* light : lights)
    {
        if (!light || !light->IsActive()) continue;
        m_primaryLight = light;
        break;
    }



    if (m_primaryLight)
    {
        m_primaryLight->OnUpdate(0.0f);
        m_resolvedLightDir = m_primaryLight->GetDirectionVec4();
        m_resolvedLightColor = m_primaryLight->GetColor();
        m_resolvedAmbientColor = m_primaryLight->GetAmbientColor();
    }
    else
    {
        // Fall back to sensible defaults when no light is present in the scene.
        m_resolvedLightDir = Vector4f(0.57735f, -0.57735f, 0.57735f, 0.0f);
        m_resolvedLightColor = Vector4f(1.0f, 1.0f, 1.0f, 1.0f);
        m_resolvedAmbientColor = Vector4f(0.15f, 0.15f, 0.15f, 1.0f);
    }
}