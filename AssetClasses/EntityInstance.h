#pragma once

#include <string>
#include <memory>

#include "Reflection/ReflectionBase.h"
#include "ComponentSystem/ComponentSystem.h"
#include "ComponentSystem/ComponentRegistry.h"
#include "ResourceManager/ResourceManager.h"
#include "Reflection/ReflectionBase.h"

#include "math\Vector4f.h"
#include "bgfx\bgfx.h"
#include "bgfx_utils.h"
class CShaderResource;
class CTextureResource;

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

