#pragma once

#include "Selectable.h"
#include "../Components/PhysicsBodyComponent.h"

namespace ImGuiVisualizers {

// Adapts a CPhysicsBodyComponent into the CSelectable interface.
// The selectable owns simple shared_ptrs for a transform and bounding sphere
// which are updated by calling UpdateFromComponent().
class CPhysicsBodySelectable final : public CSelectable
{
public:
    explicit CPhysicsBodySelectable(CPhysicsBodyComponent* component)
        // Use the component's shared pointers so the selectable references the
        // live component data (same pattern as RenderComponentSelectable).
        : CSelectable(component ? component->GetModelMatrix() : std::shared_ptr<Matrix4f>(),
                      component ? component->GetBoundingSphere() : std::shared_ptr<Vector4f>(),
                      component)
        , m_component(component)
    {}

    void UpdateFromComponent()
    {
        if (!m_component) return;

        auto transformPtr = GetTransformPtr();
        if (transformPtr)
        {
            // Read the current physics simulation transform.
            Matrix4f world = m_component->GetWorldTransform();

            if (*transformPtr != world)
            {
                // The shared matrix was modified externally (e.g. by a gizmo drag) —
                // push the change back into the physics simulation.
                m_component->SetWorldTransform(*transformPtr);
            }
            else
            {
                // No external change — sync from simulation (e.g. dynamic body moved).
                *transformPtr = world;
            }
        }

        // Bounding sphere: use local centre at origin and radius from component scale
        auto bsPtr = GetBoundingSpherePtr();
        if (bsPtr)
        {
            Vector3f scale = m_component->GetScale();
            float maxScale = scale.GetX();
            if (scale.GetY() > maxScale) maxScale = scale.GetY();
            if (scale.GetZ() > maxScale) maxScale = scale.GetZ();

            // local centre at origin; radius = half of max extent
            *bsPtr = Vector4f(0.0f, 0.0f, 0.0f, maxScale * 0.5f);
        }
    }

    CPhysicsBodyComponent* GetComponent() const { return m_component; }

private:
    CPhysicsBodyComponent* m_component = nullptr;
};

} // namespace ImGuiVisualizers
