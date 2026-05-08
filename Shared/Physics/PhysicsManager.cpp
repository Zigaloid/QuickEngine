#include "PhysicsManager.h"

#include <Jolt/Jolt.h>

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

    // Layer filtering objects – must remain valid for the lifetime of m_physicsSystem.
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

#ifdef JPH_DEBUG_RENDERER
    m_debugRenderer = new JoltDebugRenderer();
    JPH::DebugRenderer::sInstance = m_debugRenderer;
#endif

    m_initialized = true;
    return true;
}

void PhysicsManager::Update(float deltaTime)
{
    if (!m_initialized)
        return;

    if (m_broadPhaseDirty)
    {
        m_physicsSystem->OptimizeBroadPhase();
        m_broadPhaseDirty = false;
    }

    m_physicsSystem->Update(deltaTime, m_config.collisionSteps, m_tempAllocator, m_jobSystem);
}

void PhysicsManager::Shutdown()
{
    if (!m_initialized)
        return;

    #ifdef JPH_DEBUG_RENDERER
    JPH::DebugRenderer::sInstance = nullptr;
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

// ---------------------------------------------------------------------------
// Debug drawing
// ---------------------------------------------------------------------------

#ifdef JPH_DEBUG_RENDERER
void PhysicsManager::DebugDraw(const float* viewMtx)
{
    if (!m_initialized || !m_debugRenderer || !m_debugRenderer->IsEnabled())
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