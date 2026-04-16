#include "ComponentRegistry.h"
#include "CoreSystem/CoreSystem.h"
// ── ComponentRegistry ─────────────────────────────────────────────────────────

ComponentRegistry& GetComponentRegistry()
{
	static ComponentRegistry theRegistry;
	return theRegistry;
}

void ComponentRegistry::Register(const std::string& className, const std::string& displayName, const std::string& category)
{
	// Remove existing entry with the same class name
	std::erase_if(m_types, [&](const ComponentTypeInfo& t) {
		return t.className == className;
	});

	ComponentTypeInfo info;
	info.className = className;
	info.displayName = displayName;
	info.category = category;
	m_types.push_back(std::move(info));

	// Sort alphabetically by display name
	std::sort(m_types.begin(), m_types.end(),
		[](const ComponentTypeInfo& a, const ComponentTypeInfo& b) {
		return a.displayName < b.displayName;
	});
}

void ComponentRegistry::Unregister(const std::string& className)
{
	std::erase_if(m_types, [&](const ComponentTypeInfo& t) {
		return t.className == className;
	});
}

void ComponentRegistry::Clear()
{
	m_types.clear();
}

const ComponentTypeInfo* ComponentRegistry::Find(const std::string& className) const
{
	for (const auto& type : m_types) {
		if (type.className == className) {
			return &type;
		}
	}
	return nullptr;
}

AutoRegisterComponent::AutoRegisterComponent(const char* className, const char* prettyName, const char* category)
{
    GetComponentRegistry().Register(className, prettyName, category);
}