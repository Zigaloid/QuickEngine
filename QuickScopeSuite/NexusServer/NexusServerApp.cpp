#include <algorithm>
#include <iomanip>
#include <sstream>
#include "Registration.h"

#include <iostream>
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "Net/NexusServer.h"
#include <Windows.h>
#include "NexusServerApp.h"

#include "gl\glew.h"
#include "glfw\glfw3.h"

#include "Profiler/ProfilerAnalyzer.h"
#include "Profiler/Profiler.h"

#include "CoreSystem/FunctionCallManager.h"
#include "Input/WindowsGamepad.h"
#include "CoreSystem/CoreSystem.h"
#include "Analysis/HeatMapContainer.h"
#include "Input/GamepadManager.h"
#include "Input/MouseManager.h"

#include "Tests/ComponentSchedulerLoadTest.h"
#include "input/KeyboardShortcutManager.h"

// ImGui includes
#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

DebugChannels::CDebugChannel AppDebug("AppDebug");

using namespace Core;
using namespace Input;

CNexusServerApp::CNexusServerApp()
{
}

CNexusServerApp::~CNexusServerApp()
{
    Shutdown();
}

CNexusServer g_NexusServer;

bool CNexusServerApp::Initialize()
{
	DECLARE_FUNC_VLOW();
	// Core system initialization
	if (!CoreSystem::Initialize(Core::InitFlag::All & ~Core::InitFlag::NexusClient))	return false;

	// Setup and create GLFW window
	if (!SetupGLFWWindow()) return false;

	// App specific initializations including component registration.
	OnInit();

	// Setup ImGui integration
	SetupImGui();

	// Setup window callbacks!
	HandleWindowCallbacks();

    if (!g_NexusServer.Start("0.0.0.0", 9500)) {
        std::cerr << "Failed to start server\n";
        return 1;
    }

    // Connect the flow graph visualizer to the server
    m_flowGraph.SetServer(&g_NexusServer);

	g_NexusServer.SetMessageCallback([this](const SNexusMessage& msg) 
	{
        m_nexusLog.AddEntry(msg.senderApp, msg.pipeName, msg.body);
    });

	return true;
}

bool CNexusServerApp::SetupGLFWWindow()
{
    DECLARE_FUNC_VLOW();

	// Initialize GLFW
	if (!glfwInit()) {
		AppDebug.printf("Failed to initialize GLFW\n");
		return false;
	}

	// Set OpenGL version hints
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_COMPAT_PROFILE);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

	// Create GLFW window
	m_glfwWindow = glfwCreateWindow(m_windowWidth, m_windowHeight, "Nexus Server", nullptr, nullptr);
	if (!m_glfwWindow) {
		AppDebug.printf("Failed to create GLFW window\n");
		glfwTerminate();
		return false;
	}

	glfwMakeContextCurrent(m_glfwWindow);

	// Initialize GLEW
	if (glewInit() != GLEW_OK) {
		AppDebug.printf("Failed to initialize GLEW\n");
		glfwDestroyWindow(m_glfwWindow);
		m_glfwWindow = nullptr;
		glfwTerminate();
		return false;
	}

	glfwSwapInterval(0); // Disable vsync

	return true;
}

void CNexusServerApp::SetupImGui()
{
	DECLARE_FUNC_VLOW();

	if (!m_glfwWindow) {
		AppDebug.printf("Cannot setup ImGui: GLFW window not initialized\n");
		return;
	}

	if (!m_imguiManager.Initialize(m_glfwWindow, m_GlslVersion)) {
		AppDebug.printf("Failed to initialize ImGuiManager\n");
		return;
	}

	AppDebug.printf("ImGui initialized successfully via ImGuiManager\n");
}

void CNexusServerApp::Run()
{
    DECLARE_FUNC_VLOW();
    	
    if (!m_glfwWindow) {
        AppDebug.printf("Cannot run: GLFW window not initialized\n");
        return;
    }
    
    double lastTime = glfwGetTime();
    
    while (!glfwWindowShouldClose(m_glfwWindow)) {
        PROFILE_FRAME();
        DECLARE_SCOPE_LOW("MainLoop");
        
        // Calculate delta time
        double currentTime = glfwGetTime();
        double deltaTime = currentTime - lastTime;
        lastTime = currentTime;
        
        // Poll events
        glfwPollEvents();
        
        // Update application
        OnUpdate(deltaTime);
        
        // Render
        RenderImGuiFrame();
        
        // Swap buffers
        glfwSwapBuffers(m_glfwWindow);
    }
}

void CNexusServerApp::RenderImGuiFrame()
{
	DECLARE_FUNC_LOW();

	if (!m_imguiManager.IsInitialized()) return;

	// Clear the window
	glfwGetFramebufferSize(m_glfwWindow, &m_windowWidth, &m_windowHeight);
	glViewport(0, 0, m_windowWidth, m_windowHeight);
	glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	// Use ImGuiManager to handle frame rendering
	m_imguiManager.RenderFrame([this]() 
	{
		RenderImGui();
	});
}

void CNexusServerApp::OnUpdate(double deltaTime)
{
    DECLARE_FUNC_LOW();

	Core::CoreSystem::GetResourceManager()->UpdateFinalization();

	auto* scheduler = Core::CoreSystem::GetJobSystemScheduler();           
    scheduler->UpdateAllAsync(deltaTime);    
    scheduler->WaitForCompletion();
}

void CNexusServerApp::Shutdown()
{
    DECLARE_FUNC_VLOW();

	g_NexusServer.SetMessageCallback(nullptr);
	// Cleanup ImGui via manager
	m_imguiManager.Shutdown();
    
	OnShutdown();

    // Cleanup GLFW window
    if (m_glfwWindow) {
        glfwDestroyWindow(m_glfwWindow);
        m_glfwWindow = nullptr;
    }
		
	Core::CoreSystem::Shutdown();

    // Cleanup GLFW
    glfwTerminate();
}

void CNexusServerApp::OnInit()
{
	DECLARE_FUNC_VLOW();

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

	// Create and initialize the MouseManager component, and provide a GlfwMouse device
	//input::MouseManager* mouseManager = componentManager->CreateComponent<Input::MouseManager>();
	//mouseManager->SetMouse(std::make_unique<Input::GlfwMouse>(m_glfwWindow));

	// Create and initialize the KeyboardShortcutManager component
	m_keyboardShortcutManager = componentManager->CreateComponent<Input::KeyboardShortcutManager>();
	m_keyboardShortcutManager->Initialize();
	
	// Initialize component manager - this will call OnInitialize on all components
	componentManager->Initialize();

	// IMPORTANT: Register keyboard shortcuts AFTER component manager is initialized
	// This ensures the KeyboardShortcutManager is properly initialized
	RegisterKeyboardShortcuts();

	// Show the frame time analysis window by default	
	m_console.SetEnabled(true);

	m_nexusLog.Initialize();

	// Create and initialize the ManipulatorComponent (Max/Maya style gizmo).
		// Initialize UI Window Manager
	m_uiWindowManager.Initialize(
		&m_console,
		&m_flowGraph,
		&m_nexusLog);

	// Show the frame time analysis window by default
	m_uiWindowManager.SetShowFrameTimeAnalysis(true);
	m_console.SetEnabled(true);
	AppDebug.printf("Application initialized!\n");

}

// Updated OnShutdown method
void CNexusServerApp::OnShutdown()
{	
	m_imguiManager.Shutdown();	
	AppDebug.printf("Application shutting down!\n");
}

void CNexusServerApp::HandleWindowCallbacks()
{
	DECLARE_FUNC_VLOW();

	if (!m_glfwWindow) return;

	// Store this pointer for GLFW callbacks
	glfwSetWindowUserPointer(m_glfwWindow, this);

	// Set up key callback
	glfwSetKeyCallback(m_glfwWindow, [](GLFWwindow* window, int key, int scancode, int action, int mods) {
		CNexusServerApp* app = static_cast<CNexusServerApp*>(glfwGetWindowUserPointer(window));
		if (!app) return;

		// Handle ESC to close
		if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
			glfwSetWindowShouldClose(window, GLFW_TRUE);
			return;
		}

		// Check if ImGui wants to capture this input
		ImGuiIO& io = ImGui::GetIO();
		if (io.WantCaptureKeyboard) {
			// Allow console toggle even when ImGui has focus
			if (action == GLFW_PRESS && key == GLFW_KEY_GRAVE_ACCENT && mods == 0) {
				if (app->m_keyboardShortcutManager) {
					app->m_keyboardShortcutManager->HandleKeyInput(key, scancode, action, mods);
				}
			}
			return; // Let ImGui handle other inputs
		}

		// Forward to shortcut manager for global shortcuts
		if (app->m_keyboardShortcutManager && app->m_keyboardShortcutManager->IsEnabled()) {
			app->m_keyboardShortcutManager->HandleKeyInput(key, scancode, action, mods);
		}
	});

	// Set up framebuffer size callback
	glfwSetFramebufferSizeCallback(m_glfwWindow, [](GLFWwindow* window, int width, int height) {
		CNexusServerApp* app = static_cast<CNexusServerApp*>(glfwGetWindowUserPointer(window));
		if (app) {
			app->m_windowWidth = width;
			app->m_windowHeight = height;
			AppDebug.printf("Window resized to %dx%d\n", width, height);
			app->UpdateImGuiDPIScaling();
		}
	});

	// Set up GLFW content scale callback for DPI changes
	glfwSetWindowContentScaleCallback(m_glfwWindow, [](GLFWwindow* window, float xscale, float yscale) {
		CNexusServerApp* app = static_cast<CNexusServerApp*>(glfwGetWindowUserPointer(window));
		if (app) {
			app->OnDPIScaleChanged(xscale, yscale);
		}
	});
}

// Update RenderImGui() to use UIWindowManager:
void CNexusServerApp::RenderImGui()
{
	DECLARE_FUNC_LOW();

	m_uiWindowManager.RenderAllWindows();

	ImGui::End(); // End the DockSpace window
}
// Add validation and better error handling to RegisterKeyboardShortcuts
void CNexusServerApp::RegisterKeyboardShortcuts()
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


	registerShortcut("ToggleConsole", "Toggle console window", GLFW_KEY_GRAVE_ACCENT, KeyModifier::None,
		[this]() { m_uiWindowManager.ToggleConsole(); });


	AppDebug.printf("Successfully registered %d/%d keyboard shortcuts\n", successCount, totalCount);

	// Enable the shortcut manager
	m_keyboardShortcutManager->SetEnabled(true);
}
void CNexusServerApp::OnDPIScaleChanged(float xscale, float yscale)
{
	m_imguiManager.HandleDPIScaleChange(xscale, yscale);
}

void CNexusServerApp::UpdateImGuiDPIScaling()
{
	if (m_glfwWindow) {
		m_imguiManager.UpdateDPIScaling(m_glfwWindow);
	}
}

