#pragma once

#include "IBerkeleySocket.h"
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <string>
#include <vector>
#include <cstdint>
#include <mutex>

#pragma comment(lib, "ws2_32.lib")

/** @brief Windows Winsock2 implementation of IBerkeleySocket. */
class WinBerkeleySocket : public IBerkeleySocket
{
public:
    WinBerkeleySocket();
    ~WinBerkeleySocket() override;

    bool Open(ESocketDomain domain, ESocketType type, ESocketProtocol protocol) override;
    void Close() override;
    bool Bind(const std::string& address, uint16_t port) override;
    bool Listen(int backlog = 5) override;
    WinBerkeleySocket* Accept(std::string* outAddress = nullptr, uint16_t* outPort = nullptr) override;
    bool Connect(const std::string& address, uint16_t port) override;
    size_t Send(const void* data, size_t size) override;
    size_t Receive(void* buffer, size_t size) override;
    size_t SendTo(const void* data, size_t size, const std::string& address, uint16_t port) override;
    size_t ReceiveFrom(void* buffer, size_t size, std::string* outAddress, uint16_t* outPort) override;
    bool SetOption(ESocketOptionLevel level, ESocketOption optionName, const void* optionValue, size_t optionLen) override;
    bool GetOption(ESocketOptionLevel level, ESocketOption optionName, void* optionValue, size_t* optionLen) override;
    bool IsOpen() const override;

private:
    SOCKET m_socket  = INVALID_SOCKET;
    bool   m_isOpen  = false;

    static std::once_flag s_wsaOnceFlag;
    static void EnsureWSAStartup();

    // Map abstract enums to platform-specific values
    static int ToNativeDomain(ESocketDomain domain);
    static int ToNativeType(ESocketType type);
    static int ToNativeProtocol(ESocketProtocol protocol);
    static int ToNativeOptionLevel(ESocketOptionLevel level);
    static int ToNativeOption(ESocketOption option);
};
