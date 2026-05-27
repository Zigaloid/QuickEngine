#include "Game.h"
#include "CommandConsole.h"
#include "ImGui3DViewVisualizer.h"
#include "KeyboardShortcutManager.h"
#include "MessageSystem/MessageBus.h"
#include "Net/NexusClient.h"
#include "SharedNexusDefines.h"
#include <iostream>
#include "PropertyWidgetMapRegistry.h"
#include "ShaderResource.h"
#include "CoreSystem\CoreDebugChannels.h"
#include "CoreSystem\CoreSystem.h"
#include "CoreSystem\AppConfig.h"
#include "Input\InputActionManager.h"
#include "MeshComponent.h"
#include "PhysicsBodyComponent.h"
#include "HeightFieldPhysicsComponent.h"
#include "LightComponent.h"
#include "LightManagerComponent.h"
#include "ComponentSystem/ComponentDependencyDefinition.h"
#include "ComponentSystem/ComponentDependencyDefinition.h"
#include "TransformComponent.h"
#include "CameraComponent.h"
#include "Input\MouseManager.h"
#include "Input\WindowsMouse.h"
#include "Input\GamepadManager.h"
#include "Input\WindowsGamepad.h"

#include "entry\entry.h"
#include "imgui.h"

bool GameApp::Initialize()
{	
	CTextureResourceReference testTextureRef;
	testTextureRef.OnLoaded();

	Rendering::BgfxRenderPrimitives::Instance().Initialize();

	m_visualizerManager.Initialize();
	m_visualizerManager.Register("Command Console", std::make_unique<CommandConsole>(), false);

    // Ensure FunctionCallManager exists so console commands can be registered
	Core::CoreSystem::Initialize(Core::InitFlag::FunctionCallManager);

	// App setup component types.
	RegisterComponents();

	// Load component dependency definitions and apply to the scheduler.
	{
		ComponentDependencyDefinitionList depList;
		std::string depPath = Core::AppConfig::Instance().ResolvePath("./Assets/Editor/ComponentDeps.cdep.obj.json");
		depList.SafeRead(depPath);
		Core::CoreSystem::GetJobSystemScheduler()->AddDependencies(depList);
	}
	// Create and initialize a MouseManager instance so it is discoverable
	// and receives OnUpdate calls via the scheduler each frame.
	auto* componentManager = Core::CoreSystem::GetComponentManager();
	auto* mouseManager = componentManager->CreateComponent<Input::MouseManager>();	
	auto* gamepadManager = componentManager->CreateComponent<Input::GamepadManager>();
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
    if (gamepadManager)
	{
		// Create an XInput-backed gamepad for player 0 by default.
		auto windowsGamepad = std::make_unique<Input::WindowsGamepad>(0);
		windowsGamepad->Initialize();
		gamepadManager->SetDefaultConfig(Input::GamepadConfig{});
		gamepadManager->AddGamepad(std::move(windowsGamepad));
	}
    auto inputActionManager = componentManager->CreateComponent<Input::InputActionManager>();
	// Register the component instance with CoreSystem so global lookups succeed
	Core::CoreSystem::SetActionManager(inputActionManager);

	inputActionManager->AttachMouseManager(mouseManager);
	inputActionManager->AttachGamepadManager(gamepadManager);

	PhysicsManager::Config physicsConfig;
	physicsConfig.maxBodies = 1024;
	physicsConfig.gravity   = -9.81f;
	m_physicsManager.Initialize(physicsConfig);
	PhysicsManager::SetInstance(&m_physicsManager);

	m_RootLevel = componentManager->CreateComponent<CLevelComponent>();
	std::string levelPath = Core::AppConfig::Instance().ResolvePath("./Assets/Levels/World.lvl.obj.json");
	m_RootLevel->SafeRead(levelPath);
	m_lastLevelPath = levelPath;

	componentManager->Initialize();

	Input::KeyboardShortcutManager::Instance().Initialize();

	RegisterConsoleCommands();

	// If the Command Console exists, refresh its FunctionCallManager-backed commands
	{
		auto* console = m_visualizerManager.GetVisualizerAs<CommandConsole>("Command Console");
		if (console) {
			console->RefreshFunctionCallManagerCommands();
		}
	}

	NEXUS_SUBSCRIBE_CALLBACK(GAME_PIPE, "ANY", HandleQuickEditCommand);

	// Search the level hierarchy for a camera component and attach it
	if (m_RootLevel)
	{
		if (auto* levelCamera = m_RootLevel->FindDescendant<CCameraComponent>())
		{
			SetActiveCamera(levelCamera);
		}
	}

	return true;
}

void GameApp::HandleQuickEditCommand(const std::string& messageBody)
{
	DECLARE_FUNC_VLOW();
	// Simple command parsing - expects "command arg1 arg2 ..."
	std::istringstream iss(messageBody);
	std::string command;
	iss >> command;

	if (command == "Level_Reload")
	{
		m_visualizerManager.GetVisualizerAs<CommandConsole>("Command Console")->SubmitCommand("Level_Reload()");
	}
}

void GameApp::RegisterComponents()
{
	auto* componentManager = Core::CoreSystem::GetComponentManager();
	auto* scheduler = Core::CoreSystem::GetJobSystemScheduler();

	componentManager->RegisterComponentType<CLevelComponent>();
	scheduler->RegisterComponentType<CLevelComponent>(0, "Level");

	componentManager->RegisterComponentType<Input::MouseManager>();
	scheduler->RegisterComponentType<Input::MouseManager>(0, "MouseManager");

	componentManager->RegisterComponentType<Input::GamepadManager>();
	scheduler->RegisterComponentType<Input::GamepadManager>(0, "GamepadManager");

	componentManager->RegisterComponentType<CEntityComponent>();
	scheduler->RegisterComponentType<CEntityComponent>(0, "Entity");

	componentManager->RegisterComponentType<CMeshComponent>();
	scheduler->RegisterComponentType<CMeshComponent>(0, "Mesh");

	componentManager->RegisterComponentType<Input::InputActionManager>();
	scheduler->RegisterComponentType<Input::InputActionManager>(0, "InputActions");

	componentManager->RegisterComponentType<CPhysicsBodyComponent>();
	scheduler->RegisterComponentType<CPhysicsBodyComponent>(0, "PhysicsBody");

	componentManager->RegisterComponentType<CTransformComponent>();
	scheduler->RegisterComponentType<CTransformComponent>(0, "Transform");

	componentManager->RegisterComponentType<CCameraComponent>();
	scheduler->RegisterComponentType<CCameraComponent>(0, "Camera");

	componentManager->RegisterComponentType<CHeightFieldPhysicsComponent>();
	scheduler->RegisterComponentType<CHeightFieldPhysicsComponent>(0, "HeightFieldPhysics");

	componentManager->RegisterComponentType<CLightComponent>();
	scheduler->RegisterComponentType<CLightComponent>(0, "Light");

	componentManager->RegisterComponentType<CLightManagerComponent>();
	scheduler->RegisterComponentType<CLightManagerComponent>(0, "LightManager");

}

void GameApp::Update(double deltaTime)
{
	DECLARE_FUNC_VLOW();
	if(Core::CoreSystem::GetResourceManager()->GetPendingFinalizationCount() == 0)
	{
		m_physicsManager.Update(static_cast<float>(deltaTime));
	}
	Input::KeyboardShortcutManager::Instance().Update(deltaTime);
	m_visualizerManager.Update(static_cast<float>(deltaTime));
	Core::MessageSystem::MessageBus::Get().ProcessAll();

	NEXUS_SEND_MESSAGE(FPS_PIPE, MSG_TYPE_FRAME_TIME, std::to_string(deltaTime).c_str());

}

void GameApp::Render(double deltaTime)
{
	DECLARE_FUNC_VLOW();
	// Apply the active camera to view 0 (the main backbuffer).
	float viewMtx[16];
	float projMtx[16];

	const bgfx::Stats* stats  = bgfx::getStats();
	const float        aspect = static_cast<float>(stats->width)
							  / static_cast<float>(stats->height);

	// Use active camera component if set, otherwise use default m_camera
	if (m_activeCamera)
	{
		m_activeCamera->GetViewMatrix(viewMtx);
		m_activeCamera->GetProjectionMatrix(projMtx, aspect);
	}
	else
	{
		m_camera.GetViewMatrix(viewMtx);
		m_camera.GetProjectionMatrix(projMtx, aspect);
	}

	bgfx::setViewTransform(0, viewMtx, projMtx);

	Rendering::BgfxRenderPrimitives::Instance().RenderGrid(0, 100.0f, 1.0f, 0xff808080);

#ifdef JPH_DEBUG_RENDERER
	m_physicsManager.DebugDraw(viewMtx);
#endif

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

void GameApp::LoadLevel(std::string filePath)
{
	std::string resolvedPath = Core::AppConfig::Instance().ResolvePath(filePath);

	auto* componentManager = Core::CoreSystem::GetComponentManager();
	if (m_RootLevel)
	{
		componentManager->ReleaseComponent(m_RootLevel);
		m_RootLevel = nullptr;
	}

	m_RootLevel = componentManager->CreateComponent<CLevelComponent>();
	m_RootLevel->SafeRead(resolvedPath);
	m_RootLevel->Initialize();
	m_lastLevelPath = resolvedPath;

	// Search the new level hierarchy for a camera component and attach it
	if (m_RootLevel)
	{
		if (auto* levelCamera = m_RootLevel->FindDescendant<CCameraComponent>())
		{
			SetActiveCamera(levelCamera);
		}
		else
		{
			// No camera found in level, revert to default
			SetActiveCamera(nullptr);
		}
	}
}

void GameApp::ReloadLevel()
{
	if (m_lastLevelPath.empty())
	{
		return;
	}

	auto* componentManager = Core::CoreSystem::GetComponentManager();
	if (m_RootLevel)
	{
		componentManager->ReleaseComponent(m_RootLevel);
		m_RootLevel = nullptr;
	}

	m_RootLevel = componentManager->CreateComponent<CLevelComponent>();
	m_RootLevel->SafeRead(m_lastLevelPath);
	m_RootLevel->Initialize();

	// Search the reloaded level hierarchy for a camera component and attach it
	if (m_RootLevel)
	{
		if (auto* levelCamera = m_RootLevel->FindDescendant<CCameraComponent>())
		{
			SetActiveCamera(levelCamera);
		}
		else
		{
			// No camera found in level, revert to default
			SetActiveCamera(nullptr);
		}
	}
}

void GameApp::RegisterConsoleCommands()
{
	auto* funcManager = Core::CoreSystem::GetFunctionManager();
	if (!funcManager)
		return;

	funcManager->RegisterFunction<void, std::string>(
		"Level_Load", [this](std::string path) { LoadLevel(path); }, {"filePath"});

	funcManager->RegisterFunction<void>(
		"Level_Reload", std::function<void()>([this]() { ReloadLevel(); }));
}

bool GameApp::Shutdown()
{
	auto* mouseManager = Core::CoreSystem::GetFirstComponentOfType<Input::MouseManager>();
	if (mouseManager && m_cameraController)
		mouseManager->RemoveInputHandler(m_cameraController);

	PhysicsManager::SetInstance(nullptr);
	Rendering::BgfxRenderPrimitives::Instance().Shutdown();
	m_physicsManager.Shutdown();
	m_visualizerManager.Shutdown();
	return true;
}
