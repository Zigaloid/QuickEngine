#pragma once
#include "app\AppInterface.h"
#include "ImGuiVisualizerManager.h"

#include <unordered_set>
#include <string>
#include <vector>

class QuickEditApp : public IApplication
{
public:
	bool Initialize() override;
	void Update(double deltaTime) override;
	void Render(double deltaTime) override;
	void ImguiUpdate() override;
	void ImguiMainMenu() override;
	bool Shutdown() override;

private:
	/// Remove editor instances whose windows have been closed.
	void CleanupClosedEditors();

	/// Process deferred editor registrations (safe outside RenderAll).
	void ProcessPendingEditors();

	ImGuiVisualizers::ImGuiVisualizerManager m_visualizerManager;

	/// Manager keys of dynamically-created ObjJsonEditor instances.
	std::unordered_set<std::string> m_openEditorKeys;

	/// Deferred registrations to avoid mutating m_entries during RenderAll.
	struct PendingEditor {
		std::string key;
		std::string filePath;
		std::string className;
	};
	std::vector<PendingEditor> m_pendingEditors;
};