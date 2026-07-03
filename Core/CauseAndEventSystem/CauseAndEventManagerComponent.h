#pragma once
#include "ComponentSystem/ComponentSystem.h"
#include "CauseEventDefinition.h"
#include <vector>
#include <memory>

class CCauseAndEventManagerComponent : public ComponentSystem::Component
{
public:
    REFL_DECLARE_OBJECT(CCauseAndEventManagerComponent, ComponentSystem::Component);
    DECLARE_COMPONENT();

    bool OnInitialize() override;
    void OnUpdate(double deltaTime) override;
    void OnShutdown() override;

    static CCauseAndEventManagerComponent* Get();

    CauseAndEventSystem::CauseEventDefinition* CreateDefinition();
    void RemoveDefinition(CauseAndEventSystem::CauseEventDefinition* def);
    void ClearAllDefinitions();

    const std::vector<std::shared_ptr<CauseAndEventSystem::CauseEventDefinition>>& GetDefinitions() const { return m_definitions; }

private:
    std::vector<std::shared_ptr<CauseAndEventSystem::CauseEventDefinition>> m_definitions;
};
