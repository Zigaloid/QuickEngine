#pragma once
#include "ComponentSystem/ComponentSystem.h"
#include "ResourceManager/ResourceManager.h"
#include "MaterialResource.h"
#include "math/vector3f.h"
#include "math/Matrix4f.h"

class CTransformComponent : public ComponentSystem::Component
{
public:
	REFL_DECLARE_OBJECT(CTransformComponent, Component);
	DECLARE_COMPONENT();

	// ── IComponent lifecycle ────────────────────────────────────────────

	bool OnInitialize() override;
	void OnUpdate(double deltaTime) override;
	void OnShutdown() override;

	bool IsLoaded() const;
private:
	Matrix4f m_matrix;
};