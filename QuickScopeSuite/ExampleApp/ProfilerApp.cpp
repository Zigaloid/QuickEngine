#include "Profiler/ProfilerAnalyzer.h"
#include "GlfwMouse.h"
#include "Profiler/Profiler.h"
#include "ImGuiVisualizers/Imguibargraph.h"
#include "ImGuiVisualizers/VisualizableMetricHeuristic.h"

#include "RenderingSystem.h"
#include "Window.h"
#include "net/NexusClient.h"

#include "CoreSystem/FunctionCallManager.h"
#include "Input/WindowsGamepad.h"
#include "CoreSystem/CoreSystem.h"
#include "Input/GamepadManager.h"
#include "Input/MouseManager.h"

#include "LevelComponent.h"
#include "CameraComponent.h"
#include "Tests/ComponentSchedulerLoadTest.h"
#include "RenderComponent.h"
#include "RenderComponentSelector.h"
#include "ManipulatorComponent.h"
#include "RenderPrimitives.h"
#include "input/KeyboardShortcutManager.h"

// ImGui includes
#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include <algorithm>
#include <iomanip>
#include <sstream>
#include "Registration.h"
#include "ProfilerApp.h"

DebugChannels::CDebugChannel AppDebug("AppDebug");

using namespace Core;
using namespace Input;

static const std::string STATUS_PIPE = "Status";
static const std::string PERFORMANCE_PIPE = "Performance";
static const std::string FPS_PIPE = "Fps";
static const std::string CONSOLE_PIPE = "Console";
static const std::string PROFILER_PIPE = "Profiler";


ProfilerApp::ProfilerApp()
{
}

ProfilerApp::~ProfilerApp()
{
    Shutdown();
}

bool ProfilerApp::Initialize()
{
    // Example: in your job system worker threads, game thread, render thread, etc.
    Profiler::ProfileLogger::GetInstance().RegisterThread("Main Thread");
	DECLARE_FUNC_VLOW();
	// Core system initialization
	if (!CoreSystem::Initialize())	return false;

	
    NEXUS_CONNECT_AND_REGISTER("127.0.0.1", 9500, "ExampleApp", "nhill");
    NEXUS_SUBSCRIBE(CONSOLE_PIPE, "ANY");

    CoreSystem::GetNexusClient()->EnableAutoReconnect(std::chrono::milliseconds(1000)); // Attempt to reconnect every 5 seconds if connection is lost

	// Setup and create window
	if (!SetupWindow()) 	return false;

	// Rendering system initialization
	if (!Rendering::RenderingSystem::Initialize())	return false;

	// App specific initializations including component registration.
	// MOVED BEFORE VIEWPORT SETUP: This creates MouseManager before viewports need it
	OnInit();

	// Setup viewports (now MouseManager exists for camera registration)
	SetupViewports();
	m_viewportLayoutManager.SetLayout(ViewportLayout::Single);

	// Setup window callbacks BEFORE ImGui so ImGui can chain them
	HandleWindowCallbacks();

	// Setup ImGui integration (install_callbacks=true will chain with existing callbacks)
	SetupImGuiForWindow();

	return true;
}

bool ProfilerApp::SetupWindow()
{
    DECLARE_FUNC_VLOW();

	// Initialize GLFW first (required by Window class)
	if (!Window::initializeGLFW()) {
		AppDebug.printf("Failed to initialize GLFW\n");
		return false;
	}

    Window::WindowConfig config;
    config.width = 1280;
    config.height = 720;
    config.title = "Example Application";
    config.resizable = true;
    config.vsync = false;
    config.samples = 4; // MSAA
    config.saveWindowState = true;
    
    m_window = std::make_unique<Window>(config);
	
	return (m_window && m_window->initialize());
}

void ProfilerApp::SetupViewports()
{
	DECLARE_FUNC_VLOW();

	if (!m_window) return;

	// Initialize viewport layout manager
	m_viewportLayoutManager.Initialize(m_window.get());

	// Setup viewport callbacks
	m_viewportLayoutManager.SetupViewportCallbacks([this](Viewport& vp, const Matrix4f& viewMatrix, const Matrix4f& projectionMatrix) {
		OnViewportRender(vp, viewMatrix, projectionMatrix);
		});	
	// Update active viewport reference	
	m_activeViewport = m_viewportLayoutManager.GetActiveViewport();
}


// In ProfilerApp::SetupImGuiForWindow(), replace the entire method with:
void ProfilerApp::SetupImGuiForWindow()
{
	DECLARE_FUNC_VLOW();

	if (!m_window || !m_window->getGLFWWindow()) {
		AppDebug.printf("Cannot setup ImGui: Window not initialized\n");
		return;
	}

	if (!m_imguiManager.Initialize(m_window->getGLFWWindow(), m_GlslVersion)) {
		AppDebug.printf("Failed to initialize ImGuiManager\n");
		return;
	}

	AppDebug.printf("ImGui initialized successfully via ImGuiManager\n");
}

void ProfilerApp::Run()
{
    DECLARE_FUNC_VLOW();
    	
    if (!m_window) {
        AppDebug.printf("Cannot run: Window not initialized\n");
        return;
    }
    
    double lastTime = m_window->getTime();

    while (!m_window->shouldClose()) {
        PROFILE_FRAME();
        DECLARE_SCOPE_LOW("MainLoop");
        
        // Calculate delta time
        double currentTime = m_window->getTime();
        double deltaTime = currentTime - lastTime;
        lastTime = currentTime;
        		
        NEXUS_SEND_MESSAGE(FPS_PIPE, std::to_string(deltaTime));

        // Poll events
        m_window->pollEvents();
        
        // Update application
        OnUpdate(deltaTime);
        
        // Render
        RenderImGuiWithViewports();
        
        // Swap buffers
        m_window->swapBuffers();


    }
}

// In ProfilerApp::RenderImGuiWithViewports(), replace with:
void ProfilerApp::RenderImGuiWithViewports()
{
	DECLARE_FUNC_LOW();

	if (!m_imguiManager.IsInitialized()) return;

	// Render viewports first (this will clear and render the 3D scene)
	m_window->render();

	// Use ImGuiManager to handle frame rendering
	m_imguiManager.RenderFrame([this]() 
	{
		RenderImGui();
	});
}

void ProfilerApp::OnViewportRender(Viewport& viewport, const Matrix4f& viewMatrix, const Matrix4f& projectionMatrix)
{
    DECLARE_FUNC_LOW();
        
    Matrix4f combinedMatrix = projectionMatrix * viewMatrix;
    Rendering::RenderingSystem::GetRenderPrimitives()->SetViewProjectionMatrix(combinedMatrix);	    
    Rendering::RenderingSystem::SetCurrentlyRenderingViewport(&viewport);
    Rendering::RenderingSystem::GetRenderQueue()->ExecuteAll();
}

void ProfilerApp::OnUpdate(double deltaTime)
{
    DECLARE_FUNC_LOW();

	Core::CoreSystem::GetResourceManager()->UpdateFinalization();

	auto* scheduler = Core::CoreSystem::GetJobSystemScheduler();       
    m_fpsTracker.UpdateFromDeltaTime(deltaTime);    
    scheduler->UpdateAllAsync(deltaTime);    
    m_activeViewport = m_window->getActiveViewport();    
    scheduler->WaitForCompletion();
	//Vector3f camPos = m_activeViewport->getCamera()->getPosition();
	//m_TestHeatMap.AddValue(camPos.x,camPos.z, "FPS", 1.0f/deltaTime);

}

void ProfilerApp::Shutdown()
{
    DECLARE_FUNC_VLOW();
	
	// Cleanup ImGui via manager
	m_imguiManager.Shutdown();

	m_propertyInspector.ClearObject();
    
	OnShutdown();

    // Cleanup window (this will also cleanup viewports)
    if (m_window) {
        m_window->shutdown();
        m_window.reset();
    }
		
	Core::CoreSystem::Shutdown();

    // Cleanup GLFW
    Window::terminateGLFW();
}

void ProfilerApp::OnInit()
{
	DECLARE_FUNC_VLOW();

	// Initialize profiler controller (this will register profiler functions)
	m_profilerController.Initialize();

	// Get the component manager from CoreSystem
	auto* componentManager = CoreSystem::GetComponentManager();
	auto* scheduler = Core::CoreSystem::GetJobSystemScheduler();

	// Register component types with the component manager
	// Schedule the component types with the scheduler for async updates.
	componentManager->RegisterComponentType<GamepadManager>(GamepadManager::ClassName(), 1, 1);
	scheduler->RegisterComponentType<GamepadManager>(10, GamepadManager::ClassName());
	componentManager->RegisterComponentType<MouseManager>(MouseManager::ClassName(), 1, 1);
	scheduler->RegisterComponentType<MouseManager>(10, MouseManager::ClassName());
	componentManager->RegisterComponentType<Input::KeyboardShortcutManager>(Input::KeyboardShortcutManager::ClassName(), 1, 1);
	scheduler->RegisterComponentType<Input::KeyboardShortcutManager>(10, Input::KeyboardShortcutManager::ClassName());

	// Create and initialize the GamepadManager component
	Input::GamepadManager* gamepadManager = componentManager->CreateComponent<Input::GamepadManager>();
	gamepadManager->AddGamepad(std::unique_ptr<IGamepad>(std::make_unique<WindowsGamepad>(0).release()));

	Input::MouseManager* mouseManager = componentManager->CreateComponent<Input::MouseManager>();
	mouseManager->SetMouse(std::move(std::make_unique<GlfwMouse>(m_window->getGLFWWindow())));

	// Create and initialize the KeyboardShortcutManager component
	m_keyboardShortcutManager = componentManager->CreateComponent<Input::KeyboardShortcutManager>();
	m_keyboardShortcutManager->Initialize();
	RegisterRenderComponentTypes();

	// Initialize component manager - this will call OnInitialize on all components
	componentManager->Initialize();

	// IMPORTANT: Register keyboard shortcuts AFTER component manager is initialized
	// This ensures the KeyboardShortcutManager is properly initialized
	RegisterKeyboardShortcuts();

	// Show the frame time analysis window by default	
	m_console.SetEnabled(true);

	// Initialize LevelManager and create default level
	if (m_levelManager.Initialize(componentManager))
	{
		m_levelManager.CreateLevel("Main Scene");
	}

	// Create and initialize the ManipulatorComponent (Max/Maya style gizmo).
	m_manipulator = componentManager->CreateComponent<Rendering::ManipulatorComponent>();
	m_RenderComponentSelector = componentManager->CreateComponent<Rendering::RenderComponentSelector>();
	m_RenderComponentSelector->SetManipulatorComponent(m_manipulator);
	m_RenderComponentSelector->Initialize();

	// Setup PropertyInspector callback for selection changes
	m_RenderComponentSelector->SetSelectionChangeCallback(
		[this](Rendering::RenderComponent* selectedComponent) {
			OnRenderComponentSelectionChanged(selectedComponent);
		}
	);

	if (m_manipulator)
	{
		m_manipulator->Initialize();
	}

	// Initialize UI Window Manager
	m_uiWindowManager.Initialize(
		&m_profilerController,
		&m_levelManager,
		&m_fpsTracker,
		&m_console,
		&m_propertyInspector
	);

	// Setup UI callbacks to use ViewportLayoutManager
	m_uiWindowManager.SetViewportLayoutCallback([this](ViewportLayout layout) {
		m_viewportLayoutManager.SetLayout(layout);
		// Update active viewport reference after layout change
		m_activeViewport = m_viewportLayoutManager.GetActiveViewport();
		m_uiWindowManager.SetActiveViewport(m_activeViewport);
		});

	m_uiWindowManager.SetViewportInfoCallback([this]() {
		return m_viewportLayoutManager.GetCurrentLayoutName();
		});

	m_uiWindowManager.SetViewportCheckCallback([this](ViewportLayout layout) {
		return m_viewportLayoutManager.IsCurrentLayout(layout);
		});

	// Set initial window references
	m_uiWindowManager.SetWindowReference(m_window.get());
	m_uiWindowManager.SetViewportLayoutEnum(m_viewportLayoutManager.GetCurrentLayout());

	// Show the frame time analysis window by default
	m_console.SetEnabled(true);
	AppDebug.printf("Application initialized!\n");
}


// Updated OnShutdown method
void ProfilerApp::OnShutdown()
{	
	m_levelManager.Shutdown();
	m_imguiManager.Shutdown();	
	m_viewportLayoutManager.Shutdown();	
	m_profilerController.Shutdown();
	AppDebug.printf("Application shutting down!\n");
}

void ProfilerApp::HandleWindowCallbacks()
{
	DECLARE_FUNC_VLOW();

	if (!m_window) return;

	// Set up window callbacks
	m_window->setKeyCallback([this](int key, int scancode, int action, int mods) {
		// Check if ImGui wants to capture this input
		ImGuiIO& io = ImGui::GetIO();
		if (io.WantCaptureKeyboard) {
			// Let ImGui handle the input, but still allow certain global shortcuts
			// Only process essential shortcuts when ImGui has focus
			if (action == GLFW_PRESS) {
				// Allow console toggle even when ImGui has focus
				if (key == GLFW_KEY_GRAVE_ACCENT && mods == 0) {
					if (m_keyboardShortcutManager) {
						m_keyboardShortcutManager->HandleKeyInput(key, scancode, action, mods);
					}
					return;
				}
			}
			return; // Let ImGui handle other inputs
		}

		// Forward to shortcut manager for global shortcuts
		if (m_keyboardShortcutManager && m_keyboardShortcutManager->IsEnabled()) {
			m_keyboardShortcutManager->HandleKeyInput(key, scancode, action, mods);
		}

		// Forward to active viewport for camera controls
		if (m_activeViewport) {
			m_activeViewport->handleKeyboard(key, scancode, action, mods);
		}
		});

	m_window->setWindowResizeCallback([this](int width, int height) {
		// Handle window resize
		AppDebug.printf("Window resized to %dx%d\n", width, height);
		UpdateImGuiDPIScaling();
		});

	// Set up GLFW monitor callback for DPI changes
	GLFWwindow* glfwWindow = m_window->getGLFWWindow();
	if (glfwWindow) {
		// Store the ProfilerApp instance in the window user pointer for callbacks
		//glfwSetWindowUserPointer(glfwWindow, this);

		// Monitor the window for DPI scale changes
		glfwSetWindowContentScaleCallback(glfwWindow, [](GLFWwindow* window, float xscale, float yscale) {
			ProfilerApp* app = static_cast<ProfilerApp*>(glfwGetWindowUserPointer(window));
			if (app) {
				app->OnDPIScaleChanged(xscale, yscale);
			}
			});
	}
}

// Update RenderImGui() to use UIWindowManager:
void ProfilerApp::RenderImGui()
{
	DECLARE_FUNC_LOW();

	// Update active viewport reference and refresh UIWindowManager's reference
	m_activeViewport = m_viewportLayoutManager.GetActiveViewport();
	m_uiWindowManager.RefreshActiveViewport(); // Use the new safe refresh method
	m_uiWindowManager.SetViewportLayoutEnum(m_viewportLayoutManager.GetCurrentLayout());
	m_uiWindowManager.SetViewportSplitRatio(m_viewportLayoutManager.GetSplitRatio());

	m_uiWindowManager.RenderAllWindows();

	ImGui::End(); // End the DockSpace window
}
// Add validation and better error handling to RegisterKeyboardShortcuts
void ProfilerApp::RegisterKeyboardShortcuts()
{
	DECLARE_FUNC_VLOW();

	if (!m_keyboardShortcutManager) {
		AppDebug.printf("ERROR: KeyboardShortcutManager not available for shortcut registration\n");
		return;
	}

	// Verify the shortcut manager is initialized
	if (!m_keyboardShortcutManager->IsInitialized()) {
		AppDebug.printf("ERROR: KeyboardShortcutManager not initialized when registering shortcuts\n");
		return;
	}

	using namespace Input;
	int successCount = 0;
	int totalCount = 0;

	// Helper lambda to register and count shortcuts
	auto registerShortcut = [&](const std::string& name, const std::string& desc, int key, KeyModifier mods, std::function<void()> callback) {
		totalCount++;
		if (m_keyboardShortcutManager->RegisterShortcut(name, desc, key, mods, std::move(callback))) {
			successCount++;
		}
		else {
			AppDebug.printf("Failed to register shortcut: %s\n", name.c_str());
		}
		};

	// Register existing shortcuts that were hardcoded

	registerShortcut("ToggleConsole", "Toggle console window", GLFW_KEY_GRAVE_ACCENT, KeyModifier::None,
		[this]() { m_uiWindowManager.ToggleConsole(); });

	registerShortcut("TogglePropertyInspector", "Toggle property inspector window", GLFW_KEY_I, KeyModifier::Ctrl,
		[this]() { m_uiWindowManager.TogglePropertyInspector(); });

	registerShortcut("AddRenderComponent", "Add render component in front of camera", GLFW_KEY_R, KeyModifier::Ctrl,
		[this]() {
			m_levelManager.AddRenderComponentInFrontOfCamera(m_activeViewport);
		});

	registerShortcut("AddGLTFComponent", "Add GLTF render component in front of camera", GLFW_KEY_R, KeyModifier::Ctrl | KeyModifier::Shift,
		[this]() {
			m_levelManager.AddGLTFRenderComponentInFrontOfCamera(m_activeViewport, "assets/Datsun/Datsun.gltf");
		});

	// Quick viewport layout shortcuts
	registerShortcut("SingleViewport", "Set single viewport layout", GLFW_KEY_1, KeyModifier::Ctrl,
		[this]() {
			m_viewportLayoutManager.SetLayout(ViewportLayout::Single);
			m_activeViewport = m_viewportLayoutManager.GetActiveViewport();
			m_uiWindowManager.RefreshActiveViewport();
		});

	registerShortcut("HorizontalSplit", "Set horizontal split viewport layout", GLFW_KEY_2, KeyModifier::Ctrl,
		[this]() {
			m_viewportLayoutManager.SetLayout(ViewportLayout::HorizontalSplit);
			m_activeViewport = m_viewportLayoutManager.GetActiveViewport();
			m_uiWindowManager.RefreshActiveViewport();
		});

	registerShortcut("VerticalSplit", "Set vertical split viewport layout", GLFW_KEY_3, KeyModifier::Ctrl,
		[this]() {
			m_viewportLayoutManager.SetLayout(ViewportLayout::VerticalSplit);
		 m_activeViewport = m_viewportLayoutManager.GetActiveViewport();
			m_uiWindowManager.RefreshActiveViewport();
		});

	registerShortcut("QuadLayout", "Set quad viewport layout", GLFW_KEY_4, KeyModifier::Ctrl,
		[this]() {
			m_viewportLayoutManager.SetLayout(ViewportLayout::Quad);
			m_activeViewport = m_viewportLayoutManager.GetActiveViewport();
			m_uiWindowManager.RefreshActiveViewport();
		});


	AppDebug.printf("Successfully registered %d/%d keyboard shortcuts\n", successCount, totalCount);

	// Enable the shortcut manager
	m_keyboardShortcutManager->SetEnabled(true);
}
void ProfilerApp::OnDPIScaleChanged(float xscale, float yscale)
{
	m_imguiManager.HandleDPIScaleChange(xscale, yscale);
}

void ProfilerApp::UpdateImGuiDPIScaling()
{
	if (m_window && m_window->getGLFWWindow()) {
		m_imguiManager.UpdateDPIScaling(m_window->getGLFWWindow());
	}
}

// Add this new method to handle selection changes
void ProfilerApp::OnRenderComponentSelectionChanged(Rendering::RenderComponent* selectedComponent)
{
	if (selectedComponent) {
		// Set the selected component in the property inspector
		m_propertyInspector.SetObject(selectedComponent);

		// Optionally show the property inspector window when something is selected
		if (!m_uiWindowManager.GetShowPropertyInspector()) {
			m_uiWindowManager.SetShowPropertyInspector(true);
		}

		AppDebug.printf("PropertyInspector updated with selected RenderComponent: %s (ID: %u)\n",
			selectedComponent->GetRflClassName() ? selectedComponent->GetRflClassName() : "Unknown",
			selectedComponent->GetId());
	}
	else {
		// Clear the property inspector when nothing is selected
		m_propertyInspector.ClearObject();
		AppDebug.printf("PropertyInspector cleared - no selection\n");
	}
}
