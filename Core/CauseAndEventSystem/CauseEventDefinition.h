#pragma once
#include "Cause.h"
#include "Event.h"
#include <vector>

namespace CauseAndEventSystem
{

class CauseEventDefinition
{
public:
    CauseEventDefinition() = default;
    ~CauseEventDefinition() = default;

    CauseEventDefinition(const CauseEventDefinition&) = delete;
    CauseEventDefinition& operator=(const CauseEventDefinition&) = delete;
    CauseEventDefinition(CauseEventDefinition&&) = default;
    CauseEventDefinition& operator=(CauseEventDefinition&&) = default;

    void SetCause(std::shared_ptr<Cause> cause) { m_cause = std::move(cause); }
    void AddEvent(std::shared_ptr<Event> event) { m_events.push_back(std::move(event)); }
    void RemoveEvent(Event* event);
    void ClearEvents() { m_events.clear(); }

    bool CheckAndFire(double deltaTime);
    void Reset();

    Cause* GetCause() const { return m_cause.get(); }
    const std::vector<std::shared_ptr<Event>>& GetEvents() const { return m_events; }

    bool IsTriggered() const { return m_triggered; }

private:
    std::shared_ptr<Cause> m_cause;
    std::vector<std::shared_ptr<Event>> m_events;
    bool m_triggered = false;
};

}
