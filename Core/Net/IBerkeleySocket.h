#pragma once

#include <cstddef>
#include <string>
#include <vector>
#include <cstdint>

// ---------------------------------------------------------------------------
// Platform-independent socket constants
// ---------------------------------------------------------------------------

// Address families
enum class ESocketDomain : int {
    IPv4 = 0,  // AF_INET
    IPv6 = 1,  // AF_INET6
};

// Socket types
enum class ESocketType : int {
    Stream   = 0,  // SOCK_STREAM
    Datagram = 1,  // SOCK_DGRAM
};

// Protocols
enum class ESocketProtocol : int {
    TCP = 0,  // IPPROTO_TCP
    UDP = 1,  // IPPROTO_UDP
};

// Option levels
enum class ESocketOptionLevel : int {
    Socket = 0,  // SOL_SOCKET
    TCP    = 1,  // IPPROTO_TCP
};

// Socket options
enum class ESocketOption : int {
    ReuseAddress = 0,  // SO_REUSEADDR
    KeepAlive    = 1,  // SO_KEEPALIVE
    NoDelay      = 2,  // TCP_NODELAY
};

// Generic interface for a Berkeley socket
class IBerkeleySocket {
public:
    virtual ~IBerkeleySocket() = default;

    // Open the socket (optionally specify address family, type, protocol)
    virtual bool Open(ESocketDomain domain, ESocketType type, ESocketProtocol protocol) = 0;

    // Close the socket
    virtual void Close() = 0;

    // Bind the socket to an address and port
    virtual bool Bind(const std::string& address, uint16_t port) = 0;

    // Listen for incoming connections (for stream sockets)
    virtual bool Listen(int backlog = 5) = 0;

    // Accept an incoming connection (returns a new socket object)
    virtual IBerkeleySocket* Accept(std::string* outAddress = nullptr, uint16_t* outPort = nullptr) = 0;

    // Connect to a remote address and port
    virtual bool Connect(const std::string& address, uint16_t port) = 0;

    // Send data (returns number of bytes sent, or -1 on error)
    virtual size_t Send(const void* data, size_t size) = 0;

    // Receive data (returns number of bytes received, or -1 on error)
    virtual size_t Receive(void* buffer, size_t size) = 0;

    // Send data to a specific address (for datagram sockets)
    virtual size_t SendTo(const void* data, size_t size, const std::string& address, uint16_t port) = 0;

    // Receive data from a specific address (for datagram sockets)
    virtual size_t ReceiveFrom(void* buffer, size_t size, std::string* outAddress, uint16_t* outPort) = 0;

    // Set socket option
    virtual bool SetOption(ESocketOptionLevel level, ESocketOption optionName, const void* optionValue, size_t optionLen) = 0;

    // Get socket option
    virtual bool GetOption(ESocketOptionLevel level, ESocketOption optionName, void* optionValue, size_t* optionLen) = 0;

    // Check if the socket is open
    virtual bool IsOpen() const = 0;
};