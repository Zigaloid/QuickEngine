#include "component.h"
#include "CoreSystem\CoreSystem.h"

using namespace ComponentSystem;

REFL_DEFINE_OBJECT(Component)
REFL_DEFINE_BOOL_MEMBER(Component, m_active),
REFL_DEFINE_COMPONENT_SHARED_PTR_VECTOR_MEMBER(Component, m_children),
REFL_DEFINE_END

void Component::Update(double deltaTime)
{
	if (!m_initialized || !m_active) return;

	OnUpdate(deltaTime);

	for (auto& child : m_children)
	{
		if (child)
			child->Update(deltaTime);
	}
}

void Component::AddChild(Component* child)
{
	if (child)
	{
		child->m_parent = weak_from_this();
		if (m_initialized)
			child->Initialize();
		auto* manager = Core::CoreSystem::GetComponentManager();
		if (manager)
		{
			if (auto sp = manager->GetSharedPtr(child))
			{
				m_children.push_back(std::move(sp));
				return;
			}
		}
		m_children.push_back(std::shared_ptr<Component>(child));
	}
}

void Component::RemoveChild(Component* child)
{
	if (child)
	{
		child->Shutdown();
		m_children.erase(
			std::remove_if(m_children.begin(), m_children.end(),
				[child](const std::shared_ptr<Component>& sp) { return sp.get() == child; }),
			m_children.end());
	}
}