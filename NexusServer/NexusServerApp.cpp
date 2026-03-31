#include "NexusServerApp.h"
#include "NexusFlowGraph.h"
#include "CommandConsole.h"
#include "NexusLogVisualizer.h"

bool NexusServerApp::Initialize()
{
	// Initialize application-specific resources here
	m_visualizerManager.Initialize();
	m_visualizerManager.Register("Flow Graph", std::make_unique<NexusFlowGraph>(), true);
	m_visualizerManager.Register("Command Console", std::make_unique<CommandConsole>(), true);
	m_visualizerManager.Register("Nexus Log", std::make_unique<NexusLogVisualizer>(), true);

	// Start the NexusServer
	if (!m_NexusServer.Start("0.0.0.0", 9500)) {		
		return 1;
	}

	// Connect the flow graph visualizer to the server
	NexusFlowGraph* flowGraphViz = m_visualizerManager.GetVisualizerAs<NexusFlowGraph>("Flow Graph");
	flowGraphViz->SetServer(&m_NexusServer);
	m_NexusServer.SetMessageCallback([this](const SNexusMessage& msg)
	{
		NexusLogVisualizer* nexusLog = m_visualizerManager.GetVisualizerAs<NexusLogVisualizer>("Nexus Log");
		nexusLog->AddEntry(msg.senderApp, msg.pipeName, msg.body);
	});


	return true;
}
		
void NexusServerApp::Update(double deltaTime)
{

}

void NexusServerApp::Render(double deltaTime)
{

}

void NexusServerApp::ImguiUpdate()
{
	m_visualizerManager.RenderAll();
}

void NexusServerApp::ImguiMainMenu()
{
	m_visualizerManager.RenderMenuBar();
}

bool NexusServerApp::Shutdown()
{
	return true;
}
