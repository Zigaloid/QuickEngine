#pragma once

#include "Selectable.h"
#include "../Components/PhysicsComponent.h"

namespace ImGuiVisualizers {

/**
 * @brief Adapts any CPhysicsComponent into the CSelectable interface.
 *
 * Uses the virtual GetEditableTransform() / GetEditableBoundingSphere()
 * methods on the base class so the gizmo system can edit any physics
 * component's shape geometry in-place without knowing the concrete type.
 *
 * For CPhysicsBodyComponent the shared ptrs come from the body resource.
 * For CCharacterComponent they point to the component's own m_localTransform.
 */
class CPhysicsComponentSelectable final : public CSelectable
{
public:
    explicit CPhysicsComponentSelectable(CPhysicsComponent* component)
        : CSelectable(component ? component->GetEditableTransform()      : std::shared_ptr<Matrix4f>(),
                      component ? component->GetEditableBoundingSphere() : std::shared_ptr<Vector4f>(),
                      component)
        , m_component(component)
    {}

    /// Refresh the bounding sphere from the current editable transform scale.
    void UpdateFromComponent()
    {
        if (!m_component) return;

        auto bsPtr        = GetBoundingSpherePtr();
        auto transformPtr = GetTransformPtr();
        if (bsPtr && transformPtr)
        {
            const Vector3f scale = transformPtr->ExtractScale();
            float maxScale = scale.GetX();
            if (scale.GetY() > maxScale) maxScale = scale.GetY();
            if (scale.GetZ() > maxScale) maxScale = scale.GetZ();
            *bsPtr = Vector4f(0.0f, 0.0f, 0.0f, maxScale * 0.5f);
        }
    }

    CPhysicsComponent* GetComponent() const { return m_component; }

private:
    CPhysicsComponent* m_component = nullptr;
};

} // namespace ImGuiVisualizers
