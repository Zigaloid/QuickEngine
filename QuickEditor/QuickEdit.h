#pragma once
#include "app\AppInterface.h"
#include "ImGuiVisualizerManager.h"

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
	ImGuiVisualizers::ImGuiVisualizerManager m_visualizerManager;
};