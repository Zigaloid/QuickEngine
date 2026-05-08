/*
 * Copyright 2011-2026 Branimir Karadzic. All rights reserved.
 * License: https://github.com/bkaradzic/bgfx/blob/master/LICENSE
 */

#include <bx/uint32_t.h>
#include "common.h"
#include "bgfx_utils.h"
#include "logo.h"
#include "imgui/imgui.h"
#include "CoreSystem/CoreSystem.h"
#include "CoreSystem/Timer.h"
#include "net/NexusClient.h"
#include "..\shared\sharednexusdefines.h"
#include "..\shared\ProfilerController.h"
#include "CodeCompare.h"
#include "ResourceManager\ResourceManager.h"

namespace
{
	CToolApp theApp;
	class ToolApp: public entry::AppI
	{
	public:
		ToolApp(const char* _name, const char* _description, const char* _url)
			: entry::AppI(_name, _description, _url), m_frameTimer(100)
		{
		}

		void init(int32_t _argc, const char* const* _argv, uint32_t _width, uint32_t _height) override
		{
			DECLARE_FUNC_VLOW();
			InitializeCoreEngine();
			m_profilerController.Init();
			InitializeBgfxView(Args(_argc, _argv), _width, _height);
			imguiCreate();
			theApp.Initialize();
		}

		virtual int shutdown() override
		{
			DECLARE_FUNC_VLOW();
			ShutdownCoreEngine();
			imguiDestroy();
			bgfx::shutdown();
			theApp.Shutdown();
			return 0;
		}

		bool update() override
		{
			DECLARE_FUNC_LOW();
			if (!entry::processEvents(m_width, m_height, m_debug, m_reset, &m_mouseState))
			{
				theApp.Update(m_deltaTime);
				ImguiUpdate();
				BgfxUpdateView();

				Core::CoreSystem::GetResourceManager()->UpdateFinalization();
				auto* scheduler = Core::CoreSystem::GetJobSystemScheduler();
				scheduler->UpdateAllAsync(m_deltaTime);
				scheduler->WaitForCompletion();

				UpdateDeltaTime();

				NEXUS_SEND_MESSAGE(FPS_PIPE, MSG_TYPE_FRAME_TIME, std::to_string(m_deltaTime).c_str());

				return true;
			}

			return false;
		}
	private:
		double UpdateDeltaTime()
		{
			double deltaTime = m_frameTimer.GetElapsed();
			m_frameTimer.Reset();
			if (deltaTime > 1.0 / 10.0) // Clamp delta time to avoid huge jumps (e.g. when debugging or if the app was paused)
			{
				deltaTime = 1.0 / 10.0;
			}
			m_deltaTime = deltaTime;
			return m_deltaTime;
		}

		bool InitializeCoreEngine()
		{
			DECLARE_FUNC_VLOW();
			// Example: in your job system worker threads, game thread, render thread, etc.
			Profiler::ProfileLogger::GetInstance().RegisterThread("Main Thread");
			// Core system initialization
			if (!Core::CoreSystem::Initialize())
				return false;

			return true;
		}
		void ShutdownCoreEngine()
		{
			DECLARE_FUNC_VLOW();
			// Example: in your job system worker threads, game thread, render thread, etc.			
			Core::CoreSystem::Shutdown();
		}
		void BgfxUpdateView()
		{
			DECLARE_FUNC_LOW();
			// Set view 0 default viewport.
			bgfx::setViewRect(0, 0, 0, uint16_t(m_width), uint16_t(m_height));

			// This dummy draw call is here to make sure that view 0 is cleared
			// if no other draw calls are submitted to view 0.
			bgfx::touch(0);

			theApp.Render(m_deltaTime);

			// Advance to next frame. Rendering thread will be kicked to
			// process submitted rendering primitives.
			bgfx::frame();
		}
		void SetupDockspace()
		{
			DECLARE_FUNC_VLOW();
			ImGuiViewport* viewport = ImGui::GetMainViewport();
			ImGui::SetNextWindowPos(viewport->WorkPos);
			ImGui::SetNextWindowSize(viewport->WorkSize);
			ImGui::SetNextWindowViewport(viewport->ID);

			ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
			window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse;
			window_flags |= ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
			window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
			window_flags |= ImGuiWindowFlags_NoBackground;

			ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
			ImGui::Begin("DockSpace", nullptr, window_flags);
			ImGui::PopStyleVar(3);

			ImGuiID dockspace_id = ImGui::GetID("GameAppDockSpace");
			ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);

			// Menu bar within the dockspace window
			if (ImGui::BeginMenuBar())
			{
				/*				if (ImGui::BeginMenu("View"))
								{
									ImGui::MenuItem("Example Dialog", nullptr, &m_showExampleDialog);
									ImGui::MenuItem("Stats", nullptr, &m_showStats);
									ImGui::EndMenu();
								}
				*/
				theApp.ImguiMainMenu();
				ImGui::EndMenuBar();
			}

			ImGui::End();

		}

		void ImguiUpdate()
		{
			DECLARE_FUNC_LOW();
			imguiBeginFrame(m_mouseState.m_mx
				, m_mouseState.m_my
				, (m_mouseState.m_buttons[entry::MouseButton::Left] ? IMGUI_MBUT_LEFT : 0)
				| (m_mouseState.m_buttons[entry::MouseButton::Right] ? IMGUI_MBUT_RIGHT : 0)
				| (m_mouseState.m_buttons[entry::MouseButton::Middle] ? IMGUI_MBUT_MIDDLE : 0)
				, m_mouseState.m_mz
				, uint16_t(m_width)
				, uint16_t(m_height)
			);

			// Setup the full-window dockspace
			SetupDockspace();

			theApp.ImguiUpdate();
			imguiEndFrame();
		}
		void InitializeBgfxView(const Args& args, uint32_t _width, uint32_t _height)
		{
			m_width = _width;
			m_height = _height;
			m_debug = BGFX_DEBUG_TEXT;
			m_reset = BGFX_RESET_VSYNC;

			bgfx::Init init;
			init.type = args.m_type;
			init.vendorId = args.m_pciId;
			init.platformData.nwh = entry::getNativeWindowHandle(entry::kDefaultWindowHandle);
			init.platformData.ndt = entry::getNativeDisplayHandle();
			init.platformData.type = entry::getNativeWindowHandleType();
			init.resolution.width = m_width;
			init.resolution.height = m_height;
			init.resolution.reset = m_reset;
			bgfx::init(init);

			// Enable debug text.
			bgfx::setDebug(m_debug);

			// Set view 0 clear state.
			bgfx::setViewClear(0
				, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH
				, 0x303030ff
				, 1.0f
				, 0
			);
		}

		entry::MouseState m_mouseState;

		uint32_t m_width;
		uint32_t m_height;
		uint32_t m_debug;
		uint32_t m_reset;
		double	m_deltaTime = 1 / 10;

		// Frame timing
		Core::Timer m_frameTimer;
		ProfilerController m_profilerController;
		// Dockable window visibility states
		bool m_showExampleDialog = true;
		bool m_showStats = false;
	};
	void TestDebugText()
	{
		// Use debug font to print information about this example.
		bgfx::dbgTextClear();

		const bgfx::Stats* stats = bgfx::getStats();

		bgfx::dbgTextImage(
			bx::max<uint16_t>(uint16_t(stats->textWidth / 2), 20) - 20
			, bx::max<uint16_t>(uint16_t(stats->textHeight / 2), 6) - 6
			, 40
			, 12
			, s_logo
			, 160
		);

		bgfx::dbgTextPrintf(0, 1, 0x0f, "Color can be changed with ANSI \x1b[9;me\x1b[10;ms\x1b[11;mc\x1b[12;ma\x1b[13;mp\x1b[14;me\x1b[0m code too.");

		bgfx::dbgTextPrintf(80, 1, 0x0f, "\x1b[;0m    \x1b[;1m    \x1b[; 2m    \x1b[; 3m    \x1b[; 4m    \x1b[; 5m    \x1b[; 6m    \x1b[; 7m    \x1b[0m");
		bgfx::dbgTextPrintf(80, 2, 0x0f, "\x1b[;8m    \x1b[;9m    \x1b[;10m    \x1b[;11m    \x1b[;12m    \x1b[;13m    \x1b[;14m    \x1b[;15m    \x1b[0m");

		bgfx::dbgTextPrintf(0, 2, 0x0f, "Backbuffer %dW x %dH in pixels, debug text %dW x %dH in characters."
			, stats->width
			, stats->height
			, stats->textWidth
			, stats->textHeight
		);
	}
} // namespace

ENTRY_IMPLEMENT_MAIN(
	ToolApp
	, "FbxToBgfx"
	, "FbxToBgfx Mesh Converter."
	, "Neil Hill"
);
