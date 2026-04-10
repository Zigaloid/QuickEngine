#include "QuickEdit.h"
#include "CommandConsole.h"
#include "ImGui3DViewVisualizer.h"
#include "MessageSystem/MessageBus.h"
#include "EntityInstance.h"
#include <iostream>
#include "PropertyWidgetMapRegistry.h"
#include "ShaderResource.h"
#include "CoreSystem\CoreDebugChannels.h"
#include "CoreSystem\CoreSystem.h"
#include "MeshComponent.h"


std::shared_ptr<CShaderResource> testShader;
bool QuickEditApp::Initialize()
{

	// Initialize application-specific resources here
	m_visualizerManager.Initialize();
	m_visualizerManager.Register("Command Console", std::make_unique<CommandConsole>(), false);
	
	// Create the document manager and register all asset types
	m_documentManager = std::make_unique<DocumentManager>(m_visualizerManager);
	m_documentManager->RegisterAssetTypes();

	// App setup component types.
	RegisterComponents();

	return true;
}

void QuickEditApp::RegisterComponents()
{
	auto* componentManager = Core::CoreSystem::GetComponentManager();
	auto* scheduler = Core::CoreSystem::GetJobSystemScheduler();

	componentManager->RegisterComponentType<CMeshComponent>();
	scheduler->RegisterComponentType<CMeshComponent>(0, "Mesh");
}

void QuickEditApp::Update(double deltaTime)
{
	m_documentManager->ProcessPendingEditors();
	m_visualizerManager.Update(static_cast<float>(deltaTime));
	m_documentManager->CleanupClosedEditors();
	Core::MessageSystem::MessageBus::Get().ProcessAll();
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
	m_documentManager.reset();
	m_visualizerManager.Shutdown();
	return true;
}
