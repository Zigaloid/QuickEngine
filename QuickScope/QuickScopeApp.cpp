
#include "CoreSystem/CoreSystem.h"
#include "Net/NexusClient.h"
#include "SharedNexusDefines.h"

#include "QuickScopeApp.h"
#include "CommandConsole.h"
#include "FPSTracker.h"
#include "ProfilerSessionManager.h"


bool QuickScopeApp::Initialize()
{
	// Initialize application-specific resources here
	NEXUS_CONNECT_AND_REGISTER("127.0.0.1", 9500, "QuickScope", "nhill");
	NEXUS_SUBSCRIBE_CALLBACK(FPS_PIPE, "ANY", HandleFPSMessage);
	m_visualizerManager.Initialize();
	m_visualizerManager.Register("Command Console", std::make_unique<CommandConsole>(), true);
	m_visualizerManager.Register("FPS Analytics", std::make_unique<FPSTracker>(), true);
	m_visualizerManager.Register("Profiler Sessions", std::make_unique<ProfilerSessionManager>(), true);	


	return true;
}

void QuickScopeApp::Update(double deltaTime)
{
	FlushFpsTracking();
}

void QuickScopeApp::Render(double deltaTime)
{

}

void QuickScopeApp::ImguiUpdate()
{
	m_visualizerManager.RenderAll();
}

void QuickScopeApp::ImguiMainMenu()
{
	m_visualizerManager.RenderMenuBar();
}

bool QuickScopeApp::Shutdown()
{
	return true;
}

void QuickScopeApp::FlushFpsTracking()
{
	// Drain pending FPS updates on the main thread
	std::lock_guard<std::mutex> lock(m_fpsMutex);
	FPSTracker* tracker = m_visualizerManager.GetVisualizerAs<FPSTracker>("FPS Analytics");
	for (float dt : m_pendingFPSUpdates)
	{
		tracker->UpdateFromDeltaTime(dt);
	}
	m_pendingFPSUpdates.clear();
}
void QuickScopeApp::HandleFPSMessage(const std::string& messageBody)
{
	float value = std::stof(messageBody);
	std::lock_guard<std::mutex> lock(m_fpsMutex);
	m_pendingFPSUpdates.push_back(value);
}
