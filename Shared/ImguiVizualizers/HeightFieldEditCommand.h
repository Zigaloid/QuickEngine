#pragma once

#include "ICommand.h"
#include "HeightFieldMeshComponent.h"
#include <vector>

namespace ImGuiVisualizers {

/**
 * @brief Undoable edit that records before/after heights for one or more points.
 *
 * Execute() writes the 'after' values, Undo() writes the 'before' values.
 * Both operations rebuild the mesh and reset render state so the visual update
 * is immediate.
 */
class CHeightFieldEditCommand final : public ICommand
{
public:
	struct Entry
	{
		uint32_t xIndex = 0;
		uint32_t zIndex = 0;
		float    before = 0.0f;
		float    after  = 0.0f;
	};

	explicit CHeightFieldEditCommand(CHeightFieldMeshComponent* comp, std::vector<Entry> entries, const char* label = "Edit Height Field")
		: m_component(comp)
		, m_entries(std::move(entries))
		, m_label(label)
	{}

	void Execute() override
	{
		if (!m_component) return;
		for (const auto& e : m_entries)
			m_component->SetHeightAt(e.xIndex, e.zIndex, e.after);

		m_component->RebuildMesh();
		m_component->ForceRenderStateReset();
	}

	void Undo() override
	{
		if (!m_component) return;
		for (const auto& e : m_entries)
			m_component->SetHeightAt(e.xIndex, e.zIndex, e.before);

		m_component->RebuildMesh();
		m_component->ForceRenderStateReset();
	}

	const char* GetLabel() const override { return m_label; }

private:
	CHeightFieldMeshComponent* m_component = nullptr;
	std::vector<Entry>         m_entries;
	const char*                m_label = "Edit Height Field";
};

} // namespace ImGuiVisualizers
