#include "Net/NexusServer.h"
#include "Net/IBerkeleySocket.h"
#include "DebugChannel/DebugChannel.h"
#include <algorithm>
#include <cstring>

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------

DebugChannels::CDebugChannel NexusDebug("Nexus");

CNexusServer::CNexusServer() = default;

CNexusServer::~CNexusServer() {
    Stop();
}

// ---------------------------------------------------------------------------
// Public interface
// ---------------------------------------------------------------------------

bool CNexusServer::Start( const std::string& address, uint16_t port) {

    if (!m_listenSocket.Open(ESocketDomain::IPv4, ESocketType::Stream, ESocketProtocol::TCP)) return false;

    // Allow address reuse
    int opt = 1;
    m_listenSocket.SetOption(ESocketOptionLevel::Socket, ESocketOption::ReuseAddress, &opt, sizeof(opt));

    if (!m_listenSocket.Bind(address, port)) return false;
    if (!m_listenSocket.Listen()) return false;

    m_running = true;
    m_acceptThread = std::thread(&CNexusServer::AcceptLoop, this);
    return true;
}

void CNexusServer::Stop() {
    m_running = false;

    if ( m_listenSocket.IsOpen()) {
        m_listenSocket.Close();
    }

    if (m_acceptThread.joinable()) {
        m_acceptThread.join();
    }

    // After the accept thread has joined, no new clients can be added.
    // Close all sockets and move the entries out in a single lock scope.
    std::vector<std::shared_ptr<SNexusClientEntry>> remaining;
    {
        std::lock_guard<std::mutex> lock(m_clientsMutex);
        for (auto& entry : m_clients) {
            if (entry->socket && entry->socket->IsOpen()) {
                entry->socket->Close();
            }
        }
        remaining = std::move(m_clients);
    }

    // Join all client threads outside the lock.
    for (auto& entry : remaining) {
        if (entry->thread.joinable()) entry->thread.join();
    }
}

void CNexusServer::SetMessageCallback(MessageCallback callback) {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_messageCallback = std::move(callback);
}

void CNexusServer::SetBinaryMessageCallback(BinaryMessageCallback callback) {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_binaryMessageCallback = std::move(callback);
}

std::vector<SNexusClientSnapshot> CNexusServer::GetClientSnapshots() const {
    std::vector<SNexusClientSnapshot> snapshots;
    std::lock_guard<std::mutex> lock(m_clientsMutex);
    snapshots.reserve(m_clients.size());
    for (const auto& entry : m_clients) {
        if (!entry || !entry->socket || !entry->socket->IsOpen()) continue;
        SNexusClientSnapshot snap;
        snap.appName       = entry->appName;
        snap.macAddress    = entry->macAddress;
        snap.registered    = entry->registered;
        snap.subscriptions = entry->subscriptions;
        snapshots.push_back(std::move(snap));
    }
    return snapshots;
}

// ---------------------------------------------------------------------------
// Accept loop � runs on its own thread
// ---------------------------------------------------------------------------

void CNexusServer::AcceptLoop() {
    while (m_running) {
        std::string addr;
        uint16_t port = 0;
        BerkeleySocket* clientSock = m_listenSocket.Accept(&addr, &port);
        if (!clientSock) continue;

        auto entry = std::make_shared<SNexusClientEntry>();
        entry->socket.reset(clientSock);

        // Capture a shared_ptr copy for the thread so the entry stays alive
        // even if PruneDeadClients erases it from the vector.
        std::shared_ptr<SNexusClientEntry> sharedEntry = entry;

        {
            std::lock_guard<std::mutex> lock(m_clientsMutex);

            // Prune dead clients before adding new ones to prevent unbounded growth.
            // Threads for dead clients have already exited (their socket is closed),
            // so joining them here won't block the accept loop for long.
            PruneDeadClients();

            entry->thread = std::thread(&CNexusServer::ClientLoop, this, sharedEntry);
            m_clients.push_back(std::move(entry));
        }
    }
}

// ---------------------------------------------------------------------------
// Per-client receive loop
// ---------------------------------------------------------------------------

void CNexusServer::ClientLoop(std::shared_ptr<SNexusClientEntry> client) {
    constexpr size_t kBufferSize = 8192;
    char buffer[kBufferSize];
    std::string accumulator;

    while (m_running && client->socket && client->socket->IsOpen()) {
        size_t bytesRead = client->socket->Receive(buffer, kBufferSize);
        if (bytesRead == 0 || bytesRead == static_cast<size_t>(-1)) break;

        accumulator.append(buffer, bytesRead);

        // Process as many complete packets as available
        while (accumulator.size() >= sizeof(SNexusHeader)) {
            SNexusHeader header{};
            std::memcpy(&header, accumulator.data(), sizeof(header));

            // Reject oversized payloads to prevent memory exhaustion
            if (header.payloadSize > kNexusMaxPayloadSize) {
                accumulator.clear();
                RemoveClient(client.get());
                return;
            }

            size_t totalSize = sizeof(header) + header.payloadSize;
            if (accumulator.size() < totalSize) break; // incomplete

            std::string payload = accumulator.substr(sizeof(header), header.payloadSize);
            accumulator.erase(0, totalSize);

            HandlePacket(client.get(), header, payload);
        }
    }

    RemoveClient(client.get());
}

// ---------------------------------------------------------------------------
// Packet handling
// ---------------------------------------------------------------------------

void CNexusServer::HandlePacket(SNexusClientEntry* client,
                                const SNexusHeader& header,
                                const std::string& payload) {
    const char* cursor = payload.data();
    size_t remaining = payload.size();

    switch (header.type) {
    case ENexusMessageType::Register: {
        std::string appName    = DeserializeString(cursor, remaining);
        std::string macAddress = DeserializeString(cursor, remaining);

        // Validate that both fields were deserialized successfully
        if (appName.empty() || macAddress.empty()) break;

        client->appName    = std::move(appName);
        client->macAddress = std::move(macAddress);
        client->registered = true;

        // Send ACK
        std::string ack = BuildAckPacket();
        std::lock_guard<std::mutex> lock(client->sendMutex);
        SendAll(client->socket.get(), ack.data(), ack.size());
        break;
    }
    case ENexusMessageType::Subscribe: {
        SNexusSubscription sub;
        sub.pipeName = DeserializeString(cursor, remaining);
        sub.appName  = DeserializeString(cursor, remaining);

        // Validate that both fields were deserialized successfully
        if (sub.pipeName.empty() || sub.appName.empty()) break;

        client->subscriptions.push_back(std::move(sub));

        std::string ack = BuildAckPacket();
        std::lock_guard<std::mutex> lock(client->sendMutex);
        SendAll(client->socket.get(), ack.data(), ack.size());
        break;
    }
    case ENexusMessageType::PipeMessage: {
        SNexusMessage msg;
        msg.senderApp   = DeserializeString(cursor, remaining);
        msg.pipeName    = DeserializeString(cursor, remaining);
        msg.messageType = DeserializeString(cursor, remaining);
        msg.body        = DeserializeString(cursor, remaining);

        NexusDebug.print(msg.pipeName);
        // Validate required fields
        if (msg.senderApp.empty() || msg.pipeName.empty()) break;

        {
            std::lock_guard<std::mutex> lock(m_callbackMutex);
            if (m_messageCallback) {
                m_messageCallback(msg);
            }
        }

        ForwardMessage(client, msg);
        break;
    }
    case ENexusMessageType::BinaryMessage: {
        SNexusBinaryMessage msg;
        msg.senderApp = DeserializeString(cursor, remaining);
        msg.pipeName  = DeserializeString(cursor, remaining);
        msg.data      = DeserializeBlob(cursor, remaining);

        NexusDebug.print(msg.pipeName);
        // Validate required fields
        if (msg.senderApp.empty() || msg.pipeName.empty()) break;

        {
            std::lock_guard<std::mutex> lock(m_callbackMutex);
            if (m_binaryMessageCallback) {
                m_binaryMessageCallback(msg);
            }
        }

        ForwardBinaryMessage(client, msg);
        break;
    }
    default:
        break;
    }
}

// ---------------------------------------------------------------------------
// Message forwarding
// ---------------------------------------------------------------------------

bool CNexusServer::MatchesSubscription(const SNexusSubscription& sub,
                                       const std::string& pipeName,
                                       const std::string& senderApp) const {
    bool pipeMatch = (sub.pipeName == "ANY" || sub.pipeName == pipeName);
    bool appMatch  = (sub.appName  == "ANY" || sub.appName  == senderApp);
    return pipeMatch && appMatch;
}

void CNexusServer::ForwardMessage(const SNexusClientEntry* sender, const SNexusMessage& msg) {
    std::string packet = BuildPipeMessagePacket(msg.senderApp, msg.pipeName, msg.messageType, msg.body);


    // Snapshot matching targets as shared_ptrs under the lock.
    // The shared_ptr keeps each entry alive even if PruneDeadClients
    // erases it from m_clients on the accept thread.
    std::vector<std::shared_ptr<SNexusClientEntry>> targets;

    {
        std::lock_guard<std::mutex> lock(m_clientsMutex);
        for (auto& entry : m_clients) {
            if (!entry->registered || !entry->socket || !entry->socket->IsOpen()) continue;

            // Don't echo the message back to the sender
            if (entry.get() == sender) continue;

            for (const auto& sub : entry->subscriptions) {
                if (MatchesSubscription(sub, msg.pipeName, msg.senderApp)) {
                    targets.push_back(entry);
                    break; // send once per client
                }
            }
        }
    }

    // Send outside m_clientsMutex � each client's sendMutex still serialises writes.
    for (auto& target : targets) {
        std::lock_guard<std::mutex> sendLock(target->sendMutex);
        if (target->socket && target->socket->IsOpen()) {
            SendAll(target->socket.get(), packet.data(), packet.size());
        }
    }
}

void CNexusServer::ForwardBinaryMessage(const SNexusClientEntry* sender, const SNexusBinaryMessage& msg) {
    std::string packet = BuildBinaryMessagePacket(msg.senderApp, msg.pipeName, msg.data.data(), msg.data.size());

    std::vector<std::shared_ptr<SNexusClientEntry>> targets;

    {
        std::lock_guard<std::mutex> lock(m_clientsMutex);
        for (auto& entry : m_clients) {
            if (!entry->registered || !entry->socket || !entry->socket->IsOpen()) continue;

            if (entry.get() == sender) continue;

            for (const auto& sub : entry->subscriptions) {
                if (MatchesSubscription(sub, msg.pipeName, msg.senderApp)) {
                    targets.push_back(entry);
                    break;
                }
            }
        }
    }

    for (auto& target : targets) {
        std::lock_guard<std::mutex> sendLock(target->sendMutex);
        if (target->socket && target->socket->IsOpen()) {
            SendAll(target->socket.get(), packet.data(), packet.size());
        }
    }
}

// ---------------------------------------------------------------------------
// Client removal & pruning
// ---------------------------------------------------------------------------

void CNexusServer::RemoveClient(SNexusClientEntry* client) {
    if (client->socket && client->socket->IsOpen()) {
        client->socket->Close();
    }
    // The client entry will be pruned from the vector in PruneDeadClients(),
    // which is called under m_clientsMutex during the next accept.
}

void CNexusServer::PruneDeadClients() {
    // Must be called while m_clientsMutex is held.
    // Join finished threads and erase their corresponding client entries.
    for (size_t i = 0; i < m_clients.size(); ) {
        bool isDead = !m_clients[i]->socket || !m_clients[i]->socket->IsOpen();
        if (isDead) {
            if (m_clients[i]->thread.joinable()) {
                m_clients[i]->thread.join();
            }
            m_clients.erase(m_clients.begin() + static_cast<ptrdiff_t>(i));
        } else {
            ++i;
        }
    }
}