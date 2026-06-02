#pragma once
#pragma once
#include "ComponentSystem/ComponentSystem.h"
#include "ResourceManager/ResourceManager.h"
#include "MaterialResource.h"
#include "math/vector3f.h"
#include "math/Matrix4f.h"
#include "NavQuery.h"
#include "CharacterComponent.h"
#include "NavigationManagerComponent.h"

#include <memory>
#include <vector>

class CAIComponent : public ComponentSystem::Component
{
public:
	REFL_DECLARE_OBJECT(CAIComponent, Component);
	DECLARE_COMPONENT();

	// ── IComponent lifecycle ────────────────────────────────────────────

	bool OnInitialize() override;
	void OnUpdate(double deltaTime) override;
	void OnShutdown() override;

	bool IsLoaded() const;

private:
	// Picks a random point within m_wanderRadius meters and requests a new path.
	void PickNewWanderTarget();

	// Steers the character toward the next waypoint on the current path.
	void FollowPath(double deltaTime);

	// Submits a debug sphere at the current wander target into the render queue.
	void DebugRenderTarget() const;

	std::shared_ptr<CNavQuery> m_navQueryPtr;
	ComponentSystem::CComponentReference<CCharacterComponent> m_CharacterComponent{ ComponentSystem::FIRST_SIBLING };
	ComponentSystem::CComponentReference<CNavigationManagerComponent> m_NavigationManagerComponent{ ComponentSystem::FIRST_IN_HIERARCHY };

	// Active path and current waypoint index.
	std::vector<Vector3f> m_path;
	int  m_pathIndex     = 0;
	bool m_pathConsumed  = false;

	// Wander timer: when it reaches zero a new destination is chosen.
	double m_wanderTimer    = 0.0;
	double m_wanderInterval = 5.0;   // seconds between new random targets
	float  m_wanderRadius   = 10.0f; // metres

	// Distance at which the agent considers a waypoint reached.
	float m_waypointTolerance = 0.5f;

	// Debug visualisation: the last wander destination that was submitted.
	Vector3f m_debugTarget;
	bool     m_hasDebugTarget = false;
};
