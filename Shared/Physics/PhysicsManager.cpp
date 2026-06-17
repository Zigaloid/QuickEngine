#include "PhysicsManager.h"
#include "Profiler/Profiler.h"
#include <Jolt/Jolt.h>

// Console variables
#include "ConsoleVariable.h"

// Declare console command to toggle physics debug draw
CONSOLE_COMMAND(bool, PhysicsManager_DebugDraw, "Set to 1 to turn on physics debug draw");

PhysicsManager* PhysicsManager::s_instance = nullptr;

#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>

#ifdef JPH_DEBUG_RENDERER
#include "JoltDebugRenderer.h"
#endif

#include <thread>
#include <cassert>

// ---------------------------------------------------------------------------
// Layer interface implementations
// ---------------------------------------------------------------------------

struct PhysicsManager::BPLayerInterfaceImpl final : public JPH::BroadPhaseLayerInterface
{
    BPLayerInterfaceImpl()
    {
        m_objectToBP[PhysicsLayers::NON_MOVING] = BroadPhaseLayers::NON_MOVING;
        m_objectToBP[PhysicsLayers::MOVING]     = BroadPhaseLayers::MOVING;
    }

    unsigned int GetNumBroadPhaseLayers() const override
    {
        return BroadPhaseLayers::NUM_LAYERS;
    }

    JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer inLayer) const override
    {
        assert(inLayer < PhysicsLayers::NUM_LAYERS);
        return m_objectToBP[inLayer];
    }

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
    const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer inLayer) const override
    {
        switch (static_cast<JPH::BroadPhaseLayer::Type>(inLayer))
        {
            case static_cast<JPH::BroadPhaseLayer::Type>(BroadPhaseLayers::NON_MOVING): return "NON_MOVING";
            case static_cast<JPH::BroadPhaseLayer::Type>(BroadPhaseLayers::MOVING):     return "MOVING";
            default: return "UNKNOWN";
        }
    }
#endif

private:
    JPH::BroadPhaseLayer m_objectToBP[PhysicsLayers::NUM_LAYERS] = {};
};

struct PhysicsManager::ObjectVsBPLayerFilterImpl final : public JPH::ObjectVsBroadPhaseLayerFilter
{
    bool ShouldCollide(JPH::ObjectLayer inLayer, JPH::BroadPhaseLayer inBPLayer) const override
    {
        switch (inLayer)
        {
            case PhysicsLayers::NON_MOVING:
                return inBPLayer == BroadPhaseLayers::MOVING;
            case PhysicsLayers::MOVING:
                return true;
            default:
                return false;
        }
    }
};

struct PhysicsManager::ObjectLayerPairFilterImpl final : public JPH::ObjectLayerPairFilter
{
    bool ShouldCollide(JPH::ObjectLayer inLayer1, JPH::ObjectLayer inLayer2) const override
    {
        switch (inLayer1)
        {
            case PhysicsLayers::NON_MOVING:
                return inLayer2 == PhysicsLayers::MOVING;
            case PhysicsLayers::MOVING:
                return true;
            default:
                return false;
        }
    }
};

// ---------------------------------------------------------------------------
// BodyContactListener
// ---------------------------------------------------------------------------

// Helper: record the contact normal for whichever side of the manifold
// corresponds to the MOVING body (i.e. the character).
/*static*/ void PhysicsManager::BodyContactListener::RecordContact(
    NormalMap& map,
    const JPH::Body& inBody1,
    const JPH::Body& inBody2,
    const JPH::ContactManifold& inManifold)
{
    // mWorldSpaceNormal points along the direction to move body2 out of body1,
    // i.e. it points FROM body1 TOWARD body2 (e.g. upward from ground into character).
    // Jolt sorts bodies so body1.ID < body2.ID; the static ground is usually body1.
    if (inBody1.GetMotionType() == JPH::EMotionType::Dynamic)
    {
        // body1 is the dynamic body: the surface pushing it is body2, so the
        // contact normal in body1's frame is the opposite of mWorldSpaceNormal.
        map[inBody1.GetID().GetIndexAndSequenceNumber()].push_back(-inManifold.mWorldSpaceNormal);
    }
    if (inBody2.GetMotionType() == JPH::EMotionType::Dynamic)
    {
        // body2 is the dynamic body: mWorldSpaceNormal already points away from
        // body1 toward body2, so it is the surface normal in body2's frame.
        map[inBody2.GetID().GetIndexAndSequenceNumber()].push_back(inManifold.mWorldSpaceNormal);
    }
}

void PhysicsManager::BodyContactListener::OnContactAdded(
    const JPH::Body& inBody1,
    const JPH::Body& inBody2,
    const JPH::ContactManifold& inManifold,
    JPH::ContactSettings& /*ioSettings*/)
{
    std::lock_guard lock(m_writeMutex);
    RecordContact(m_writing, inBody1, inBody2, inManifold);
}

void PhysicsManager::BodyContactListener::OnContactPersisted(
    const JPH::Body& inBody1,
    const JPH::Body& inBody2,
    const JPH::ContactManifold& inManifold,
    JPH::ContactSettings& /*ioSettings*/)
{
    std::lock_guard lock(m_writeMutex);
    RecordContact(m_writing, inBody1, inBody2, inManifold);
}

void PhysicsManager::BodyContactListener::OnContactRemoved(const JPH::SubShapeIDPair& /*inSubShapePair*/)
{
    // Contacts are fully rebuilt each step via SwapBuffers(); nothing to do here.
}

void PhysicsManager::BodyContactListener::SwapBuffers()
{
    std::lock_guard lock(m_writeMutex);
    // Only replace the read buffer when Jolt actually reported contacts this step.
    // If m_writing is empty (e.g. body briefly inactive), preserve the last known
    // contacts so ground state doesn't flicker for one frame.
    if (!m_writing.empty())
    {
        m_reading = std::move(m_writing);
        m_writing.clear();
    }
    else
    {
        // Still need to clear reading so contacts don't persist indefinitely
        // when the body genuinely has no contacts (e.g. in the air).
        // Use a frame counter to expire stale data after 2 steps.
        ++m_staleFrames;
        if (m_staleFrames >= 2)
        {
            m_reading.clear();
            m_staleFrames = 0;
        }
    }
}

std::vector<JPH::Vec3> PhysicsManager::BodyContactListener::GetNormalsForBody(JPH::BodyID id) const
{
    const auto it = m_reading.find(id.GetIndexAndSequenceNumber());
    if (it != m_reading.end())
        return it->second;
    return {};
}

// ---------------------------------------------------------------------------
// PhysicsManager public API
// ---------------------------------------------------------------------------

std::vector<JPH::Vec3> PhysicsManager::GetContactNormalsForBody(JPH::BodyID bodyId) const
{
    return m_contactListener.GetNormalsForBody(bodyId);
}

// ---------------------------------------------------------------------------
// PhysicsManager
// ---------------------------------------------------------------------------

bool PhysicsManager::Initialize(const Config& config)
{
    assert(!m_initialized);

    m_config = config;

    // Register Jolt allocator and all default types once per process.
    JPH::RegisterDefaultAllocator();
    JPH::Factory::sInstance = new JPH::Factory();
    JPH::RegisterTypes();

    // Temporary allocator used during simulation steps.
    const uint32_t tempSizeBytes = m_config.tempAllocatorSizeMB * 1024u * 1024u;
    m_tempAllocator = new JPH::TempAllocatorImpl(tempSizeBytes);

    // Job system for multi-threaded simulation.
    const int numThreads = (m_config.maxConcurrentJobs < 0)
        ? static_cast<int>(std::thread::hardware_concurrency()) - 1
        : m_config.maxConcurrentJobs - 1;

    m_jobSystem = new JPH::JobSystemThreadPool(
        JPH::cMaxPhysicsJobs,
        JPH::cMaxPhysicsBarriers,
        numThreads);

    // Layer filtering objects — must remain valid for the lifetime of m_physicsSystem.
    m_bpLayerInterface   = new BPLayerInterfaceImpl();
    m_objVsBPFilter      = new ObjectVsBPLayerFilterImpl();
    m_objLayerPairFilter = new ObjectLayerPairFilterImpl();

    // Create and initialise the Jolt physics world.
    m_physicsSystem = new JPH::PhysicsSystem();
    m_physicsSystem->Init(
        m_config.maxBodies,
        m_config.numBodyMutexes,
        m_config.maxBodyPairs,
        m_config.maxContactConstraints,
        *m_bpLayerInterface,
        *m_objVsBPFilter,
        *m_objLayerPairFilter);

    m_physicsSystem->SetGravity(JPH::Vec3(0.0f, m_config.gravity, 0.0f));
    m_physicsSystem->SetContactListener(&m_contactListener);

#ifdef JPH_DEBUG_RENDERER
    m_debugRenderer = new JoltDebugRenderer();
    JPH::DebugRenderer::sInstance = m_debugRenderer;
#endif

    m_initialized = true;
    return true;
}

void PhysicsManager::Update(float deltaTime)
{
    DECLARE_FUNC_VLOW();
    if (!m_initialized)
        return;

    if (m_broadPhaseDirty)
    {
        DECLARE_SCOPE_VLOW();
        m_physicsSystem->OptimizeBroadPhase();
        m_broadPhaseDirty = false;
    }
    {
        DECLARE_SCOPE_VLOW();
        m_physicsSystem->Update(deltaTime, m_config.collisionSteps, m_tempAllocator, m_jobSystem);
        // Publish the contacts recorded during this step to the game-thread read buffer.
        m_contactListener.SwapBuffers();
    }
}

void PhysicsManager::Shutdown()
{
    if (!m_initialized)
        return;

    m_physicsSystem->SetContactListener(nullptr);

#ifdef JPH_DEBUG_RENDERER
    delete m_debugRenderer;     m_debugRenderer  = nullptr;
#endif

    delete m_physicsSystem;     m_physicsSystem  = nullptr;
    delete m_objLayerPairFilter; m_objLayerPairFilter = nullptr;
    delete m_objVsBPFilter;     m_objVsBPFilter  = nullptr;
    delete m_bpLayerInterface;  m_bpLayerInterface = nullptr;
    delete m_jobSystem;         m_jobSystem      = nullptr;
    delete m_tempAllocator;     m_tempAllocator  = nullptr;

    JPH::UnregisterTypes();
    delete JPH::Factory::sInstance;
    JPH::Factory::sInstance = nullptr;

    m_initialized = false;
}

// ---------------------------------------------------------------------------
// Body creation helpers
// ---------------------------------------------------------------------------

JPH::ObjectLayer PhysicsManager::ObjectLayerForMotionType(JPH::EMotionType motionType) const
{
    return (motionType == JPH::EMotionType::Static)
        ? PhysicsLayers::NON_MOVING
        : PhysicsLayers::MOVING;
}

JPH::BodyID PhysicsManager::AddBox(
    JPH::Vec3Arg     halfExtents,
    JPH::RVec3Arg    position,
    JPH::QuatArg     rotation,
    JPH::EMotionType motionType)
{
    JPH::BodyCreationSettings settings(
        new JPH::BoxShape(halfExtents),
        position,
        rotation,
        motionType,
        ObjectLayerForMotionType(motionType));

    return GetBodyInterface().CreateAndAddBody(settings, JPH::EActivation::Activate);
}

JPH::BodyID PhysicsManager::AddSphere(
    float            radius,
    JPH::RVec3Arg    position,
    JPH::QuatArg     rotation,
    JPH::EMotionType motionType)
{
    JPH::BodyCreationSettings settings(
        new JPH::SphereShape(radius),
        position,
        rotation,
        motionType,
        ObjectLayerForMotionType(motionType));

    return GetBodyInterface().CreateAndAddBody(settings, JPH::EActivation::Activate);
}

JPH::BodyID PhysicsManager::AddCapsule(
    float            halfHeight,
    float            radius,
    JPH::RVec3Arg    position,
    JPH::QuatArg     rotation,
    JPH::EMotionType motionType)
{
    JPH::BodyCreationSettings settings(
        new JPH::CapsuleShape(halfHeight, radius),
        position,
        rotation,
        motionType,
        ObjectLayerForMotionType(motionType));

    return GetBodyInterface().CreateAndAddBody(settings, JPH::EActivation::Activate);
}

void PhysicsManager::RemoveBody(JPH::BodyID bodyId)
{
    if (bodyId.IsInvalid())
        return;

    GetBodyInterfaceLocking().RemoveBody(bodyId);
    GetBodyInterfaceLocking().DestroyBody(bodyId);
}

#ifdef JPH_DEBUG_RENDERER
void PhysicsManager::DebugDraw(const float* viewMtx)
{
    if (!m_initialized || !m_debugRenderer)
        return;

    // Sync console variable to renderer enabled state each frame so toggling via console takes effect immediately
    m_debugRenderer->SetEnabled(PhysicsManager_DebugDraw);

    if (!m_debugRenderer->IsEnabled())
        return;

    m_debugRenderer->SetCamera(viewMtx);

    // Configure what to draw
    JPH::BodyManager::DrawSettings settings;
    settings.mDrawShape = true;
    settings.mDrawShapeWireframe = true;
    settings.mDrawBoundingBox = true;
    settings.mDrawCenterOfMassTransform = false;
    settings.mDrawWorldTransform = false;
    settings.mDrawVelocity = false;
    settings.mDrawSleepStats = false;

    m_physicsSystem->DrawBodies(settings, m_debugRenderer);

    // Flush all accumulated lines to bgfx
    m_debugRenderer->Flush();
}
#endif