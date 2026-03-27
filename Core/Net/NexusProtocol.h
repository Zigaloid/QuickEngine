#pragma once

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "IBerkeleySocket.h"

// Maximum allowed payload size (50 MB). Prevents malicious or corrupt headers
// from causing unbounded memory growth in the receive accumulator.
inline constexpr uint32_t kNexusMaxPayloadSize = 50u << 20;

// Message types used in the IPC protocol
enum class ENexusMessageType : uint8_t {
    Register        = 0x01,  // Client registers app name + MAC address
    Subscribe       = 0x02,  // Client subscribes to a named pipe/app
    PipeMessage     = 0x03,  // Client sends a message on a named pipe
    Ack             = 0x04,  // Server acknowledgment
    BinaryMessage   = 0x05,  // Client sends a binary payload on a named pipe
};

// Fixed-size header prefixed to every IPC message
#pragma pack(push, 1)
struct SNexusHeader {
    ENexusMessageType type;
    uint32_t payloadSize;
};
#pragma pack(pop)

// Registration payload: application name + MAC address
struct SNexusRegistration {
    std::string appName;
    std::string macAddress;
};

// Subscription payload: the named pipe and application to listen for
struct SNexusSubscription {
    std::string pipeName;
    std::string appName;
};

// Pipe message payload
struct SNexusMessage {
    std::string senderApp;
    std::string pipeName;
    std::string messageType;
    std::string body;
};

// Binary pipe message payload
struct SNexusBinaryMessage {
    std::string senderApp;
    std::string pipeName;
    std::vector<uint8_t> data;
};

// ---- Serialisation helpers ------------------------------------------------

inline std::string SerializeString(const std::string& s) {
    uint32_t len = static_cast<uint32_t>(s.size());
    std::string out(reinterpret_cast<const char*>(&len), sizeof(len));
    out.append(s);
    return out;
}

inline std::string DeserializeString(const char*& cursor, size_t& remaining) {
    if (remaining < sizeof(uint32_t)) return {};
    uint32_t len = 0;
    std::memcpy(&len, cursor, sizeof(len));
    cursor += sizeof(len);
    remaining -= sizeof(len);
    if (remaining < len) return {};
    std::string result(cursor, len);
    cursor += len;
    remaining -= len;
    return result;
}

inline std::string SerializeBlob(const void* data, size_t size) {
    uint32_t len = static_cast<uint32_t>(size);
    std::string out(reinterpret_cast<const char*>(&len), sizeof(len));
    out.append(reinterpret_cast<const char*>(data), size);
    return out;
}

inline std::vector<uint8_t> DeserializeBlob(const char*& cursor, size_t& remaining) {
    if (remaining < sizeof(uint32_t)) return {};
    uint32_t len = 0;
    std::memcpy(&len, cursor, sizeof(len));
    cursor += sizeof(len);
    remaining -= sizeof(len);
    if (remaining < len) return {};
    std::vector<uint8_t> result(cursor, cursor + len);
    cursor += len;
    remaining -= len;
    return result;
}

// ---- Send helper ----------------------------------------------------------

// Sends all bytes in the buffer, looping on partial writes.
// Returns true only if every byte was successfully sent.
// Accepts the interface type so it works with any socket implementation.
inline bool SendAll(IBerkeleySocket* socket, const void* data, size_t size) {
    const char* ptr = static_cast<const char*>(data);
    size_t remaining = size;
    while (remaining > 0) {
        size_t sent = socket->Send(ptr, remaining);
        if (sent == 0 || sent == static_cast<size_t>(-1)) return false;
        ptr       += sent;
        remaining -= sent;
    }
    return true;
}

// ---- Packet builders ------------------------------------------------------

inline std::string BuildRegistrationPacket(const std::string& appName, const std::string& userName) {
    std::string payload = SerializeString(appName) + SerializeString(userName);
    SNexusHeader hdr{ENexusMessageType::Register, static_cast<uint32_t>(payload.size())};
    std::string packet(reinterpret_cast<const char*>(&hdr), sizeof(hdr));
    packet.append(payload);
    return packet;
}

inline std::string BuildSubscriptionPacket(const std::string& pipeName, const std::string& appName) {
    std::string payload = SerializeString(pipeName) + SerializeString(appName);
    SNexusHeader hdr{ENexusMessageType::Subscribe, static_cast<uint32_t>(payload.size())};
    std::string packet(reinterpret_cast<const char*>(&hdr), sizeof(hdr));
    packet.append(payload);
    return packet;
}

inline std::string BuildPipeMessagePacket(const std::string& senderApp, const std::string& pipeName, const std::string& messageType, const std::string& body) {
    std::string payload = SerializeString(senderApp) + SerializeString(pipeName) + SerializeString(messageType) + SerializeString(body);
    SNexusHeader hdr{ENexusMessageType::PipeMessage, static_cast<uint32_t>(payload.size())};
    std::string packet(reinterpret_cast<const char*>(&hdr), sizeof(hdr));
    packet.append(payload);
    return packet;
}

inline std::string BuildBinaryMessagePacket(const std::string& senderApp, const std::string& pipeName, const void* data, size_t dataSize) {
    std::string payload = SerializeString(senderApp) + SerializeString(pipeName) + SerializeBlob(data, dataSize);
    SNexusHeader hdr{ENexusMessageType::BinaryMessage, static_cast<uint32_t>(payload.size())};
    std::string packet(reinterpret_cast<const char*>(&hdr), sizeof(hdr));
    packet.append(payload);
    return packet;
}

inline std::string BuildAckPacket() {
    SNexusHeader hdr{ENexusMessageType::Ack, 0};
    return std::string(reinterpret_cast<const char*>(&hdr), sizeof(hdr));
}