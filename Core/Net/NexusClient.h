#pragma once

#include "Net/NexusProtocol.h"
#include "Net/BerkeleySocket.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include <iostream>

/** @brief Single-connection IPC client that connects to a CNexusServer.
 *         One instance per application -- pipes share this connection. */
class CNexusClient
{
public:
    using MessageCallback       = std::function<void(const SNexusMessage&)>;
    using BinaryMessageCallback = std::function<void(const SNexusBinaryMessage&)>;

    CNexusClient();
    ~CNexusClient();

    // Non-copyable, non-movable (owns thread + socket)
    CNexusClient(const CNexusClient&) = delete;
    CNexusClient& operator=(const CNexusClient&) = delete;

    /** @param address Server IP address.
     *  @param port    Server port.
     *  @return true on success. */
    bool Connect(const std::string& address, uint16_t port);

    /** @brief Disconnect from the server. */
    void Disconnect();

    /** @brief Enable automatic reconnection monitoring.
     *         If the connection is lost (or was never established), a background thread
     *         will retry every retryInterval. Subscriptions and registration are
     *         automatically restored on reconnection.
     *  @param retryInterval Interval between reconnection attempts. */
    void EnableAutoReconnect(std::chrono::milliseconds retryInterval = std::chrono::milliseconds(3000));

    /** @brief Disable the automatic reconnection monitor. */
    void DisableAutoReconnect();

    /** @param appName  Application name to register.
     *  @param userName User name to register.
     *  @return true on success. */
    bool Register(const std::string& appName, const std::string& userName);

    /** @param pipeName Pipe to subscribe to ("ANY" = wildcard).
     *  @param appName  Application to listen for ("ANY" = wildcard).
     *  @return true on success. */
    bool Subscribe(const std::string& pipeName, const std::string& appName);

    /** @brief Subscribe with a per-subscription message callback. */
    bool Subscribe(const std::string& pipeName, const std::string& appName, MessageCallback callback);

    /** @brief Subscribe with a per-subscription binary message callback. */
    bool SubscribeBinary(const std::string& pipeName, const std::string& appName, BinaryMessageCallback callback);

    /** @param pipeName    Pipe to send on.
     *  @param messageType Message type string.
     *  @param body        Message body.
     *  @return true on success. */
    bool SendPipeMessage(const std::string& pipeName, const std::string& messageType, const std::string& body);

    /** @param pipeName Pipe to send on.
     *  @param data     Binary payload.
     *  @param dataSize Size in bytes.
     *  @return true on success. */
    bool SendBinaryMessage(const std::string& pipeName, const void* data, size_t dataSize);

    /** @param callback Catch-all callback for incoming text messages. */
    void SetMessageCallback(MessageCallback callback);

    /** @param pipeName Pipe filter ("ANY" = all).
     *  @param appName  App filter ("ANY" = all).
     *  @param callback Callback to invoke. */
    void SetMessageCallback(const std::string& pipeName, const std::string& appName, MessageCallback callback);

    /** @param callback Catch-all callback for incoming binary messages. */
    void SetBinaryMessageCallback(BinaryMessageCallback callback);

    /** @brief Set a binary callback filtered by pipe name and/or sender application. */
    void SetBinaryMessageCallback(const std::string& pipeName, const std::string& appName, BinaryMessageCallback callback);

    /** @param pipeName Remove all callbacks (text and binary) matching this pipe name. */
    void RemoveCallbacks(const std::string& pipeName);

    /** @brief Returns true if connected. */
    bool IsConnected() const;

    /** @brief Returns the registered application name. */
    const std::string& GetAppName() const { return m_appName; }

private:
    void ReceiveLoop();
    void DispatchMessage(const SNexusMessage& msg);
    void DispatchBinaryMessage(const SNexusBinaryMessage& msg);
    void MonitorLoop();
    bool RestoreSession();

    struct SFilteredCallback
    {
        std::string pipeName;
        std::string appName;
        MessageCallback callback;
    };

    struct SFilteredBinaryCallback
    {
        std::string pipeName;
        std::string appName;
        BinaryMessageCallback callback;
    };

    struct SSubscriptionRecord
    {
        std::string pipeName;
        std::string appName;
    };

    BerkeleySocket      m_socket;
    std::atomic<bool>   m_connected{false};
    std::thread         m_receiveThread;
    std::mutex          m_sendMutex;
    std::mutex          m_callbackMutex;

    std::string         m_appName;
    std::string         m_userName;

    // Auto-reconnect state
    std::string                  m_serverAddress;
    uint16_t                     m_serverPort{0};
    std::atomic<bool>            m_monitorRunning{false};
    std::thread                  m_monitorThread;
    std::chrono::milliseconds    m_retryInterval{3000};
    std::mutex                   m_monitorMutex;
    std::condition_variable      m_monitorCV;
    std::mutex                   m_subscriptionMutex;
    std::vector<SSubscriptionRecord> m_subscriptionRecords;

    MessageCallback                      m_messageCallback;
    std::vector<SFilteredCallback>       m_filteredCallbacks;

    BinaryMessageCallback                m_binaryMessageCallback;
    std::vector<SFilteredBinaryCallback> m_filteredBinaryCallbacks;
};
