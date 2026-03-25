#pragma once

#include <memory>
#include <iostream>
#include "CoreSystem/FunctionQueue.h"
#include "viewport.h"
#include "DebugChannel/DebugChannel.h"


extern DebugChannels::CDebugChannel RenderDebug;

namespace Rendering {
    class RenderPrimitives;
}

namespace Rendering {

    // Central system manager that initializes and manages core engine systems
    class RenderingSystem {
    private:
        // Core system components        
        static std::unique_ptr<Rendering::RenderPrimitives> s_RenderPrimitives;
        static bool s_initialized;
        static std::unique_ptr<Core::FunctionQueue> s_RenderQueue;
		static Viewport* s_currentlyRenderingViewport;
        static Viewport* m_activeViewport;
        // Private constructor - singleton pattern
        RenderingSystem() = delete;
        ~RenderingSystem() = delete;

    public:
        // Core system lifecycle
        static bool Initialize();
        static void Shutdown();
        static bool IsInitialized() { return s_initialized; }
		static Viewport *GetCurrentlyRenderingViewport() { return s_currentlyRenderingViewport; }
        static void SetCurrentlyRenderingViewport(Viewport* viewport) { s_currentlyRenderingViewport = viewport; }

        static Viewport* GetActiveViewport() { return m_activeViewport; }
		static void SetActiveViewport(Viewport* viewport) { m_activeViewport = viewport; }

        // Static accessors for core systems
        static Rendering::RenderPrimitives* GetRenderPrimitives() {
            return s_RenderPrimitives.get(); 
        }
		static Core::FunctionQueue* GetRenderQueue() {
			return s_RenderQueue.get();
		}

        // Utility methods
        static void PrintSystemStatus();
    };
} // namespace Core