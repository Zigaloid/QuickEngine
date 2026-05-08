#include "CoreSystem/CoreSystem.h"
#include "Net/NexusClient.h"
#include "..\SharedNexusDefines.h"

#include "CodeCompare.h"
#include "CommandConsole.h"
#include "AIAssistantVisualizer.h"
#include "CodeCompareVisualizer.h"

#include "imgui.h"

bool CToolApp::Initialize()
{
	NEXUS_CONNECT_AND_REGISTER("127.0.0.1", 9500, "CodeCompare", "nhill");
	Core::CoreSystem::GetNexusClient()->EnableAutoReconnect();

	m_visualizerManager.Register("Command Console",
		std::make_unique<CommandConsole>(), true);

	auto codeCompare = std::make_unique<CodeCompareVisualizer>();
	codeCompare->SetAIService(&m_aiService);
	m_visualizerManager.Register("Code Comparison", std::move(codeCompare), true);

	m_visualizerManager.Register("AI Assistant",
		std::make_unique<AIAssistantVisualizer>(m_aiService), true);

	m_visualizerManager.Initialize();
	Input::KeyboardShortcutManager::Instance().Initialize();

	return true;
}

void CToolApp::Update(double deltaTime)
{
	m_aiService.Update();
    Input::KeyboardShortcutManager::Instance().Update(deltaTime);
}

void CToolApp::Render(double /*deltaTime*/)
{
}

void CToolApp::ImguiUpdate()
{
	m_visualizerManager.RenderAll();
}

void CToolApp::ImguiMainMenu()
{
	m_visualizerManager.RenderMenuBar();
}

bool CToolApp::Shutdown()
{
	m_visualizerManager.Shutdown();
	return true;
}