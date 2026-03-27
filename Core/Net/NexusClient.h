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

// ---------------------------------------------------------------------------
// Single-connection IPC client that connects to a CNexusServer.
// One instance per application — pipes share this connection.
// ---------------------------------------------------------------------------
class CNexusClient {
public:
    using MessageCallback       = std::function<void(const SNexusMessage&)>;
    using BinaryMessageCallback = std::function<void(const SNexusBinaryMessage&)>;

    CNexusClient();
    ~CNexusClient();

    // Non-copyable, non-movable (owns thread + socket)
    CNexusClient(const CNexusClient&) = delete;
    CNexusClient& operator=(const CNexusClient&) = delete;

    // Connect to a server at the given IP and port.
    bool Connect(const std::string& address, uint16_t port);

    // Disconnect from the server.
    void Disconnect();

    // Enable automatic reconnection monitoring.
    // If the connection is lost (or was never established), a background thread
    // will retry every |retryInterval|. Subscriptions and registration are
    // automatically restored on reconnection.
    void EnableAutoReconnect(std::chrono::milliseconds retryInterval = std::chrono::milliseconds(3000));

    // Disable the automatic reconnection monitor.
    void DisableAutoReconnect();

    // Register this client's application name and user name with the server.
    bool Register(const std::string& appName, const std::string& userName);

    // Subscribe to messages from a specific named pipe and/or application.
    // Use "ANY" for either parameter to receive all.
    bool Subscribe(const std::string& pipeName, const std::string& appName);
    bool Subscribe(const std::string& pipeName, const std::string& appName, MessageCallback callback);
    bool SubscribeBinary(const std::string& pipeName, const std::string& appName, BinaryMessageCallback callback);
    // Send a text message on a named pipe through the server.
    bool SendPipeMessage(const std::string& pipeName, const std::string& messageType, const std::string& body);

    // Send a binary payload on a named pipe through the server.
    bool SendBinaryMessage(const std::string& pipeName, const void* data, size_t dataSize);

    // Set a catch-all callback for incoming text messages forwarded by the server.
    void SetMessageCallback(MessageCallback callback);

    // Set a callback filtered by pipe name and/or sender application.
    // Use "ANY" for either parameter to match all values for that field.
    void SetMessageCallback(const std::string& pipeName, const std::string& appName, MessageCallback callback);

    // Set a catch-all callback for incoming binary messages forwarded by the server.
    void SetBinaryMessageCallback(BinaryMessageCallback callback);

    // Set a binary callback filtered by pipe name and/or sender application.
    void SetBinaryMessageCallback(const std::string& pipeName, const std::string& appName, BinaryMessageCallback callback);

    // Remove all filtered callbacks (text and binary) that match the given pipe name.
    void RemoveCallbacks(const std::string& pipeName);

    // Returns true if connected.
    bool IsConnected() const;

    // Returns the registered application name.
    const std::string& GetAppName() const { return m_appName; }

private:
    void ReceiveLoop();
    void DispatchMessage(const SNexusMessage& msg);
    void DispatchBinaryMessage(const SNexusBinaryMessage& msg);
    void MonitorLoop();
    bool RestoreSession();

    struct SFilteredCallback {
        std::string pipeName;
        std::string appName;
        MessageCallback callback;
    };

    struct SFilteredBinaryCallback {
        std::string pipeName;
        std::string appName;
        BinaryMessageCallback callback;
    };

    struct SSubscriptionRecord {
        std::string pipeName;
        std::string appName;
    };

    BerkeleySocket m_socket;
    std::atomic<bool> m_connected{false};
    std::thread m_receiveThread;
    std::mutex m_sendMutex;
    std::mutex m_callbackMutex;

    std::string m_appName;
    std::string m_userName;

    // Auto-reconnect state
    std::string m_serverAddress;
    uint16_t m_serverPort{0};
    std::atomic<bool> m_monitorRunning{false};
    std::thread m_monitorThread;
    std::chrono::milliseconds m_retryInterval{3000};
    std::mutex m_monitorMutex;
    std::condition_variable m_monitorCV;
    std::mutex m_subscriptionMutex;
    std::vector<SSubscriptionRecord> m_subscriptionRecords;

    MessageCallback m_messageCallback;
    std::vector<SFilteredCallback> m_filteredCallbacks;

    BinaryMessageCallback m_binaryMessageCallback;
    std::vector<SFilteredBinaryCallback> m_filteredBinaryCallbacks;
};

