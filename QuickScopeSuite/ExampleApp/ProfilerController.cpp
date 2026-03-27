#include "ProfilerController.h"
#include "CoreSystem/CoreSystem.h"
#include "Net/NexusClient.h"
#include "CoreSystem/FunctionCallManager.h"
#include "..\SharedNexusDefines.h"

#include "DebugChannel/DebugChannel.h"
#include "imgui.h"
#include <algorithm>
#include <sstream>

using namespace Core;

extern DebugChannels::CDebugChannel AppDebug;

ProfilerController::ProfilerController()
    : m_showFlameGraph(false)
    , m_initialized(false)
{
}
void ProfilerController::HandleProfilerMessage(const std::string& messageBody)
{        
    DECLARE_FUNC_VLOW();    
    // Simple command parsing - expects "command arg1 arg2 ..."
    std::istringstream iss(messageBody);
    std::string command;
    iss >> command;
    
    if (command == "Start") {
        StartProfiling();
    } else if (command == "Stop") {
        StopProfiling(true);
    } else if (command == "Clear") {
        Clear();   
    } else {
        AppDebug.printf("Unknown profiler command received: %s\n", command.c_str());
    }
}

void ProfilerController::Initialize()
{
    DECLARE_FUNC_VLOW();
    
    if (m_initialized) return;
    
    NEXUS_SUBSCRIBE_CALLBACK(PROFILER_PIPE, "QuickScope", HandleProfilerMessage);

    // Register profiler functions with FunctionCallManager
    auto* functionManager = Core::CoreSystem::GetFunctionManager();
    if (functionManager) {
        functionManager->RegisterFunction<void>("startProfiling", std::function<void()>([this]() {
            StartProfiling();
        }));
        
        functionManager->RegisterFunction<void>("stopProfiling", std::function<void()>([this]() {
            StopProfiling();
        }));
        
        functionManager->RegisterFunction<void>("clearProfiler", std::function<void()>([this]() {
            Clear();
        }));
    }
    
    m_initialized = true;
    AppDebug.printf("ProfilerController initialized\n");
}

void ProfilerController::Shutdown()
{
    DECLARE_FUNC_VLOW();
    
    if (!m_initialized) return;
    
    // Flush any remaining profiler data
    auto& profiler = Profiler::ProfileLogger::GetInstance();
    if (profiler.IsEnabled()) {
        profiler.SetEnabled(false);
        profiler.FlushToFile();
    }
        
    m_initialized = false;
    AppDebug.printf("ProfilerController shutdown\n");
}

void ProfilerController::StartProfiling()
{
    DECLARE_FUNC_LOW();
    
    auto& profiler = Profiler::ProfileLogger::GetInstance();
    if (!profiler.IsEnabled()) {
        profiler.Clear();
        profiler.SetEnabled(true);
        AppDebug.printf("Profiler started\n");
    }
}

void ProfilerController::StopProfiling(bool sendAsBinary)
{
    DECLARE_FUNC_LOW();
    
    auto& profiler = Profiler::ProfileLogger::GetInstance();
    if (profiler.IsEnabled())
    {
        if (sendAsBinary) {
            // Send events as a compact binary message
            std::vector<Profiler::ProfileEvent> events = profiler.CopyEvents();
            if (!events.empty()) {
                std::vector<uint8_t> profilerEvents = profiler.SerializeRecentEventsBinary();                
                NEXUS_SEND_BINARY_MESSAGE(PROFILER_PACKET_PIPE, profilerEvents.data(), profilerEvents.size());
            }
        } else {
            // Send events as a JSON text message
            std::string profilerEvents = profiler.SerializeRecentEvents();
            NEXUS_SEND_MESSAGE(PROFILER_PACKET_PIPE,MSG_TYPE_PROFILER_DATA, profilerEvents);
        }
    }
}

bool ProfilerController::IsEnabled() const
{
    return Profiler::ProfileLogger::GetInstance().IsEnabled();
}

void ProfilerController::Clear()
{
    DECLARE_FUNC_LOW();
    
    auto& profiler = Profiler::ProfileLogger::GetInstance();
    profiler.Clear();
    AppDebug.printf("Profiler data cleared\n");
}

