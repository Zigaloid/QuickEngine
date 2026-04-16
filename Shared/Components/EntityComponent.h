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

	// ── IComponent lifecycle ────────────────────────────────────────────

	bool OnInitialize() override;
	void OnUpdate(double deltaTime) override;
	void OnShutdown() override;

	// ── Public API ──────────────────────────────────────────────────────

	bool IsReady() const;
    std::string GetMeshResourceFileName() const { return m_meshResource.GetResourceFileName(); }

private:
	std::string m_name;    
    CResourceReference m_meshResource;

};