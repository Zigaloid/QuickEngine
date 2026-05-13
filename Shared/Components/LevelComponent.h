#pragma once
#include "ComponentSystem/ComponentSystem.h"
#include "ResourceManager/ResourceManager.h"
#include "MaterialResource.h"
#include "ComponentResource.h"

class CShaderResource;
class CMeshResource;

class CLevelComponent : public ComponentSystem::Component
{
public:
	REFL_DECLARE_OBJECT(CLevelComponent, Component);
	DECLARE_COMPONENT();

	// ── IComponent lifecycle ────────────────────────────────────────────

	bool OnInitialize() override;
	void OnUpdate(double deltaTime) override;
	void OnShutdown() override;

	bool IsReady() const;

	// ── Name accessors ──────────────────────────────────────────────────

	const std::string& GetName() const { return m_name; }
	void SetName(const std::string& name) { m_name = name; }

	// Visibility in editor UI (separate from component active state)
	bool IsVisibleInEditor() const { return m_visibleInEditor; }
	void SetVisibleInEditor(bool v) { m_visibleInEditor = v; }

private:
	std::string m_name;
	bool m_visibleInEditor = true;

};