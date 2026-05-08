#include "QuickEdit.h"
#include "CommandConsole.h"
#include "ImGui3DViewVisualizer.h"
#include "MessageSystem/MessageBus.h"
#include <iostream>
#include "PropertyWidgetMapRegistry.h"
#include "ShaderResource.h"
#include "CoreSystem\CoreDebugChannels.h"
#include "CoreSystem\CoreSystem.h"
#include "MeshComponent.h"
#include "imgui.h"

std::shared_ptr<CShaderResource> testShader;
bool QuickEditApp::Initialize()
{
	CTextureResourceReference testTextureRef;
	testTextureRef.OnLoaded();


	// Initialize application-specific resources here
	m_visualizerManager.Initialize();
	m_visualizerManager.Register("Command Console", std::make_unique<CommandConsole>(), false);

	// Create the document manager and register all asset types
	m_documentManager = std::make_unique<DocumentManager>(m_visualizerManager);
	m_documentManager->RegisterAssetTypes();

	// App setup component types.
	RegisterComponents();

	PhysicsManager::Config physicsConfig;
	physicsConfig.maxBodies = 1024;
	physicsConfig.gravity   = -9.81f;
	m_physicsManager.Initialize(physicsConfig);
    Input::KeyboardShortcutManager::Instance().Initialize();
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
	m_physicsManager.Update(static_cast<float>(deltaTime));

	m_documentManager->ProcessPendingEditors();
	m_visualizerManager.Update(static_cast<float>(deltaTime));
	m_documentManager->CleanupClosedEditors();
	Core::MessageSystem::MessageBus::Get().ProcessAll();
    Input::KeyboardShortcutManager::Instance().Update(deltaTime);    
}

void QuickEditApp::Render(double deltaTime)
{

}

void QuickEditApp::ImguiUpdate()
{
    // Create a full-viewport host window that contains the main dockspace.
    // This ensures newly opened internal editor windows can dock into it by default.
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);

    ImGuiWindowFlags hostWindowFlags = ImGuiWindowFlags_MenuBar
                                     | ImGuiWindowFlags_NoTitleBar
                                     | ImGuiWindowFlags_NoCollapse
                                     | ImGuiWindowFlags_NoResize
                                     | ImGuiWindowFlags_NoMove
                                     | ImGuiWindowFlags_NoBringToFrontOnFocus
                                     | ImGuiWindowFlags_NoNavFocus;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::Begin("MainDockSpace", nullptr, hostWindowFlags);
    ImGui::PopStyleVar(2);

    // Create the dockspace
    ImGuiID dockspaceId = ImGui::GetID("MainDockSpace");
    ImGui::DockSpace(dockspaceId, ImVec2(0, 0), ImGuiDockNodeFlags_PassthruCentralNode);

    // Render the menu bar into the host window (RenderMenuBar expects to be called inside a window with MenuBar)
    if (ImGui::BeginMenuBar()) {
        m_visualizerManager.RenderMenuBar();
        ImGui::EndMenuBar();
    }

    // Render all registered visualizers (they will get a SetNextWindowDockID in the manager)
    m_visualizerManager.RenderAll();

    ImGui::End();
}

void QuickEditApp::ImguiMainMenu()
{
	m_visualizerManager.RenderMenuBar();
}

bool QuickEditApp::Shutdown()
{
    m_physicsManager.Shutdown();

	m_documentManager.reset();
	m_visualizerManager.Shutdown();
	return true;
}
