#pragma once
#include <vector>
#include <string>
#include "ComponentSystem.h"
#include "Reflection/ReflectionBase.h"

namespace Core {
	class CoreSystem;
}
/**
 * @brief Information about a component type that can be created.
 */
struct ComponentTypeInfo
{
	std::string className;      // Class name for ClassFactory::CreateObject
	std::string displayName;    // Human-readable name for UI
	std::string category;       // Optional category for grouping
};

/**
 * @brief Registry of component types that can be added to component arrays.
 */

class ComponentRegistry
{
public:
	void Register(const std::string& className, const std::string& displayName, const std::string& category = "");
	void Unregister(const std::string& className);
	void Clear();
	const std::vector<ComponentTypeInfo>& GetAll() const { return m_types; }
	const ComponentTypeInfo* Find(const std::string& className) const;

private:
	std::vector<ComponentTypeInfo> m_types;
};

/** @brief RAII helper that self-registers a component type at static init time. */
class AutoRegisterComponent
{
public:
	AutoRegisterComponent(const char* className, const char* prettyName, const char* category);
};

ComponentRegistry& GetComponentRegistry();

#define DECLARE_COMPONENT() static AutoRegisterComponent s_autoRegister;
#define REGISTER_COMPONENT(_className, _prettyName, _category) AutoRegisterComponent _className::s_autoRegister(#_className, _prettyName, _category);
