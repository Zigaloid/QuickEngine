#pragma once
#include "ComponentSystem/ComponentSystem.h"
#include "ResourceManager/ResourceManager.h"
#include "StaticMeshResource.h"
#include "MeshComponent.h"

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

	const std::string& GetName() const { return m_name; }
	void SetName(const std::string& name) { m_name = name; }

	/// Re-binds all CRenderComponent children to their sibling CTransformComponent
	/// after deserialisation, when the full sibling set is guaranteed to be present.
	void OnLoaded() override
	{
		for (auto& child : GetChildren())
		{
			if (auto* rc = dynamic_cast<CRenderComponent*>(child.get()))
				rc->RebindTransform();
		}
	}

private:
	std::string m_name;   
};