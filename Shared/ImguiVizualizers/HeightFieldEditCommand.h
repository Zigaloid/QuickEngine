#pragma once

#include "ICommand.h"
#include "HeightFieldMeshComponent.h"
#include "Math/Vector3f.h"
#include <vector>

namespace ImGuiVisualizers {

/**
 * @brief Undoable edit that records before/after positions for one or more vertices.
 *
 * Execute() writes the 'after' positions, Undo() writes the 'before' positions.
 * Both operations update the mesh vertex data and reset render state so the visual
 * update is immediate.
 */
class CHeightFieldEditCommand final : public ICommand
{
public:
	struct Entry
	{
		uint16_t vertexIndex = 0;  // Index within the mesh group
		Vector3f before;            // Position before edit
		Vector3f after;             // Position after edit
	};

	explicit CHeightFieldEditCommand(CHeightFieldMeshComponent* comp, std::vector<Entry> entries, const char* label = "Edit Height Field")
		: m_component(comp)
		, m_entries(std::move(entries))
		, m_label(label)
	{}

	void Execute() override
	{
		ApplyPositions(m_entries, true);
	}

	void Undo() override
	{
		ApplyPositions(m_entries, false);
	}

	const char* GetLabel() const override { return m_label.c_str(); }

private:
	void ApplyPositions(const std::vector<Entry>& entries, bool useAfter);

	CHeightFieldMeshComponent* m_component = nullptr;
	std::vector<Entry> m_entries;
	std::string m_label;
};

} // namespace ImGuiVisualizers
