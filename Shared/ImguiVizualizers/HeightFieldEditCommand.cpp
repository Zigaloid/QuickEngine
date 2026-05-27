#include "HeightFieldEditCommand.h"
#include "../Components/HeightFieldMeshComponent.h"
#include "../ResourceTypes/MeshResource.h"
#include <bgfx/bgfx.h>
#include <cstring>

namespace ImGuiVisualizers {

	void CHeightFieldEditCommand::ApplyPositions(const std::vector<Entry>& entries, bool useAfter)
	{
		if (!m_component)
			return;

		auto meshRes = m_component->GetMeshResource();
		if (!meshRes || !meshRes->IsLoaded())
			return;

		auto mesh = meshRes->GetMesh();
		if (!mesh || mesh->m_groups.empty())
			return;

		// Work with the first group (assuming single-group mesh)
		auto& group = mesh->m_groups[0];
		if (!group.m_vertices)
			return;

		// Apply position changes to each entry
		for (const auto& entry : entries)
		{
			if (entry.vertexIndex >= group.m_numVertices)
				continue;

			// Choose which position to apply
			const Vector3f& targetPos = useAfter ? entry.after : entry.before;

			// Update the vertex position in the mesh data
			// Assume the vertex layout has Position as the first 3 floats
			uint16_t stride = mesh->m_layout.getStride();
			uint8_t* vertexData = group.m_vertices + (entry.vertexIndex * stride);

			// Cast to float* and write the position directly
			// This assumes Position is the first attribute and is 3 floats
			float* posPtr = reinterpret_cast<float*>(vertexData);
			posPtr[0] = targetPos.x;
			posPtr[1] = targetPos.y;
			posPtr[2] = targetPos.z;
		}

		// Recalculate normals for all groups based on updated vertex positions
		m_component->RecalculateMeshNormals();

		// Rebuild the GPU vertex buffer with the modified data
		// First, destroy the old vertex buffer
		if (bgfx::isValid(group.m_vbh))
		{
			bgfx::destroy(group.m_vbh);
		}

		// Create a new vertex buffer with the updated vertex data
		uint16_t stride = mesh->m_layout.getStride();
		const bgfx::Memory* mem = bgfx::copy(group.m_vertices, group.m_numVertices * stride);
		group.m_vbh = bgfx::createVertexBuffer(mem, mesh->m_layout);

		// Mark mesh state as uninitialized so it will be re-rendered properly
		m_component->SetXSteps(m_component->GetXSteps()); // trigger dirty flag
	}

} // namespace ImGuiVisualizers