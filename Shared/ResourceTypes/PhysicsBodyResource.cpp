#include "PhysicsBodyResource.h"
#include "CoreSystem/CoreSystem.h"

// Reflection for the reference wrapper
REFL_DEFINE_OBJECT(CPhysicsBodyResourceReference)
REFL_DEFINE_END

REFL_DEFINE_OBJECT(CPhysicsBodyResource)
    REFL_DEFINE_INT_MEMBER    (CPhysicsBodyResource, m_shapeType),
    REFL_DEFINE_MATRIX4_MEMBER(CPhysicsBodyResource, m_transform),
    REFL_DEFINE_INT_MEMBER    (CPhysicsBodyResource, m_motionType),
    REFL_DEFINE_FLOAT_MEMBER  (CPhysicsBodyResource, m_friction),
    REFL_DEFINE_FLOAT_MEMBER  (CPhysicsBodyResource, m_restitution),
    REFL_DEFINE_FLOAT_MEMBER  (CPhysicsBodyResource, m_linearDamping),
    REFL_DEFINE_FLOAT_MEMBER  (CPhysicsBodyResource, m_angularDamping)
REFL_DEFINE_END

bool CPhysicsBodyResource::Update(FileSystem::FileSystemManager& fileSystem)
{
    m_isLoaded = true;
    SafeRead(this->GetPath());
    return true;
}

// Construct the Jolt shape based on serialised parameters.
void CPhysicsBodyResource::Finalize()
{
    ResourceSystem::Resource::Finalize();
}




