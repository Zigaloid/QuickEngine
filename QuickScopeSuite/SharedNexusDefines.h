#pragma once
// Pipe names for shared memory communication.
static const std::string PROFILER_PACKET_PIPE = "ProfilerPacket";
static const std::string STATUS_PIPE = "Status";
static const std::string PERFORMANCE_PIPE = "Performance";
static const std::string FPS_PIPE = "Fps";
static const std::string CONSOLE_PIPE = "Console";
static const std::string PROFILER_PIPE = "Profiler";
static const std::string TELEMETRY_PIPE = "Telemetry";

// Message types.
static const std::string MSG_TYPE_PROFILER_DATA = "ProfilerData";
static const std::string MSG_TYPE_CAMERA_POS = "CameraPos";
static const std::string MSG_TYPE_FRAME_TIME = "FrameTime";
static const std::string MSG_TYPE_PROFILE_CONTROL = "ProfilerControl";

// ------------------------------------------------------------------------ -
// Telemetry / HeatMap helpers
// ---------------------------------------------------------------------------

#include "Math/Vector3f.h"
#include "coresystem/coresystem.h"
#include <string>
#include <cstdio>

// Format a heatmap telemetry message body: "pos=x,y,z value=v"
// Use with NEXUS_SEND_MESSAGE(TELEMETRY_PIPE, seriesName, body).
inline std::string FormatHeatMapMessage(const Vector3f & position, double value)
{
	char buf[128];
	std::snprintf(buf, sizeof(buf), "pos=%.4f,%.4f,%.4f value=%.6f",
		position.x, position.y, position.z, value);
	return std::string(buf);
}

// Convenience: format and send a heatmap telemetry sample in one call.
// Requires CoreSystem/NexusClient to be available (uses NEXUS_SEND_MESSAGE).
inline void SendHeatMapTelemetry(const std::string& seriesName, const Vector3f& position, double value)
{
	NEXUS_SEND_MESSAGE(TELEMETRY_PIPE, seriesName, FormatHeatMapMessage(position, value));
}

#define SEND_TELEMETRY(_type, _pos, _value, _interval) \
	{ \
		static Timer timer(_interval); \
		if (timer.HasExpired()) { \
			timer.Reset(); \
			Vector3f camPos = _pos;	\
			SendHeatMapTelemetry(_type, _pos, _value); \
		} \
	}