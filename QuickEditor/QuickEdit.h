#pragma once
#include "app\AppInterface.h"
#include "ImGuiVisualizerManager.h"
#include "DocumentManager.h"
#include "Physics/PhysicsManager.h"

#include <memory>

class QuickEditApp : public IApplication
{
public:
	bool Initialize() override;
	void Update(double deltaTime) override;
	void Render(double deltaTime) override;
	void ImguiUpdate() override;
	void ImguiMainMenu() override;
	bool Shutdown() override;
	void RegisterComponents();

private:
	ImGuiVisualizers::ImGuiVisualizerManager m_visualizerManager;
	std::unique_ptr<DocumentManager>         m_documentManager;
	PhysicsManager                           m_physicsManager;
};