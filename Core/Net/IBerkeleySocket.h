#pragma once

#include <cstddef>
#include <string>
#include <vector>
#include <cstdint>

// ── Platform-Independent Socket Constants ─────────────────────────────────────

/** @brief Address families. */
enum class ESocketDomain : int
{
    IPv4 = 0,  // AF_INET
    IPv6 = 1,  // AF_INET6
};

/** @brief Socket types. */
enum class ESocketType : int
{
    Stream   = 0,  // SOCK_STREAM
    Datagram = 1,  // SOCK_DGRAM
};

/** @brief Protocols. */
enum class ESocketProtocol : int
{
    TCP = 0,  // IPPROTO_TCP
    UDP = 1,  // IPPROTO_UDP
};

/** @brief Option levels. */
enum class ESocketOptionLevel : int
{
    Socket = 0,  // SOL_SOCKET
    TCP    = 1,  // IPPROTO_TCP
};

/** @brief Socket options. */
enum class ESocketOption : int
{
    ReuseAddress = 0,  // SO_REUSEADDR
    KeepAlive    = 1,  // SO_KEEPALIVE
    NoDelay      = 2,  // TCP_NODELAY
};

/** @brief Generic interface for a Berkeley socket. */
class IBerkeleySocket
{
public:
    virtual ~IBerkeleySocket() = default;

    /** @param domain   Address family.
     *  @param type     Socket type.
     *  @param protocol Protocol.
     *  @return true on success. */
    virtual bool Open(ESocketDomain domain, ESocketType type, ESocketProtocol protocol) = 0;

    /** @brief Close the socket. */
    virtual void Close() = 0;

    /** @param address Local address to bind to.
     *  @param port    Local port to bind to.
     *  @return true on success. */
    virtual bool Bind(const std::string& address, uint16_t port) = 0;

    /** @param backlog Maximum pending connection queue length.
     *  @return true on success. */
    virtual bool Listen(int backlog = 5) = 0;

    /** @param outAddress Optional output for remote address.
     *  @param outPort    Optional output for remote port.
     *  @return Newly-allocated socket, or nullptr on failure. */
    virtual IBerkeleySocket* Accept(std::string* outAddress = nullptr, uint16_t* outPort = nullptr) = 0;

    /** @param address Remote address to connect to.
     *  @param port    Remote port to connect to.
     *  @return true on success. */
    virtual bool Connect(const std::string& address, uint16_t port) = 0;

    /** @param data Pointer to data buffer.
     *  @param size Number of bytes to send.
     *  @return Bytes sent, or static_cast<size_t>(-1) on error. */
    virtual size_t Send(const void* data, size_t size) = 0;

    /** @param buffer Pointer to receive buffer.
     *  @param size   Buffer size in bytes.
     *  @return Bytes received, or static_cast<size_t>(-1) on error. */
    virtual size_t Receive(void* buffer, size_t size) = 0;

    /** @brief Send data to a specific address (for datagram sockets). */
    virtual size_t SendTo(const void* data, size_t size, const std::string& address, uint16_t port) = 0;

    /** @brief Receive data from a specific address (for datagram sockets). */
    virtual size_t ReceiveFrom(void* buffer, size_t size, std::string* outAddress, uint16_t* outPort) = 0;

    /** @brief Set socket option. */
    virtual bool SetOption(ESocketOptionLevel level, ESocketOption optionName, const void* optionValue, size_t optionLen) = 0;

    /** @brief Get socket option. */
    virtual bool GetOption(ESocketOptionLevel level, ESocketOption optionName, void* optionValue, size_t* optionLen) = 0;

    /** @brief Returns true if the socket is open. */
    virtual bool IsOpen() const = 0;
};
