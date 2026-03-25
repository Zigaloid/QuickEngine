#pragma once
#include <string>
#include <memory>
#include <map>
#include <vector>
#include <iostream>
#include <fstream>
#include <assert.h>
#include <cstdarg>

namespace DebugChannels
{

enum
{
	OF_Null = 0x0000,
	OF_StdOut = 0x0001,
	OF_Log = 0x0002,
	OF_Nexus = 0x0004,
};

class CDebugChannel
{
public:
	CDebugChannel(const std::string& name);
	~CDebugChannel();

	void OpenLog()
	{
		std::string fileName = m_name + ".log";
		m_outFile.open(fileName, std::ios::binary);
	};	
	void CloseLog()
	{
		if( m_outFile.is_open())
			m_outFile.close();
	};			
	void printf(const char* format, ...);
	void print(const std::string message);
	void warning(const char* format, ...);
	void SetOutputFlags(int flags)
	{
		m_outputflags = flags;
	}
	bool Assert(bool condition, const char *message, ...);
	const std::string& GetName() const { return m_name; }
private:	
	std::string FormatString(const char* fmt, va_list vl);
	const std::string m_name;
	int m_outputflags = OF_StdOut | OF_Log | OF_Nexus;
	std::ofstream m_outFile;
};

class CDebugChannelManager
{
public:
	static CDebugChannelManager& GetInstance();
	void AddChannel(CDebugChannel *channel)
	{
		m_channels[channel->GetName()] = channel;
	}
	void RemoveChannel(CDebugChannel* channel)
	{		
		m_channels.erase(channel->GetName());
	}

private:
	std::map<const std::string, CDebugChannel *> m_channels;
};

} // namespace DebugChannels