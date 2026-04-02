#include "QuickEdit.h"
#include "CommandConsole.h"

#include "EntityDefinition.h"
#include "StaticMeshDefinition.h"

bool QuickEditApp::Initialize()
{
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
