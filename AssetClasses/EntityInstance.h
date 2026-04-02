#pragma once
#include "Reflection/ReflectionBase.h"
#include "ComponentSystem/ComponentSystem.h"
#include "ComponentSystem/ComponentRegistry.h"
#include "math\Vector4f.h"

//#define AUTO_REGISTER_COMPONENT(className, prettyName, category) \
	

class CEntityInstance : public ComponentSystem::Component
{
public:
	REFL_DECLARE_OBJECT(CEntityInstance, Component);
	DECLARE_COMPONENT();	

private:
	std::string m_entityDefinition = "undfined";
	std::string m_name = "undfined";
	Vector4f m_color = Vector4f(1, 1, 1, 1);
};