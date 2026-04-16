#pragma once

#include "../Reflection/ReflectionBase.h"

#include <string>
#include <vector>
#include <mutex>
#include <functional>
#include <memory>
#include <cstdint>
#include <unordered_map>
#include <atomic>
#include <utility>

namespace Core {
namespace MessageSystem {

// ── Message ───────────────────────────────────────────────────────────────────

/** @brief A single topic-based message that can carry optional payload fields. */
struct Message
{
    enum Flags : uint8_t
    {
        kHasString = 1 << 0,
        kHasInt    = 1 << 1,
        kHasFloat  = 1 << 2,
        kHasObject = 1 << 3
    };

    std::string                     topic;
    uint8_t                         flags    = 0;
    std::string                     str;
    int                             intVal   = 0;
    float                           floatVal = 0.0f;
    std::weak_ptr<CReflectedBase>   object;

    explicit Message(const std::string& t) : topic(t) {}

    void SetString(const std::string& s) { str      = s; flags |= kHasString; }
    void SetInt(int v)                   { intVal   = v; flags |= kHasInt; }
    void SetFloat(float v)               { floatVal = v; flags |= kHasFloat; }
    void SetObject(const std::weak_ptr<CReflectedBase>& o) { object = o; flags |= kHasObject; }

    bool HasString() const { return (flags & kHasString) != 0; }
    bool HasInt()    const { return (flags & kHasInt)    != 0; }
    bool HasFloat()  const { return (flags & kHasFloat)  != 0; }
    bool HasObject() const { return (flags & kHasObject) != 0; }
};

using MessageHandler = std::function<void(const Message&)>;

// ── MessageBus ────────────────────────────────────────────────────────────────

/** @brief Thread-safe, topic-based message bus (singleton). */
class MessageBus
{
public:
    static MessageBus& Get();

    // ── Posting ──────────────────────────────────────────────────────────────

    /** @param msg Message to post. */
    void Post(const Message& msg);
    /** @param topic Topic string. @param s String payload. */
    void PostString(const std::string& topic, const std::string& s);
    /** @param topic Topic string. @param v Integer payload. */
    void PostInt(const std::string& topic, int v);
    /** @param topic Topic string. @param v Float payload. */
    void PostFloat(const std::string& topic, float v);
    /** @param topic Topic string. @param o Object payload. */
    void PostObject(const std::string& topic, const std::weak_ptr<CReflectedBase>& o);

    // ── Subscription ─────────────────────────────────────────────────────────

    /** @param topic Topic to subscribe to.
     *  @param handler Callback invoked when a matching message is processed.
     *  @return Subscription ID for later unsubscription. */
    uint64_t Subscribe(const std::string& topic, MessageHandler handler);

    /** @param topic Topic the subscription belongs to.
     *  @param subscriptionId ID returned from Subscribe().
     *  @return True if the subscription was found and removed. */
    bool Unsubscribe(const std::string& topic, uint64_t subscriptionId);

    /** @param topic Removes all subscriptions for this topic. */
    void UnsubscribeAll(const std::string& topic);

    // ── Processing ───────────────────────────────────────────────────────────

    /** @brief Dispatch all pending messages to their registered handlers. */
    void ProcessAll();
    /** @param defaultHandler Called for messages with no registered subscribers. */
    void ProcessAll(const MessageHandler& defaultHandler);

    // ── Utilities ────────────────────────────────────────────────────────────

    size_t GetPendingCount();

private:
    MessageBus()  = default;
    ~MessageBus() = default;

    std::mutex           m_pendingMutex;
    std::vector<Message> m_pending;

    std::mutex m_subMutex;
    std::unordered_map<std::string, std::vector<std::pair<uint64_t, MessageHandler>>> m_subscribers;
    std::atomic<uint64_t> m_nextSubscriberId{ 1 };
};

} // namespace MessageSystem
} // namespace Core
