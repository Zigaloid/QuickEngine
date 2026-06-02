#include "CoreSystem/CoreSystem.h"
#include "ComponentSystem/ComponentSystem.h"
#include "AIComponent.h"
#include "ShaderResource.h"
#include "MeshResource.h"
#include "Physics/PhysicsManager.h"

#include <Jolt/Jolt.h>
JPH_SUPPRESS_WARNINGS
#include <Jolt/Physics/Body/BodyInterface.h>

#include <cmath>
#include <cstdlib>
#include <memory>

// ── CAIComponent ────────────────────────────────────────────────────────────
REGISTER_COMPONENT(CAIComponent, "AIComponent", "AI");

REFL_DEFINE_OBJECT(CAIComponent)
REFL_DEFINE_END

bool CAIComponent::OnInitialize()
{
	m_CharacterComponent.Get(this);
	m_NavigationManagerComponent.Get(this);

	// Create and register the nav query so the NavigationManagerComponent
	// services it asynchronously each frame.
	m_navQueryPtr = std::make_shared<CNavQuery>();
	if (auto* nav = m_NavigationManagerComponent.Get())
		nav->RegisterQuery(m_navQueryPtr);

	// Pick an initial destination immediately.
	m_wanderTimer = 0.0;
	return true;
}

void CAIComponent::OnUpdate(double deltaTime)
{
	CCharacterComponent* character = m_CharacterComponent.Get();
	CNavigationManagerComponent* nav = m_NavigationManagerComponent.Get();

	if (!character || !nav)
		return;

	// Tick the wander timer regardless of navmesh state so that as soon as
	// IsReady() becomes true we immediately pick a destination.
	m_wanderTimer -= deltaTime;
	if (m_wanderTimer <= 0.0)
		PickNewWanderTarget();

	if (!nav->IsReady())
		return;

	// Enforce navmesh containment: if the character has drifted off the mesh,
	// snap them back to the nearest valid polygon each frame.
	{
		Vector3f currentPos = character->GetWorldTransform().ExtractTranslation();
		Vector3f snapped;
		if (nav->FindNearestPoint(currentPos, snapped))
		{
			const float dx = snapped.GetX() - currentPos.GetX();
			const float dz = snapped.GetZ() - currentPos.GetZ();
			const float dy = snapped.GetY() - currentPos.GetY();
			const float distSq = dx * dx + dy * dy + dz * dz;
			if (distSq > 0.01f) // ~0.1 m threshold
			{
				Matrix4f corrected = character->GetWorldTransform();
				corrected.SetTranslation(snapped);
				character->SetWorldTransform(corrected, JPH::EActivation::Activate);
			}
		}
	}

	// Consume a freshly computed path once.
	if (!m_pathConsumed && m_navQueryPtr->GetState() == CNavQuery::State::Ready)
	{
		m_path         = m_navQueryPtr->GetPath();
		m_pathIndex    = 0;
		m_pathConsumed = true;
	}

	FollowPath(deltaTime);
}

void CAIComponent::OnShutdown()
{
	if (auto* nav = m_NavigationManagerComponent.Get())
		nav->UnregisterQuery(m_navQueryPtr);

	m_navQueryPtr.reset();
	Component::OnShutdown();
}

bool CAIComponent::IsLoaded() const
{
	return true;
}

// ── Private helpers ──────────────────────────────────────────────────────────

void CAIComponent::PickNewWanderTarget()
{
	CCharacterComponent* character = m_CharacterComponent.Get();
	CNavigationManagerComponent* nav = m_NavigationManagerComponent.Get();
	if (!character || !nav || !nav->IsReady())
		return; // Leave m_wanderTimer <= 0 so we retry next frame.

	// Commit the timer reset only once we know we can actually query.
	m_wanderTimer = m_wanderInterval;

	Vector3f currentPos = character->GetWorldTransform().ExtractTranslation();

	// Random offset within a circle of radius m_wanderRadius.
	const float angle  = (static_cast<float>(std::rand()) / RAND_MAX) * 6.28318f;
	const float dist   = (static_cast<float>(std::rand()) / RAND_MAX) * m_wanderRadius;
	Vector3f candidate(
		currentPos.GetX() + std::cos(angle) * dist,
		currentPos.GetY(),
		currentPos.GetZ() + std::sin(angle) * dist);

	// Snap candidate to the nearest valid navmesh position.
	Vector3f snapped;
	nav->FindNearestPoint(candidate, snapped);
	//return; // No navmesh polygon nearby — try again next interval.

	m_navQueryPtr->SetStart(currentPos);
	m_navQueryPtr->SetDestination(snapped);
	m_pathConsumed = false;
}

void CAIComponent::FollowPath(double /*deltaTime*/)
{
	if (m_path.empty() || m_pathIndex >= static_cast<int>(m_path.size()))
		return;

	CCharacterComponent* character = m_CharacterComponent.Get();
	if (!character)
		return;

	JPH::BodyID bodyId = character->GetBodyID();
	if (bodyId.IsInvalid())
		return;

	PhysicsManager* physics = PhysicsManager::Get();
	if (!physics || !physics->IsInitialized())
		return;

	Vector3f currentPos = character->GetWorldTransform().ExtractTranslation();
	const Vector3f& waypoint = m_path[m_pathIndex];

	// Advance past waypoints that are already close enough.
	const float dx = waypoint.GetX() - currentPos.GetX();
	const float dz = waypoint.GetZ() - currentPos.GetZ();
	const float distSq = dx * dx + dz * dz;
	if (distSq < m_waypointTolerance * m_waypointTolerance)
	{
		++m_pathIndex;
		return;
	}

	// Compute a normalised horizontal direction toward the waypoint.
	const float invDist = 1.0f / std::sqrt(distSq);
	const float dirX = dx * invDist;
	const float dirZ = dz * invDist;

	const float moveSpeed = character->GetMoveSpeed();

	JPH::BodyInterface& bi = physics->GetBodyInterface();
	JPH::Vec3 currentVel = bi.GetLinearVelocity(bodyId);

	// Set horizontal velocity toward the waypoint while preserving vertical.
	bi.SetLinearVelocity(bodyId, JPH::Vec3(dirX * moveSpeed, currentVel.GetY(), dirZ * moveSpeed));
	bi.SetAngularVelocity(bodyId, JPH::Vec3::sZero());
}