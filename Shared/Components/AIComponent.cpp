#include "CoreSystem/CoreSystem.h"
#include "ComponentSystem/ComponentSystem.h"
#include "AIComponent.h"
#include "ShaderResource.h"
#include "MeshResource.h"
#include "Physics/PhysicsManager.h"
#include "Rendering/BgfxRenderPrimitives.h"

#include <Jolt/Jolt.h>
JPH_SUPPRESS_WARNINGS
#include <Jolt/Physics/Body/BodyInterface.h>

#include <bx/math.h>

#include <cmath>
#include <cstdlib>
#include <memory>

// ── CAIComponent ────────────────────────────────────────────────────────────
REGISTER_COMPONENT(CAIComponent, "AIComponent", "AI");

REFL_DEFINE_OBJECT(CAIComponent)
REFL_DEFINE_END

bool CAIComponent::OnInitialize()
{
	DECLARE_FUNC_VLOW();
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
	DECLARE_FUNC_MEDIUM();
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
	// apply a corrective velocity impulse toward the nearest valid polygon.
	// We never teleport as that breaks the physics simulation.
	{
		Vector3f currentPos = character->GetWorldTransform().ExtractTranslation();
		Vector3f snapped;
		if (nav->FindNearestPoint(currentPos, snapped))
		{
			const float dx = snapped.GetX() - currentPos.GetX();
			const float dz = snapped.GetZ() - currentPos.GetZ();
			const float distSq = dx * dx + dz * dz; // XZ only — ignore vertical drift

			// Only correct when the character is meaningfully outside the nav mesh.
			if (distSq > 0.01f) // ~0.1 m threshold
			{
				JPH::BodyID bodyId = character->GetBodyID();
				PhysicsManager* physics = PhysicsManager::Get();
				if (!bodyId.IsInvalid() && physics && physics->IsInitialized())
				{
					const float invDist   = 1.0f / std::sqrt(distSq);
					const float dirX      = dx * invDist;
					const float dirZ      = dz * invDist;
					const float moveSpeed = character->GetMoveSpeed();

					JPH::BodyInterface& bi = physics->GetBodyInterface();
					JPH::Vec3 currentVel   = bi.GetLinearVelocity(bodyId);

					// Redirect horizontal velocity toward the mesh while preserving
					// any vertical velocity (e.g. gravity / jump).
					bi.SetLinearVelocity(bodyId,
						JPH::Vec3(dirX * moveSpeed, currentVel.GetY(), dirZ * moveSpeed));
				}
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
	DebugRenderTarget();
}

void CAIComponent::OnShutdown()
{
	DECLARE_FUNC_VLOW();
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
	if (nav->FindNearestPoint(candidate, snapped) == false)
	{
		m_hasDebugTarget = false;
		return; // No valid position nearby — try again next frame.
	}
    // Commit the timer reset only once we know we can actually query.
    m_wanderTimer = m_wanderInterval;

	m_debugTarget    = snapped;
	m_hasDebugTarget = true;

	m_navQueryPtr->SetStart(currentPos);
	m_navQueryPtr->SetDestination(snapped);
	m_pathConsumed = false;
}

void CAIComponent::DebugRenderTarget() const
{
	if (!m_hasDebugTarget)
		return;

	// Capture the target by value so the lambda is safe to execute on the
	// render thread after this frame's update has completed.
	const Vector3f target = m_debugTarget;

	auto* renderFunctionQueue = Core::CoreSystem::GetRenderFunctionQueue();
	if (!renderFunctionQueue)
		return;

	renderFunctionQueue->AddFunction([target]()
		{
			// Build a scale-translate matrix: 0.4 m radius sphere at the target.
			float mtx[16];
			bx::mtxSRT(mtx,
				0.4f, 0.4f, 0.4f,           // scale
				0.0f, 0.0f, 0.0f,           // rotation
				target.GetX(), target.GetY(), target.GetZ()); // translation

			// Solid red — ABGR format: 0xff0000ff
			Rendering::BgfxRenderPrimitives& prims = Rendering::BgfxRenderPrimitives::Instance();
			prims.RenderSphere(0, mtx, 0xff0000ff);
		}, "CAIComponent::DebugRenderTarget");
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