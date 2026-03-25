#include "Net/NexusClient.h"
#include "Net/IBerkeleySocket.h"

#include <cstring>

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------

CNexusClient::CNexusClient() = default;

CNexusClient::~CNexusClient() {
    DisableAutoReconnect();
    Disconnect();
}

// ---------------------------------------------------------------------------
// Connection
// ---------------------------------------------------------------------------
bool CNexusClient::Connect(const std::string& address, uint16_t port) {

    m_serverAddress = address;
    m_serverPort = port;

    if (!m_socket.Open(ESocketDomain::IPv4, ESocketType::Stream, ESocketProtocol::TCP)) return false;
    if (!m_socket.Connect(address, port)) {
        m_socket.Close();
        return false;
    }

    m_connected = true;
    m_receiveThread = std::thread(&CNexusClient::ReceiveLoop, this);
    return true;
}

void CNexusClient::Disconnect() {
    m_connected = false;

    if ( m_socket.IsOpen()) {
        m_socket.Close();
    }

    if (m_receiveThread.joinable()) {
        m_receiveThread.join();
    }
}

bool CNexusClient::IsConnected() const {
    return m_connected && m_socket.IsOpen();
}

// ---------------------------------------------------------------------------
// Auto-reconnect
// ---------------------------------------------------------------------------

void CNexusClient::EnableAutoReconnect(std::chrono::milliseconds retryInterval) {
    if (m_monitorRunning) return;

    m_retryInterval  = retryInterval;
    m_monitorRunning = true;
    m_monitorThread  = std::thread(&CNexusClient::MonitorLoop, this);
}

void CNexusClient::DisableAutoReconnect() {
    if (!m_monitorRunning) return;

    m_monitorRunning = false;
    m_monitorCV.notify_all();

    if (m_monitorThread.joinable()) {
        m_monitorThread.join();
    }
}

void CNexusClient::MonitorLoop() {
    while (m_monitorRunning) {
        {
            std::unique_lock<std::mutex> lock(m_monitorMutex);
            m_monitorCV.wait_for(lock, m_retryInterval, [this] { return !m_monitorRunning.load(); });
        }

        if (!m_monitorRunning) break;

        if (!IsConnected() && !m_serverAddress.empty()) {
            // Clean up any previous connection state
            Disconnect();

            if (Connect(m_serverAddress, m_serverPort)) {
                RestoreSession();
            }
        }
    }
}

bool CNexusClient::RestoreSession() {
    // Re-register if we had a previous registration
    if (!m_appName.empty()) {
        if (!Register(m_appName, m_userName)) return false;
    }

    // Re-subscribe to all previously recorded subscriptions
    std::lock_guard<std::mutex> lock(m_subscriptionMutex);
    for (const auto& sub : m_subscriptionRecords) {
        std::string packet = BuildSubscriptionPacket(sub.pipeName, sub.appName);
        std::lock_guard<std::mutex> sendLock(m_sendMutex);
        if (!SendAll(&m_socket, packet.data(), packet.size())) return false;
    }

    return true;
}

// ---------------------------------------------------------------------------
// Registration
// ---------------------------------------------------------------------------

bool CNexusClient::Register(const std::string& appName, const std::string& userName) {
    // Always record the intent so RestoreSession() can replay it
    m_appName  = appName;
    m_userName = userName;

    if (!IsConnected()) return false;

    std::string packet = BuildRegistrationPacket(appName, userName);

    std::lock_guard<std::mutex> lock(m_sendMutex);
    return SendAll(&m_socket, packet.data(), packet.size());
}

// ---------------------------------------------------------------------------
// Subscription
// ---------------------------------------------------------------------------
bool CNexusClient::Subscribe(const std::string& pipeName, const std::string& appName) {
    // Always record the subscription so RestoreSession() can replay it
    {
        std::lock_guard<std::mutex> lock(m_subscriptionMutex);
        bool found = false;
        for (const auto& sub : m_subscriptionRecords) {
            if (sub.pipeName == pipeName && sub.appName == appName) { found = true; break; }
        }
        if (!found) {
            m_subscriptionRecords.push_back({pipeName, appName});
        }
    }

    if (!IsConnected()) return false;

    std::string packet = BuildSubscriptionPacket(pipeName, appName);

    std::lock_guard<std::mutex> lock(m_sendMutex);
    return SendAll(&m_socket, packet.data(), packet.size());
}

bool CNexusClient::Subscribe(const std::string& pipeName, const std::string& appName, MessageCallback callback) {
    // Record the callback regardless of connection state
    SetMessageCallback(pipeName, appName, std::move(callback));

    // Subscribe records the intent and sends if connected
    Subscribe(pipeName, appName);
    return true;
}
bool CNexusClient::SubscribeBinary(const std::string& pipeName, const std::string& appName, BinaryMessageCallback callback) {
    // Record the callback regardless of connection state
    SetBinaryMessageCallback(pipeName, appName, std::move(callback));

    // Subscribe records the intent and sends if connected
    Subscribe(pipeName, appName);
    return true;
}
// ---------------------------------------------------------------------------
// Sending
// ---------------------------------------------------------------------------

bool CNexusClient::SendPipeMessage(const std::string& pipeName, const std::string& body) {
    if (!IsConnected()) return false;

    std::lock_guard<std::mutex> lock(m_sendMutex);
    std::string packet = BuildPipeMessagePacket(m_appName, pipeName, body);
    return SendAll(&m_socket, packet.data(), packet.size());
}

bool CNexusClient::SendBinaryMessage(const std::string& pipeName, const void* data, size_t dataSize) {
    if (!IsConnected()) return false;

    std::lock_guard<std::mutex> lock(m_sendMutex);
    std::string packet = BuildBinaryMessagePacket(m_appName, pipeName, data, dataSize);
    return SendAll(&m_socket, packet.data(), packet.size());
}

// ---------------------------------------------------------------------------
// Receiving
// ---------------------------------------------------------------------------

void CNexusClient::SetMessageCallback(MessageCallback callback) {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_messageCallback = std::move(callback);
}

void CNexusClient::SetMessageCallback(const std::string& pipeName, const std::string& appName, MessageCallback callback) {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_filteredCallbacks.push_back({pipeName, appName, std::move(callback)});
}

void CNexusClient::SetBinaryMessageCallback(BinaryMessageCallback callback) {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_binaryMessageCallback = std::move(callback);
}

void CNexusClient::SetBinaryMessageCallback(const std::string& pipeName, const std::string& appName, BinaryMessageCallback callback) {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_filteredBinaryCallbacks.push_back({pipeName, appName, std::move(callback)});
}

void CNexusClient::RemoveCallbacks(const std::string& pipeName) {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    std::erase_if(m_filteredCallbacks, [&](const SFilteredCallback& entry) {
        return entry.pipeName == pipeName;
    });
    std::erase_if(m_filteredBinaryCallbacks, [&](const SFilteredBinaryCallback& entry) {
        return entry.pipeName == pipeName;
    });
}

void CNexusClient::DispatchMessage(const SNexusMessage& msg) {
    std::lock_guard<std::mutex> lock(m_callbackMutex);

    bool handled = false;

    for (const auto& entry : m_filteredCallbacks) {
        bool pipeMatch = (entry.pipeName == "ANY" || entry.pipeName == msg.pipeName);
        bool appMatch  = (entry.appName  == "ANY" || entry.appName  == msg.senderApp);

        if (pipeMatch && appMatch) {
            entry.callback(msg);
            handled = true;
        }
    }

    // Fall back to the catch-all callback if no filtered callback matched
    if (!handled && m_messageCallback) {
        m_messageCallback(msg);
    }
}

void CNexusClient::DispatchBinaryMessage(const SNexusBinaryMessage& msg) {
    std::lock_guard<std::mutex> lock(m_callbackMutex);

    bool handled = false;

    for (const auto& entry : m_filteredBinaryCallbacks) {
        bool pipeMatch = (entry.pipeName == "ANY" || entry.pipeName == msg.pipeName);
        bool appMatch  = (entry.appName  == "ANY" || entry.appName  == msg.senderApp);

        if (pipeMatch && appMatch) {
            entry.callback(msg);
            handled = true;
        }
    }

    // Fall back to the catch-all binary callback if no filtered callback matched
    if (!handled && m_binaryMessageCallback) {
        m_binaryMessageCallback(msg);
    }
}

void CNexusClient::ReceiveLoop() {
    constexpr size_t kBufferSize = 8192;
    char buffer[kBufferSize];
    std::string accumulator;

    while (m_connected && m_socket.IsOpen()) {
        size_t bytesRead = m_socket.Receive(buffer, kBufferSize);
        if (bytesRead == 0 || bytesRead == static_cast<size_t>(-1)) break;

        accumulator.append(buffer, bytesRead);

        while (accumulator.size() >= sizeof(SNexusHeader)) {
            SNexusHeader header{};
            std::memcpy(&header, accumulator.data(), sizeof(header));

            // Reject oversized payloads to prevent memory exhaustion
            if (header.payloadSize > kNexusMaxPayloadSize) {
                accumulator.clear();
                m_connected = false;
                return;
            }

            size_t totalSize = sizeof(header) + header.payloadSize;
            if (accumulator.size() < totalSize) break;

            std::string payload = accumulator.substr(sizeof(header), header.payloadSize);
            accumulator.erase(0, totalSize);

            if (header.type == ENexusMessageType::PipeMessage) {
                const char* cursor = payload.data();
                size_t remaining = payload.size();

                SNexusMessage msg;
                msg.senderApp = DeserializeString(cursor, remaining);
                msg.pipeName  = DeserializeString(cursor, remaining);
                msg.body      = DeserializeString(cursor, remaining);

                DispatchMessage(msg);
            }
            else if (header.type == ENexusMessageType::BinaryMessage) {
                const char* cursor = payload.data();
                size_t remaining = payload.size();

                SNexusBinaryMessage msg;
                msg.senderApp = DeserializeString(cursor, remaining);
                msg.pipeName  = DeserializeString(cursor, remaining);
                msg.data      = DeserializeBlob(cursor, remaining);

                DispatchBinaryMessage(msg);
            }
            // ACKs are silently consumed
        }
    }

    m_connected = false;
}