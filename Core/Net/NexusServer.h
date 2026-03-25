#pragma once

#include "Net/NexusProtocol.h"
#include "Net/BerkeleySocket.h"

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

// Describes a single connected client and its dedicated receive thread.
struct SNexusClientEntry {
    std::unique_ptr<BerkeleySocket> socket;
    std::string appName;
    std::string macAddress;
    bool registered = false;

    // Each subscription: (pipeName, appName) — "ANY" means wildcard
    std::vector<SNexusSubscription> subscriptions;

    // Guards all Send() calls on this client's socket so that
    // concurrent forwards from different client threads don't interleave.
    std::mutex sendMutex;

    // The dedicated receive thread for this client.
    std::thread thread;
};

// Thread-safe snapshot of a single client's state for UI visualization.
struct SNexusClientSnapshot {
    std::string appName;
    std::string macAddress;
    bool registered = false;
    std::vector<SNexusSubscription> subscriptions;
};

// Server that accepts IPC clients, manages registration / subscription,
// and forwards named-pipe messages to interested parties.
class CNexusServer {
public:
    using MessageCallback       = std::function<void(const SNexusMessage&)>;
    using BinaryMessageCallback = std::function<void(const SNexusBinaryMessage&)>;

    CNexusServer();
    ~CNexusServer();

    // Start listening on the given address and port.
    bool Start( const std::string& address, uint16_t port);

    // Stop the server and disconnect all clients.
    void Stop();

    // Optional callback invoked whenever a text pipe message is received.
    void SetMessageCallback(MessageCallback callback);

    // Optional callback invoked whenever a binary pipe message is received.
    void SetBinaryMessageCallback(BinaryMessageCallback callback);

    // Thread-safe snapshot of all connected clients for UI visualization.
    std::vector<SNexusClientSnapshot> GetClientSnapshots() const;

    // Check if the server is currently running.
    bool IsRunning() const { return m_running.load(); }

private:
    void AcceptLoop();
    void ClientLoop(std::shared_ptr<SNexusClientEntry> client);
    void HandlePacket(SNexusClientEntry* client, const SNexusHeader& header, const std::string& payload);
    void ForwardMessage(const SNexusClientEntry* sender, const SNexusMessage& msg);
    void ForwardBinaryMessage(const SNexusClientEntry* sender, const SNexusBinaryMessage& msg);
    void RemoveClient(SNexusClientEntry* client);
    void PruneDeadClients();
    bool MatchesSubscription(const SNexusSubscription& sub, const std::string& pipeName, const std::string& senderApp) const;

    BerkeleySocket m_listenSocket;
    std::atomic<bool> m_running{false};
    std::thread m_acceptThread;

    mutable std::mutex m_clientsMutex;
    std::vector<std::shared_ptr<SNexusClientEntry>> m_clients;

    std::mutex m_callbackMutex;
    MessageCallback m_messageCallback;
    BinaryMessageCallback m_binaryMessageCallback;
};