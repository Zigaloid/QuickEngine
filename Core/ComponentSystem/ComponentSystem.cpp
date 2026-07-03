#include "pch.h"
#include "ComponentSystem.h"
#include "CoreSystem/CoreSystem.h"

void ComponentSystem::Component::Shutdown()
{
	if (m_initialized)
	{
		m_active = false;
		m_parent.reset();

		for (auto& child : m_children)
		{
			if (child)
				child->Shutdown();
		}
		m_children.clear();

		OnShutdown();
		m_initialized = false;
	}
}
