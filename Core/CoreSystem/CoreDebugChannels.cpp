
#include "DebugChannel/DebugChannel.h"

// Debug channels for core systems.
DebugChannels::CDebugChannel AppDebug("AppDebug");			// Created here for convenience but intended to be used by the application.
DebugChannels::CDebugChannel ReflectionDebug("Reflection");	
DebugChannels::CDebugChannel CoreDebug("Core");
DebugChannels::CDebugChannel JobSystemDebug("JobSystem");
DebugChannels::CDebugChannel FileSystemDebug("FileSystem");
DebugChannels::CDebugChannel ProfilerDebug("Profiler");
