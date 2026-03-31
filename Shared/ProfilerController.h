#pragma once
#include "CoreSystem/CoreSystem.h"
#include "net/NexusClient.h"

class ProfilerController
{
public:
	void HandleProfilerMessage(const std::string& messageBody)
	{
		DECLARE_FUNC_VLOW();
		// Simple command parsing - expects "command arg1 arg2 ..."
		std::istringstream iss(messageBody);
		std::string command;
		iss >> command;

		if (command == "Start") {
			StartProfiling();
		}
		else if (command == "Stop") {
			StopProfiling(true);
		}
		else if (command == "Clear") {
			Clear();
		}	
		else
		{
			ProfilerDebug.printf("Unknown profiler command received: %s\n", command.c_str());
		}
	}
	void Init()
	{
		DECLARE_FUNC_VLOW();
		NEXUS_SUBSCRIBE_CALLBACK(PROFILER_PIPE, "QuickScope", HandleProfilerMessage);
		auto& profiler = Profiler::ProfileLogger::GetInstance();
	}
	void StartProfiling()
	{
		DECLARE_FUNC_LOW();

		auto& profiler = Profiler::ProfileLogger::GetInstance();
		if (!profiler.IsEnabled()) {
			profiler.Clear();
			profiler.SetEnabled(true);
			ProfilerDebug.printf("Profiler started\n");
		}
	}

	void StopProfiling(bool sendAsBinary)
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
			}
			else {
				// Send events as a JSON text message
				std::string profilerEvents = profiler.SerializeRecentEvents();
				NEXUS_SEND_MESSAGE(PROFILER_PACKET_PIPE, MSG_TYPE_PROFILER_DATA, profilerEvents);
			}
		}
	}

	bool IsEnabled() const
	{
		return Profiler::ProfileLogger::GetInstance().IsEnabled();
	}

	void Clear()
	{
		DECLARE_FUNC_LOW();

		auto& profiler = Profiler::ProfileLogger::GetInstance();
		profiler.Clear();
		ProfilerDebug.printf("Profiler data cleared\n");
	}
};