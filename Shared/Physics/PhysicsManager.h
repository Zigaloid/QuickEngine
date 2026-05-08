#pragma once

#include <Jolt/Jolt.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>

#ifdef JPH_DEBUG_RENDERER
#include <Jolt/Physics/Body/BodyManager.h>
class JoltDebugRenderer;
#endif

JPH_SUPPRESS_WARNINGS

// ---------------------------------------------------------------------------
// Object / broad phase layer definitions
// ---------------------------------------------------------------------------

namespace PhysicsLayers
{
    static constexpr JPH::ObjectLayer NON_MOVING = 0;
    static constexpr JPH::ObjectLayer MOVING     = 1;
    static constexpr JPH::ObjectLayer NUM_LAYERS = 2;
}

namespace BroadPhaseLayers
{
    static constexpr JPH::BroadPhaseLayer NON_MOVING(0);
    static constexpr JPH::BroadPhaseLayer MOVING(1);
    static constexpr unsigned int NUM_LAYERS = 2;
}

// ---------------------------------------------------------------------------

class PhysicsManager
{
public:
    // Configuration applied before Initialize().
    struct Config
    {
        uint32_t    maxBodies            = 1024;
        uint32_t    maxBodyPairs         = 4096;
        uint32_t    maxContactConstraints = 2048;
        uint32_t    numBodyMutexes       = 0;       // 0 = auto-detect
        int         maxConcurrentJobs    = -1;      // -1 = hardware_concurrency - 1
        uint32_t    tempAllocatorSizeMB  = 10;
        float       gravity              = -9.81f;
        int         collisionSteps       = 2;
    };

    PhysicsManager() = default;
    ~PhysicsManager() = default;

    PhysicsManager(const PhysicsManager&)            = delete;
    PhysicsManager& operator=(const PhysicsManager&) = delete;

    bool Initialize(const Config& config = {});
    void Update(float deltaTime);
    void Shutdown();

    // -----------------------------------------------------------------------
    // Body creation helpers – return JPH::BodyID::cInvalidBodyID on failure
    // -----------------------------------------------------------------------

    /// Add a static or dynamic box to the world.
    JPH::BodyID AddBox(
        JPH::Vec3Arg    halfExtents,
        JPH::RVec3Arg   position,
        JPH::QuatArg    rotation    = JPH::Quat::sIdentity(),
        JPH::EMotionType motionType = JPH::EMotionType::Dynamic);

    /// Add a static or dynamic sphere to the world.
    JPH::BodyID AddSphere(
        float           radius,
        JPH::RVec3Arg   position,
        JPH::QuatArg    rotation    = JPH::Quat::sIdentity(),
        JPH::EMotionType motionType = JPH::EMotionType::Dynamic);

    /// Add a static or dynamic capsule (aligned along Y-axis) to the world.
    JPH::BodyID AddCapsule(
        float           halfHeight,
        float           radius,
        JPH::RVec3Arg   position,
        JPH::QuatArg    rotation    = JPH::Quat::sIdentity(),
        JPH::EMotionType motionType = JPH::EMotionType::Dynamic);

    /// Remove and destroy a body.
    void RemoveBody(JPH::BodyID bodyId);

    // -----------------------------------------------------------------------
    // Accessors
    // -----------------------------------------------------------------------

    JPH::PhysicsSystem*  GetPhysicsSystem()  { return m_physicsSystem; }
    JPH::BodyInterface&  GetBodyInterface()  { return m_physicsSystem->GetBodyInterfaceNoLock(); }
    JPH::BodyInterface&  GetBodyInterfaceLocking()  { return m_physicsSystem->GetBodyInterface(); }
    bool                 IsInitialized() const { return m_initialized; }

#ifdef JPH_DEBUG_RENDERER
    /// Draw all physics bodies using Jolt's debug renderer. Call during your Render() pass.
    void DebugDraw(const float* viewMtx);

    /// Access the debug renderer for configuration (enable/disable, view ID, etc.).
    JoltDebugRenderer* GetDebugRenderer() { return m_debugRenderer; }
#endif

    /// Marks the broad phase as needing rebuild (called from any thread).
    void                 SetBroadPhaseDirty() { m_broadPhaseDirty = true; }

    // -----------------------------------------------------------------------
    // Global instance access
    // -----------------------------------------------------------------------

    /// Returns the active PhysicsManager, or nullptr if none has been set.
    static PhysicsManager* Get() { return s_instance; }

    /// Called by the owning application to register / unregister the active instance.
    static void SetInstance(PhysicsManager* instance) { s_instance = instance; }

private:
    static PhysicsManager* s_instance;

    JPH::ObjectLayer ObjectLayerForMotionType(JPH::EMotionType motionType) const;

    Config                                      m_config;
    bool                                        m_initialized = false;
    bool                                        m_broadPhaseDirty = false;

    JPH::TempAllocatorImpl*                     m_tempAllocator  = nullptr;
    JPH::JobSystemThreadPool*                   m_jobSystem      = nullptr;
    JPH::PhysicsSystem*                         m_physicsSystem  = nullptr;

#ifdef JPH_DEBUG_RENDERER
    JoltDebugRenderer*                          m_debugRenderer  = nullptr;
#endif

    // Layer interface objects – must outlive m_physicsSystem.
    struct BPLayerInterfaceImpl;
    struct ObjectVsBPLayerFilterImpl;
    struct ObjectLayerPairFilterImpl;

    BPLayerInterfaceImpl*           m_bpLayerInterface    = nullptr;
    ObjectVsBPLayerFilterImpl*      m_objVsBPFilter       = nullptr;
    ObjectLayerPairFilterImpl*      m_objLayerPairFilter  = nullptr;
};