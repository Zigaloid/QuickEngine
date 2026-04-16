#include "DebugChannel.h"

#include "CoreSystem/CoreSystem.h"
#include "net/NexusClient.h"

using namespace DebugChannels;
using std::string;

// ── CDebugChannelManager ──────────────────────────────────────────────────────

CDebugChannelManager& CDebugChannelManager::GetInstance()
{
    static CDebugChannelManager theManager;
    return theManager;
}

// ── CDebugChannel ─────────────────────────────────────────────────────────────

CDebugChannel::CDebugChannel(const string& name) : m_name(name)
{
    CDebugChannelManager::GetInstance().AddChannel(this);
    OpenLog();
}

CDebugChannel::~CDebugChannel()
{
    CDebugChannelManager::GetInstance().RemoveChannel(this);
    CloseLog();
}

string CDebugChannel::FormatString(const char* fmt, va_list vl)
{
    static const int bufferSize = 1024;
    static char buffer[bufferSize];
    string result;
    int nsize = vsnprintf(buffer, bufferSize, fmt, vl);
    if (bufferSize <= nsize)
    {
        nsize++;
        char* dynBuffer = new char[nsize];
        nsize = vsnprintf(dynBuffer, nsize, fmt, vl);
        result = dynBuffer;
        delete[] dynBuffer;
    }
    else
    {
        result = buffer;
    }
    return result;
}

void CDebugChannel::print(const string& message)
{
    if (m_outputFlags & OF_StdOut)
    {
        if (Core::CoreSystem::GetLog())
            Core::CoreSystem::GetLog()->Info("[%s] %s\n", m_name.c_str(), message.c_str());
        else
            std::cout << message;
    }

    if (m_outputFlags & OF_Log && m_outFile.is_open())
        m_outFile << message;

    if (m_outputFlags & OF_Nexus && Core::CoreSystem::GetNexusClient() && Core::CoreSystem::GetNexusClient()->IsConnected())
        Core::CoreSystem::GetNexusClient()->SendPipeMessage(m_name, "Debug", message);
}

void CDebugChannel::printf(const char* format, ...)
{
    va_list vl;
    va_start(vl, format);
    string formatted = FormatString(format, vl);
    this->print(formatted);
    va_end(vl);
}

void CDebugChannel::warning(const char* format, ...)
{
    va_list vl;
    va_start(vl, format);
    string formatted = FormatString(format, vl);
    formatted = "[WARNING] " + formatted;
    this->print(formatted);
    va_end(vl);
}

bool CDebugChannel::Assert(bool condition, const char* format, ...)
{
    va_list vl;
    va_start(vl, format);
    string formatted = FormatString(format, vl);
    va_end(vl);
    if (condition == false)
    {
        print(formatted);
        assert(condition);
    }
    return condition;
}
