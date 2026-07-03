#include "pch.h"

#include "CauseEventDefinition.h"

namespace CauseAndEventSystem
{

    void CauseEventDefinition::RemoveEvent(Event* event)
    {
        for (auto it = m_events.begin(); it != m_events.end(); ++it)
        {
            if (it->get() == event)
            {
                m_events.erase(it);
                return;
            }
        }
    }

    bool CauseEventDefinition::CheckAndFire(double deltaTime)
    {
        if (m_triggered)
            return true;

        if (!m_cause)
            return false;

        if (!m_cause->CheckCondition(deltaTime))
            return false;

        m_triggered = true;

        for (auto& event : m_events)
        {
            if (event)
                event->Execute();
        }

        return true;
    }

    void CauseEventDefinition::Reset()
    {
        m_triggered = false;
        if (m_cause)
            m_cause->Reset();
    }

}