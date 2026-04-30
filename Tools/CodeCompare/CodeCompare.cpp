#include "CoreSystem/CoreSystem.h"
#include "Net/NexusClient.h"
#include "..\SharedNexusDefines.h"

#include "CodeCompare.h"
#include "CommandConsole.h"

#include "imgui.h"

bool CToolApp::Initialize()
{
	NEXUS_CONNECT_AND_REGISTER("127.0.0.1", 9500, "CodeCompare", "nhill");
	Core::CoreSystem::GetNexusClient()->EnableAutoReconnect();

	m_visualizerManager.Register("Command Console",
		std::make_unique<CommandConsole>(), true);
	m_visualizerManager.Register("Code Comparison",
		std::make_unique<CodeCompareVisualizer>(), true);

	m_visualizerManager.Initialize();

	return true;
}

void CToolApp::Update(double /*deltaTime*/)
{
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