#pragma once
#include "ComponentSystem/ComponentSystem.h"
#include "ResourceManager/ResourceManager.h"
#include "StaticMeshResource.h"

#include <string>

class CShaderResource;
class CMeshResource;

class CEntityComponent : public ComponentSystem::Component
{
public:
	REFL_DECLARE_OBJECT(CEntityComponent, Component);
	DECLARE_COMPONENT();

	// ── IComponent lifecycle ────────────────────────────────────────────

	bool OnInitialize() override;
	void OnUpdate(double deltaTime) override;
	void OnShutdown() override;
		
private:
	std::string m_name;   
};