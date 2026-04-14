#pragma once
#include "ComponentSystem/ComponentSystem.h"
#include "ResourceManager/ResourceManager.h"
#include "MaterialResource.h"
#include <string>

class CShaderResource;
class CMeshResource;

class CEntityComponent : public ComponentSystem::Component
{
public:
	REFL_DECLARE_OBJECT(CEntityComponent, Component);
	DECLARE_COMPONENT();

	// Component lifecycle
	bool OnInitialize() override;
	void OnUpdate(double deltaTime) override;
	void OnShutdown() override;

	/// Returns true when both the mesh and shader resources have been
	/// fully loaded and finalized by the ResourceManager.
	bool IsReady() const;
    
    /// Returns the mesh resource filename referenced by this entity (may be empty).
    std::string GetMeshResourceFileName() const { return m_meshResource.GetResourceFileName(); }

private:
	std::string m_name;    
    CResourceReference m_meshResource;

};