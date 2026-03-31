#pragma once

#include <vector>
#include <string>
#include <mutex>

#include "app\AppInterface.h"

#include "ImGuiVisualizerManager.h"

class QuickScopeApp : public IApplication
{
public:
	bool Initialize() override;
	void Update(double deltaTime) override;
	void Render(double deltaTime) override;
	void ImguiUpdate() override;
	void ImguiMainMenu() override;
	bool Shutdown() override;

private:
	void HandleFPSMessage(const std::string& messageBody);
	void FlushFpsTracking();
	ImGuiVisualizers::ImGuiVisualizerManager m_visualizerManager;
	std::mutex m_fpsMutex;
	std::vector<float> m_pendingFPSUpdates;
};


