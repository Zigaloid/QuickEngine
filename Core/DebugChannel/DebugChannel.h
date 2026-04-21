#pragma once

#include <string>
#include <memory>
#include <map>
#include <vector>
#include <iostream>
#include <fstream>
#include <assert.h>
#include <cstdarg>

namespace DebugChannels {

	// ── Output flag constants ─────────────────────────────────────────────────────

	enum
	{
		OF_Null = 0x0000,
		OF_StdOut = 0x0001,
		OF_Log = 0x0002,
		OF_Nexus = 0x0004,
	};

	// ── CDebugChannel ─────────────────────────────────────────────────────────────

	/** @brief A named output channel that routes formatted messages to stdout, a log file, and/or Nexus. */
	class CDebugChannel {
	public:
		explicit CDebugChannel(const std::string& name);
		~CDebugChannel();

		void OpenLog()
		{
			std::string fileName = m_name + ".log";
			m_outFile.open(fileName, std::ios::binary);
		}

		void CloseLog()
		{
			if (m_outFile.is_open())
				m_outFile.close();
		}

		void printf(const char* format, ...);
		void print(const std::string& message);
		void warning(const char* format, ...);

		void SetOutputFlags(int flags)
		{
			m_outputFlags = flags;
		}

		bool Assert(bool condition, const char* message, ...);

		/** @brief Return the channel name. */
		const std::string& GetName() const { return m_name; }
		bool isEnabled() const { return m_enabled; }
		void SetEnabled(bool enabled) { m_enabled = enabled; }
	private:
		std::string FormatString(const char* fmt, va_list vl);
		bool m_enabled = true;
		const std::string m_name;
		int               m_outputFlags = OF_StdOut | OF_Log | OF_Nexus;
		std::ofstream     m_outFile;
	};

	// ── CDebugChannelManager ──────────────────────────────────────────────────────

	/** @brief Singleton registry of all active CDebugChannel instances. */
	class CDebugChannelManager {
	public:
		static CDebugChannelManager& GetInstance();

		void AddChannel(CDebugChannel* channel)
		{
			m_channels[channel->GetName()] = channel;
		}

		void RemoveChannel(CDebugChannel* channel)
		{
			m_channels.erase(channel->GetName());
		}

	private:
		std::map<const std::string, CDebugChannel*> m_channels;
	};

} // namespace DebugChannels
