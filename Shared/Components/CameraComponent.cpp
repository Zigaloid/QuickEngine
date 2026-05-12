#include "CameraComponent.h"
#include "TransformComponent.h"
#include "EntityComponent.h"
#include "Math/Vector3f.h"
#include <bx/math.h>

REFL_DEFINE_OBJECT(CCameraComponent)
REFL_DEFINE_END

REGISTER_COMPONENT(CCameraComponent, "Camera", "Rendering");

bool CCameraComponent::OnInitialize()
{
    return true;
}

void CCameraComponent::OnUpdate(double /*deltaTime*/)
{
    // Get parent component
    ComponentSystem::Component* parent = GetParent();
    if (!parent)
        return;

    // Find the TransformComponent in the parent (same entity level)
    CTransformComponent* transform = parent->FindChild<CTransformComponent>();
    if (transform)
    {
        // Get the parent's world transform
        Matrix4f& localTransform = transform->GetTransform();
        Vector3f characterPos = localTransform.ExtractTranslation();

        // Apply height offset to get camera target
        Vector3f targetPos = characterPos + m_offset;

        // Update camera target to follow character
        m_camera.SetTarget(targetPos.GetX(), targetPos.GetY(), targetPos.GetZ());
    }

    // Update distance in case it changed
    m_camera.SetDistance(m_distance);
}
