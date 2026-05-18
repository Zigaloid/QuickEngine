#pragma once

#include "Selectable.h"
#include "../Components/HeightFieldMeshComponent.h"
#include "../Components/EntityComponent.h"
#include "../Components/TransformComponent.h"
#include <memory>

namespace ImGuiVisualizers {

/**
 * @brief Represents a selectable point in a height field mesh.
 *
 * Each height field point is defined by its (x, z) indices in the height data grid.
 * The selectable provides a small bounding sphere centered at the point's world position,
 * with a fixed radius to make it easier to pick in the viewport.
 *
 * The transform is updated dynamically to reflect the current world position of the point,
 * which includes both its height value and the component's world transform.
 */
class CHeightFieldPointSelectable final : public CSelectable
{
public:
    explicit CHeightFieldPointSelectable(CHeightFieldMeshComponent* component, uint32_t xIndex, uint32_t zIndex)
        : m_component(component)
        , m_xIndex(xIndex)
        , m_zIndex(zIndex)
    {
        // Initialize with a default picking radius
        m_pickingRadius = std::make_shared<Vector4f>(0.0f, 0.0f, 0.0f, 0.3f);
        UpdateTransform();
        SetBoundingSphere(m_pickingRadius);
    }

    CHeightFieldMeshComponent* GetComponent() const { return m_component; }
    uint32_t GetXIndex() const { return m_xIndex; }
    uint32_t GetZIndex() const { return m_zIndex; }

    /// Updates the transform and bounding sphere to reflect the current height value.
    /// Call this whenever the height field data is modified.
    void UpdateTransform()
    {
        if (!m_component)
            return;

        // Calculate local position in height field space
        float localX = static_cast<float>(m_xIndex) * m_component->GetStepSize();
        float localZ = static_cast<float>(m_zIndex) * m_component->GetStepSize();
        float localY = m_component->GetHeightAt(m_xIndex, m_zIndex);

        // Get component's world transform via entity or transform sibling
        Matrix4f worldTransform = Matrix4f::GetIdentity();

        // Try to find a sibling TransformComponent
        if (auto* transformComponent = m_component->FindSibling<CTransformComponent>())
        {
            worldTransform = transformComponent->GetTransform();
        }
        else if (auto* entity = dynamic_cast<CEntityComponent*>(m_component->GetParent()))
        {
            // If the component is a child of an entity, try to find transform there
            if (auto* transformComponent = entity->FindChild<CTransformComponent>())
            {
                worldTransform = transformComponent->GetTransform();
            }
        }

        // Create a transform matrix for this point
        Matrix4f pointTransform = Matrix4f::GetIdentity();
        pointTransform.SetTranslation(Vector3f(localX, localY, localZ));

        // Combine with component's world transform
        Matrix4f worldPointTransform = worldTransform * pointTransform;

        // Store the transform
        if (!m_transform)
            m_transform = std::make_shared<Matrix4f>();
        *m_transform = worldPointTransform;

        SetTransform(m_transform);
        SetOwner(m_component);
    }

private:
    CHeightFieldMeshComponent* m_component = nullptr;
    uint32_t m_xIndex = 0;
    uint32_t m_zIndex = 0;
    std::shared_ptr<Vector4f> m_pickingRadius;
    std::shared_ptr<Matrix4f> m_transform;
};

} // namespace ImGuiVisualizers
