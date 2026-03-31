#pragma once
#include "app\AppInterface.h"
#include "net\NexusServer.h"
#include "ImGuiVisualizerManager.h"

class NexusServerApp : public IApplication
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
	CNexusServer m_NexusServer;
};