
#include "CoreSystem/CoreSystem.h"
#include "ComponentSystem.h"

using namespace ComponentSystem;

REFL_DEFINE_OBJECT(Component)
	REFL_DEFINE_BOOL_MEMBER(Component, m_active),	
	REFL_DEFINE_COMPONENT_PTR_VECTOR_MEMBER(Component, m_children),
REFL_DEFINE_END

void Component::Update(double deltaTime)
{
	if (!m_initialized || !m_active) return;

	OnUpdate(deltaTime);
}


void Component::RemoveChild(Component* child)
{
	if (child) 
	{
		child->Shutdown();
		// Remove from children vector
		m_children.erase(std::remove(m_children.begin(), m_children.end(), child), m_children.end());
	}
}

void Component::Shutdown()
{
	auto* manager = Core::CoreSystem::GetComponentManager();
	if (m_initialized)
	{
		// Get ComponentManager reference for proper cleanup
		
		m_active = false;
		m_parent = nullptr;
		// Shutdown and release all children first
		for (auto& child : m_children) {
			if (child) {
				child->Shutdown();
				if (manager) {
					manager->ReleaseComponent(child);
				}
			}
		}
		m_children.clear();

		OnShutdown();
		m_initialized = false;
		manager->ReleaseComponent(this);
	}
	
}

