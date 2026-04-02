#include "QuickEdit.h"
#include "CommandConsole.h"

#include "EntityInstance.h"
#include "StaticMeshDefinition.h"
#include <iostream>

bool QuickEditApp::Initialize()
{

	CEntityInstance instance;
	instance.Read("./Assets/Entities/Test2.entity.obj.json");

	// Debug: Check if children were loaded
	const auto& children = instance.GetChildren();
	if (children.empty()) {
		std::cout << "ERROR: No children loaded!\n";
	} else {
		std::cout << "SUCCESS: Loaded " << children.size() << " child component(s)\n";
		for (size_t i = 0; i < children.size(); i++) {
			if (children[i]) {
				std::cout << "  Child " << i << ": " << children[i]->GetRflClassName() << "\n";
			}
		}
	}

	// Initialize application-specific resources here
	m_visualizerManager.Initialize();
	m_visualizerManager.Register("Command Console", std::make_unique<CommandConsole>(), true);

	// Create the document manager and register all asset types
	m_documentManager = std::make_unique<DocumentManager>(m_visualizerManager);
	m_documentManager->RegisterAssetTypes();

	return true;
}

void QuickEditApp::Update(double deltaTime)
{
	m_documentManager->ProcessPendingEditors();
	m_visualizerManager.Update(static_cast<float>(deltaTime));
	m_documentManager->CleanupClosedEditors();
}

void QuickEditApp::Render(double deltaTime)
{

}

void QuickEditApp::ImguiUpdate()
{
	m_visualizerManager.RenderAll();
}

void QuickEditApp::ImguiMainMenu()
{
	m_visualizerManager.RenderMenuBar();
}

bool QuickEditApp::Shutdown()
{
	return true;
}
