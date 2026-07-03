#include "pch.h"
#include "CauseAndEventManagerComponent.h"
#include "CoreSystem/CoreSystem.h"
#include "Profiler/Profiler.h"

REGISTER_COMPONENT(CCauseAndEventManagerComponent, "Cause and Event Manager", "System");

REFL_DEFINE_OBJECT(CCauseAndEventManagerComponent)
REFL_DEFINE_END

bool CCauseAndEventManagerComponent::OnInitialize()
{
    DECLARE_FUNC_VLOW();
    return true;
}

void CCauseAndEventManagerComponent::OnUpdate(double deltaTime)
{
    DECLARE_FUNC_MEDIUM();

    for (auto it = m_definitions.begin(); it != m_definitions.end(); ++it)
    {
        if (*it)
            (*it)->CheckAndFire(deltaTime);
    }
}

void CCauseAndEventManagerComponent::OnShutdown()
{
    DECLARE_FUNC_VLOW();
    m_definitions.clear();
    Component::OnShutdown();
}

CCauseAndEventManagerComponent* CCauseAndEventManagerComponent::Get()
{
    return Core::CoreSystem::GetFirstActiveComponentOfType<CCauseAndEventManagerComponent>();
}

CauseAndEventSystem::CauseEventDefinition* CCauseAndEventManagerComponent::CreateDefinition()
{
    auto def = std::make_shared<CauseAndEventSystem::CauseEventDefinition>();
    m_definitions.push_back(def);
    return def.get();
}

void CCauseAndEventManagerComponent::RemoveDefinition(CauseAndEventSystem::CauseEventDefinition* def)
{
    for (auto it = m_definitions.begin(); it != m_definitions.end(); ++it)
    {
        if (it->get() == def)
        {
            m_definitions.erase(it);
            return;
        }
    }
}

void CCauseAndEventManagerComponent::ClearAllDefinitions()
{
    m_definitions.clear();
}

