#include "Game.h"
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
#include "Input\MouseManager.h"
#include "Input\WindowsMouse.h"
#include "entry\entry.h"
#include "imgui.h"


std::shared_ptr<CShaderResource> testShader;
bool GameApp::Initialize()
{
	CTextureResourceReference testTextureRef;
	testTextureRef.OnLoaded();

	m_primitives.Initialize();

	m_visualizerManager.Initialize();
	m_visualizerManager.Register("Command Console", std::make_unique<CommandConsole>(), false);

	// App setup component types.
	RegisterComponents();

	PhysicsManager::Config physicsConfig;
	physicsConfig.maxBodies = 1024;
	physicsConfig.gravity   = -9.81f;
	m_physicsManager.Initialize(physicsConfig);

	// Register the camera controller with the core mouse system.
	auto* mouseManager = Core::CoreSystem::GetFirstComponentOfType<Input::MouseManager>();
	if (mouseManager)
	{
		// Create and register a Windows mouse with the native window handle.
		HWND hwnd = static_cast<HWND>(entry::getNativeWindowHandle(entry::kDefaultWindowHandle));

		Input::MouseConfig config;
		config.enableRawInput = false; // Use GetCursorPos polling — raw input requires WndProc integration

		auto windowsMouse = std::make_unique<Input::WindowsMouse>(hwnd, config);
		windowsMouse->Initialize();
		mouseManager->SetMouse(std::move(windowsMouse));

		m_cameraController = std::make_shared<GameCameraController>(m_camera);
		mouseManager->AddInputHandler(m_cameraController);
	}

	return true;
}

void GameApp::RegisterComponents()
{
	auto* componentManager = Core::CoreSystem::GetComponentManager();
	auto* scheduler = Core::CoreSystem::GetJobSystemScheduler();

	componentManager->RegisterComponentType<CMeshComponent>();
	scheduler->RegisterComponentType<CMeshComponent>(0, "Mesh");

	componentManager->RegisterComponentType<Input::MouseManager>();
	scheduler->RegisterComponentType<Input::MouseManager>(0, "MouseManager");

	// Create and initialize a MouseManager instance so it is discoverable
	// and receives OnUpdate calls via the scheduler each frame.
	auto* mouseManager = componentManager->CreateComponent<Input::MouseManager>();
	if (mouseManager)
	{
		mouseManager->Initialize();
	}
}

void GameApp::Update(double deltaTime)
{
	m_physicsManager.Update(static_cast<float>(deltaTime));
	m_visualizerManager.Update(static_cast<float>(deltaTime));
	Core::MessageSystem::MessageBus::Get().ProcessAll();

	auto* scheduler = Core::CoreSystem::GetJobSystemScheduler();
	scheduler->UpdateAllAsync(static_cast<float>(deltaTime));
	scheduler->WaitForCompletion();
}

void GameApp::Render(double deltaTime)
{
	// Apply the game camera to view 0 (the main backbuffer).
	float viewMtx[16];
	float projMtx[16];

	const bgfx::Stats* stats  = bgfx::getStats();
	const float        aspect = static_cast<float>(stats->width)
	                          / static_cast<float>(stats->height);

	m_camera.GetViewMatrix(viewMtx);
	m_camera.GetProjectionMatrix(projMtx, aspect);
	
	bgfx::setViewTransform(0, viewMtx, projMtx);

	m_primitives.RenderGrid(0, 100.0f, 1.0f, 0xff808080);
	
}

void GameApp::ImguiUpdate()
{
    // Render all registered visualizers (they dock into the GameAppDockSpace)
    m_visualizerManager.RenderAll();
}

void GameApp::ImguiMainMenu()
{
	m_visualizerManager.RenderMenuBar();
}

bool GameApp::Shutdown()
{
	auto* mouseManager = Core::CoreSystem::GetFirstComponentOfType<Input::MouseManager>();
	if (mouseManager && m_cameraController)
		mouseManager->RemoveInputHandler(m_cameraController);

	m_physicsManager.Shutdown();
	m_visualizerManager.Shutdown();
	return true;
}
