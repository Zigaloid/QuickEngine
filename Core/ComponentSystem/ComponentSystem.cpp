#include "ComponentSystem.h"
#include "CoreSystem/CoreSystem.h"

using namespace ComponentSystem;

REFL_DEFINE_OBJECT(Component)
	REFL_DEFINE_BOOL_MEMBER(Component, m_active),
	REFL_DEFINE_COMPONENT_RAW_PTR_VECTOR_MEMBER(Component, m_children),
REFL_DEFINE_END

void Component::Update(double deltaTime)
{
	if (!m_initialized || !m_active) return;

	OnUpdate(deltaTime);

	for (auto& child : m_children)
	{
		if (child)
		{
			child->Update(deltaTime);
		}
	}
}

void Component::RemoveChild(Component* child)
{
	if (child)
	{
		child->Shutdown();
		m_children.erase(std::remove(m_children.begin(), m_children.end(), child), m_children.end());
	}
}

void ComponentSystem::Component::Shutdown()
{
    auto* manager = Core::CoreSystem::GetComponentManager();
    if (m_initialized)
    {
        m_active = false;
        m_parent = nullptr;

        for (auto& child : m_children)
        {
            if (child)
            {
                child->Shutdown();
                if (manager)
                {
                    manager->ReleaseComponent(child);
                }
            }
        }
        m_children.clear();

        OnShutdown();
        m_initialized = false;

        // Guard matches the child-release guard above – manager may be null
        // if CoreSystem has already been torn down.
        if (manager)
        {
            manager->ReleaseComponent(this);
        }
    }
}
