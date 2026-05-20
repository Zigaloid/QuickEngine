#pragma once

#include "Selectable.h"
#include "../Components/HeightFieldMeshComponent.h"
#include "../Components/EntityComponent.h"
#include "../Components/TransformComponent.h"
#include "../ResourceTypes/MeshResource.h"
#include <memory>
#include <bgfx/bgfx.h>

namespace ImGuiVisualizers {

/**
 * @brief Represents a selectable point for a vertex in a mesh.
 *
 * Each mesh vertex is identified by the group it belongs to and its index within that group.
 * The selectable provides a small bounding sphere centered at the vertex's world position.
 *
 * The transform is updated dynamically to reflect the current world position of the vertex,
 * which includes the mesh resource's world transform.
 */
class CMeshVertexSelectable final : public CSelectable
{
public:
    explicit CMeshVertexSelectable(
        CHeightFieldMeshComponent* component,
        CMeshResource* meshResource,
        const Group* group,
        uint16_t vertexIndex,
        const bgfx::VertexLayout* layout)
        : m_component(component)
        , m_meshResource(meshResource)
        , m_group(group)
        , m_vertexIndex(vertexIndex)
        , m_layout(layout)
    {
        // Initialize with a default picking radius
        m_pickingRadius = std::make_shared<Vector4f>(0.0f, 0.0f, 0.0f, 0.3f);
        UpdateTransform();
        SetBoundingSphere(m_pickingRadius);
    }

    CHeightFieldMeshComponent* GetComponent() const { return m_component; }
    CMeshResource* GetMeshResource() const { return m_meshResource; }
    const Group* GetGroup() const { return m_group; }
    uint16_t GetVertexIndex() const { return m_vertexIndex; }
    const bgfx::VertexLayout* GetLayout() const { return m_layout; }

    /// Extracts and returns the current world-space position of this vertex
    Vector3f GetWorldPosition() const
    {
        if (!m_component || !m_group || !m_layout)
            return Vector3f(0.0f, 0.0f, 0.0f);

        // Extract vertex position from mesh data if available
        if (!m_group->m_vertices)
            return Vector3f(0.0f, 0.0f, 0.0f);

        uint16_t stride = m_layout->getStride();
        if (stride == 0)
            return Vector3f(0.0f, 0.0f, 0.0f);

        // Validate vertex index
        if (m_vertexIndex >= m_group->m_numVertices)
            return Vector3f(0.0f, 0.0f, 0.0f);

        uint8_t* vertexData = m_group->m_vertices + (m_vertexIndex * stride);

        // Cast to float* and read the position directly
        // This assumes Position is the first attribute and is 3 floats
        const float* posPtr = reinterpret_cast<const float*>(vertexData);
        Vector3f localPos(posPtr[0], posPtr[1], posPtr[2]);

        // Get component's world transform via entity or transform sibling
        Matrix4f worldTransform = Matrix4f::GetIdentity();

        if (auto* transformComponent = m_component->FindSibling<CTransformComponent>())
        {
            worldTransform = transformComponent->GetTransform();
        }
        else if (auto* entity = dynamic_cast<CEntityComponent*>(m_component->GetParent()))
        {
            if (auto* transformComponent = entity->FindChild<CTransformComponent>())
            {
                worldTransform = transformComponent->GetTransform();
            }
        }

        return worldTransform.TransformPoint(localPos);
    }

    /// Sets the vertex position in the mesh (in local space of the component)
    void SetLocalPosition(const Vector3f& localPos)
    {
        if (!m_group || !m_layout || !m_group->m_vertices)
            return;

        uint16_t stride = m_layout->getStride();
        uint8_t* vertexData = m_group->m_vertices + (m_vertexIndex * stride);

        // Cast to float* and write the position directly
        float* posPtr = reinterpret_cast<float*>(vertexData);
        posPtr[0] = localPos.x;
        posPtr[1] = localPos.y;
        posPtr[2] = localPos.z;
    }

    /// Updates the transform and bounding sphere to reflect the current vertex position
    void UpdateTransform()
    {
        if (!m_component)
            return;

        Vector3f worldPos = GetWorldPosition();

        // Create a transform matrix for this vertex
        Matrix4f pointTransform = Matrix4f::GetIdentity();
        pointTransform.SetTranslation(worldPos);

        // Store the transform
        if (!m_transform)
            m_transform = std::make_shared<Matrix4f>();
        *m_transform = pointTransform;

        SetTransform(m_transform);
        SetOwner(m_component);
    }

private:
    CHeightFieldMeshComponent* m_component = nullptr;
    CMeshResource* m_meshResource = nullptr;
    const Group* m_group = nullptr;
    uint16_t m_vertexIndex = 0;
    const bgfx::VertexLayout* m_layout = nullptr;
    std::shared_ptr<Vector4f> m_pickingRadius;
    std::shared_ptr<Matrix4f> m_transform;
};

} // namespace ImGuiVisualizers
