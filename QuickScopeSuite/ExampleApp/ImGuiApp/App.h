#pragma once

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include "Profiler/Profiler.h"
#include "ImGuiVisualizers/CommandConsole.h"
#include "../../Rendering/Window.h"

#if defined(_WIN32)
#include <GL/glew.h>
#endif

#include <GLFW/glfw3.h>
#include <stdio.h>

class App
{
public:
	App(const char* title, int width, int height)
		: m_Title(title)
		, m_Width(width)
		, m_Height(height)
		, m_Window(nullptr)
		, m_RenderWindow(nullptr)
		, m_ClearColor(0.45f, 0.55f, 0.60f, 1.00f)
		, m_ShowDemoWindow(true)
		, m_ShowAnotherWindow(false)
	{
	}

	~App()
	{
		Shutdown();
	}

	bool Init()
	{
		if (!InitWindow())
			return false;

		if (!InitImGui())
			return false;

		OnInit();
		return true;
	}

	void Run()
	{
		while (!m_RenderWindow->shouldClose())
		{
			PROFILE_FRAME();
			DECLARE_SCOPE_LOW("MainLoop");
			m_RenderWindow->pollEvents();
			BeginFrame();
			OnUpdate();
			EndFrame();
		}
	}

	void Shutdown()
	{
		OnShutdown();
		ShutdownImGui();
		ShutdownWindow();
	}

protected:
	// Override these in derived classes
	virtual void OnInit() {}
	virtual void OnUpdate() {}
	virtual void OnShutdown() {}

	// Helper methods for derived classes
	ImGuiID CreateDockSpace()
	{
		ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(viewport->WorkPos);
		ImGui::SetNextWindowSize(viewport->WorkSize);
		ImGui::SetNextWindowViewport(viewport->ID);

		ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
		window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse;
		window_flags |= ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
		window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

		ImGui::Begin("DockSpace", nullptr, window_flags);
		ImGui::PopStyleVar(3);

		ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
		ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);

		return dockspace_id;
	}

	void EndDockSpace()
	{
		ImGui::End();
	}

	GLFWwindow* GetWindow() { return m_Window; }
	Window* GetRenderWindow() { return m_RenderWindow; }
	ImVec4& GetClearColor() { return m_ClearColor; }

private:
	bool InitWindow()
	{
		// Create Window configuration
		Window::WindowConfig config;
		config.width = m_Width;
		config.height = m_Height;
		config.title = m_Title;
		config.resizable = true;
		config.vsync = false;

		m_RenderWindow = new Window(config);
		if (!m_RenderWindow->initialize()) {
			delete m_RenderWindow;
			m_RenderWindow = nullptr;
			return false;
		}

		m_Window = m_RenderWindow->getGLFWWindow();
		return true;
	}

	bool InitImGui()
	{
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO();
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
		io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

		ImGui::StyleColorsDark();

		ImGuiStyle& style = ImGui::GetStyle();
		if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			style.WindowRounding = 0.0f;
			style.Colors[ImGuiCol_WindowBg].w = 1.0f;
		}

		ImGui_ImplGlfw_InitForOpenGL(m_Window, true);
		if (!ImGui_ImplOpenGL3_Init("#version 330"))
		{
			fprintf(stderr, "Failed to initialize ImGui OpenGL3 backend\n");
			return false;
		}

		return true;
	}

	void BeginFrame()
	{
		DECLARE_FUNC_LOW();
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();
	}

	void EndFrame()
	{
		DECLARE_FUNC_LOW();
		{
			DECLARE_SCOPE_LOW("ImGuiRender");
			ImGui::Render();
		}

		{
			DECLARE_SCOPE_LOW("OpenGLRender");
			int display_w, display_h;
			glfwGetFramebufferSize(m_Window, &display_w, &display_h);
			glViewport(0, 0, display_w, display_h);
			glClearColor(m_ClearColor.x * m_ClearColor.w, m_ClearColor.y * m_ClearColor.w,
				m_ClearColor.z * m_ClearColor.w, m_ClearColor.w);
			glClear(GL_COLOR_BUFFER_BIT);

			ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

			ImGuiIO& io = ImGui::GetIO();
			if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
			{
				GLFWwindow* backup_current_context = glfwGetCurrentContext();
				ImGui::UpdatePlatformWindows();
				ImGui::RenderPlatformWindowsDefault();
				glfwMakeContextCurrent(backup_current_context);
			}
		}
		m_RenderWindow->swapBuffers();
	}

	void ShutdownImGui()
	{
		if (m_Window)
		{
			ImGui_ImplOpenGL3_Shutdown();
			ImGui_ImplGlfw_Shutdown();
			ImGui::DestroyContext();
		}
	}

	void ShutdownWindow()
	{
		if (m_RenderWindow)
		{
			delete m_RenderWindow;
			m_RenderWindow = nullptr;
			m_Window = nullptr;
		}
	}

protected:
	const char* m_Title;
	int m_Width;
	int m_Height;
	GLFWwindow* m_Window;        // Points to the Window class's GLFW window
	Window* m_RenderWindow;      // The actual Window class instance
	ImVec4 m_ClearColor;

	// Example state
	bool m_ShowDemoWindow;
	bool m_ShowAnotherWindow;
};