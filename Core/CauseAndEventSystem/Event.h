#pragma once
#include "ComponentSystem/Component.h"
#include <memory>

namespace CauseAndEventSystem
{

class Event
{
public:
    Event() = default;
    Event(const Event&) = delete;
    Event& operator=(const Event&) = delete;
    Event(Event&&) = default;
    Event& operator=(Event&&) = default;

    virtual ~Event() = default;

    virtual void Execute() = 0;
    virtual const char* GetEventName() const = 0;

    void SetOwnerComponent(ComponentSystem::Component* owner) { m_ownerComponent = owner; }
    ComponentSystem::Component* GetOwnerComponent() const { return m_ownerComponent; }

private:
    ComponentSystem::Component* m_ownerComponent = nullptr;
};

}
