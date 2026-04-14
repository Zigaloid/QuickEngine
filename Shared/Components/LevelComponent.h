#pragma once
#include "ComponentSystem/ComponentSystem.h"
#include "ResourceManager/ResourceManager.h"
#include "MaterialResource.h"

class CShaderResource;
class CMeshResource;

class CLevelComponent : public ComponentSystem::Component
{
public:
	REFL_DECLARE_OBJECT(CLevelComponent, Component);
	DECLARE_COMPONENT();

	// Component lifecycle
	bool OnInitialize() override;
	void OnUpdate(double deltaTime) override;
	void OnShutdown() override;

	/// Returns true when both the mesh and shader resources have been
	/// fully loaded and finalized by the ResourceManager.
	bool IsReady() const;
private:
	std::string m_name;

};