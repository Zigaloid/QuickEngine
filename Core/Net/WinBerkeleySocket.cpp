#include "WinBerkeleySocket.h"
#include <stdexcept>
#include <cstring>

std::once_flag WinBerkeleySocket::s_wsaOnceFlag;

void WinBerkeleySocket::EnsureWSAStartup() {
    std::call_once(s_wsaOnceFlag, []() {
        WSADATA wsaData;
        int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (result != 0) {
            throw std::runtime_error("WSAStartup failed");
        }
    });
}

// ---------------------------------------------------------------------------
// Enum-to-native mappings
// ---------------------------------------------------------------------------

int WinBerkeleySocket::ToNativeDomain(ESocketDomain domain) {
    switch (domain) {
    case ESocketDomain::IPv4: return AF_INET;
    case ESocketDomain::IPv6: return AF_INET6;
    default:                  return AF_INET;
    }
}

int WinBerkeleySocket::ToNativeType(ESocketType type) {
    switch (type) {
    case ESocketType::Stream:   return SOCK_STREAM;
    case ESocketType::Datagram: return SOCK_DGRAM;
    default:                    return SOCK_STREAM;
    }
}

int WinBerkeleySocket::ToNativeProtocol(ESocketProtocol protocol) {
    switch (protocol) {
    case ESocketProtocol::TCP: return IPPROTO_TCP;
    case ESocketProtocol::UDP: return IPPROTO_UDP;
    default:                   return IPPROTO_TCP;
    }
}

int WinBerkeleySocket::ToNativeOptionLevel(ESocketOptionLevel level) {
    switch (level) {
    case ESocketOptionLevel::Socket: return SOL_SOCKET;
    case ESocketOptionLevel::TCP:    return IPPROTO_TCP;
    default:                         return SOL_SOCKET;
    }
}

int WinBerkeleySocket::ToNativeOption(ESocketOption option) {
    switch (option) {
    case ESocketOption::ReuseAddress: return SO_REUSEADDR;
    case ESocketOption::KeepAlive:    return SO_KEEPALIVE;
    case ESocketOption::NoDelay:      return TCP_NODELAY;
    default:                          return 0;
    }
}

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------

WinBerkeleySocket::WinBerkeleySocket()
    : m_socket(INVALID_SOCKET), m_isOpen(false) {
    EnsureWSAStartup();
}

WinBerkeleySocket::~WinBerkeleySocket() {
    Close();
}

bool WinBerkeleySocket::Open(ESocketDomain domain, ESocketType type, ESocketProtocol protocol) {
    if (m_isOpen) Close();
    m_socket = ::socket(ToNativeDomain(domain), ToNativeType(type), ToNativeProtocol(protocol));
    m_isOpen = (m_socket != INVALID_SOCKET);
    return m_isOpen;
}

void WinBerkeleySocket::Close() {
    if (m_isOpen && m_socket != INVALID_SOCKET) {
        ::closesocket(m_socket);
        m_socket = INVALID_SOCKET;
        m_isOpen = false;
    }
}

bool WinBerkeleySocket::Bind(const std::string& address, uint16_t port) {
    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (address.empty() || address == "0.0.0.0") {
        addr.sin_addr.s_addr = INADDR_ANY;
    } else {
        if (inet_pton(AF_INET, address.c_str(), &addr.sin_addr) != 1) {
            return false;
        }
    }
    return ::bind(m_socket, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0;
}

bool WinBerkeleySocket::Listen(int backlog) {
    return ::listen(m_socket, backlog) == 0;
}

WinBerkeleySocket* WinBerkeleySocket::Accept(std::string* outAddress, uint16_t* outPort) {
    sockaddr_in clientAddr = {};
    int addrLen = sizeof(clientAddr);
    SOCKET clientSocket = ::accept(m_socket, reinterpret_cast<sockaddr*>(&clientAddr), &addrLen);
    if (clientSocket == INVALID_SOCKET) return nullptr;

    if (outAddress) {
        char buf[INET_ADDRSTRLEN] = {};
        inet_ntop(AF_INET, &clientAddr.sin_addr, buf, sizeof(buf));
        *outAddress = buf;
    }
    if (outPort) {
        *outPort = ntohs(clientAddr.sin_port);
    }

    auto* newSocket = new WinBerkeleySocket();
    newSocket->m_socket = clientSocket;
    newSocket->m_isOpen = true;
    return newSocket;
}

bool WinBerkeleySocket::Connect(const std::string& address, uint16_t port) {
    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (inet_pton(AF_INET, address.c_str(), &addr.sin_addr) != 1) {
        return false;
    }
    return ::connect(m_socket, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0;
}

size_t WinBerkeleySocket::Send(const void* data, size_t size) {
    int sent = ::send(m_socket, static_cast<const char*>(data), static_cast<int>(size), 0);
    return sent == SOCKET_ERROR ? -1 : sent;
}

size_t WinBerkeleySocket::Receive(void* buffer, size_t size) {
    int recvd = ::recv(m_socket, static_cast<char*>(buffer), static_cast<int>(size), 0);
    return recvd == SOCKET_ERROR ? -1 : recvd;
}

size_t WinBerkeleySocket::SendTo(const void* data, size_t size, const std::string& address, uint16_t port) {
    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (inet_pton(AF_INET, address.c_str(), &addr.sin_addr) != 1) {
        return -1;
    }
    int sent = ::sendto(m_socket, static_cast<const char*>(data), static_cast<int>(size), 0,
                        reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    return sent == SOCKET_ERROR ? -1 : sent;
}

size_t WinBerkeleySocket::ReceiveFrom(void* buffer, size_t size, std::string* outAddress, uint16_t* outPort) {
    sockaddr_in fromAddr = {};
    int addrLen = sizeof(fromAddr);
    int recvd = ::recvfrom(m_socket, static_cast<char*>(buffer), static_cast<int>(size), 0,
                           reinterpret_cast<sockaddr*>(&fromAddr), &addrLen);
    if (recvd == SOCKET_ERROR) return -1;
    if (outAddress) {
        char buf[INET_ADDRSTRLEN] = {};
        inet_ntop(AF_INET, &fromAddr.sin_addr, buf, sizeof(buf));
        *outAddress = buf;
    }
    if (outPort) {
        *outPort = ntohs(fromAddr.sin_port);
    }
    return recvd;
}

bool WinBerkeleySocket::SetOption(ESocketOptionLevel level, ESocketOption optionName, const void* optionValue, size_t optionLen) {
    return ::setsockopt(m_socket, ToNativeOptionLevel(level), ToNativeOption(optionName),
                        static_cast<const char*>(optionValue), static_cast<int>(optionLen)) == 0;
}

bool WinBerkeleySocket::GetOption(ESocketOptionLevel level, ESocketOption optionName, void* optionValue, size_t* optionLen) {
    int len = static_cast<int>(*optionLen);
    int result = ::getsockopt(m_socket, ToNativeOptionLevel(level), ToNativeOption(optionName),
                              static_cast<char*>(optionValue), &len);
    if (result == 0) {
        *optionLen = static_cast<size_t>(len);
        return true;
    }
    return false;
}

bool WinBerkeleySocket::IsOpen() const {
    return m_isOpen && m_socket != INVALID_SOCKET;
}