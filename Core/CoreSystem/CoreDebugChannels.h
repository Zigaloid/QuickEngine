#pragma once

#include "DebugChannel/DebugChannel.h"
// Debug channels for core systems.
extern DebugChannels::CDebugChannel AppDebug;			// Created here for convenience but intended to be used by the application.
extern DebugChannels::CDebugChannel ReflectionDebug;
extern DebugChannels::CDebugChannel CoreDebug;
extern DebugChannels::CDebugChannel JobSystemDebug;
extern DebugChannels::CDebugChannel FileSystemDebug;
extern DebugChannels::CDebugChannel ProfilerDebug;
extern DebugChannels::CDebugChannel ResourceManagerDebug;