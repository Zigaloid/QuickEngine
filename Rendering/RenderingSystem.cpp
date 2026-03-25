#include <iostream>
#include "RenderPrimitives.h"
#include "RenderingSystem.h"

DebugChannels::CDebugChannel RenderDebug("RenderDebug");

namespace Rendering {
	// Static member definitions - FIXED: Properly qualify the static member
	std::unique_ptr<Rendering::RenderPrimitives> RenderingSystem::s_RenderPrimitives = nullptr;
	std::unique_ptr<Core::FunctionQueue> RenderingSystem::s_RenderQueue = nullptr;
	Viewport * RenderingSystem::s_currentlyRenderingViewport = nullptr;
	Viewport* RenderingSystem::m_activeViewport = nullptr;

	bool RenderingSystem::s_initialized = false;
	bool RenderingSystem::Initialize() 
	{
		if (s_initialized) {
			RenderDebug.printf("Rendering system: Already initialized\n");
			return true;
		}

		RenderDebug.printf("Rendering: Initializing render systems...\n");

		try {
			s_RenderPrimitives = std::make_unique<Rendering::RenderPrimitives>();
			s_RenderPrimitives->Initialize();
			s_RenderQueue = std::make_unique<Core::FunctionQueue>();
		}
		catch (const std::exception& e) {
			RenderDebug.warning("Rendering: Exception during initialization: %s\n", e.what());
			Shutdown(); // Clean up any partially initialized systems
			return false;
		}
		return s_initialized = true;
	}

	void RenderingSystem::Shutdown() {
		if (!s_initialized) {
			return;
		}

		RenderDebug.printf("RenderSystem: Shutting down render engine systems...\n");

		if (s_RenderPrimitives) {
			RenderDebug.printf("RenderSystem: Shutting down JobSystemScheduler...\n");
			s_RenderPrimitives->Shutdown();
			s_RenderPrimitives.reset();
		}

		if (s_RenderQueue) {
			s_RenderQueue.reset();
		}

		s_initialized = false;
		RenderDebug.printf("RenderSystem: All render systems shutdown complete\n");
	}

	void RenderingSystem::PrintSystemStatus() {
		RenderDebug.printf("\n=== Render System Status ===\n");
		RenderDebug.printf("Initialized: %s\n", (s_initialized ? "Yes" : "No"));

		if (!s_initialized) {
			RenderDebug.printf("========================\n\n");
			return;
		}

		RenderDebug.printf("=========================\n\n");
	}

} // namespace renderingSystem