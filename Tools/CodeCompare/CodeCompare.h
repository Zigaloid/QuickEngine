#pragma once

#include <vector>
#include <string>

#include "app\AppInterface.h"
#include "ImGuiVisualizerManager.h"
#include "CodeCompareVisualizer.h"
#include "AIAssistantService.h"

class CToolApp : public IApplication
{
public:
	bool Initialize() override;
	void Update(double deltaTime) override;
	void Render(double deltaTime) override;
	void ImguiUpdate() override;
	void ImguiMainMenu() override;
	bool Shutdown() override;

private:
	ImGuiVisualizers::ImGuiVisualizerManager m_visualizerManager;
	AIAssistantService m_aiService;
};