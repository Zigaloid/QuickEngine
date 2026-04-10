#pragma once

#include <string>
#include <vector>
#include <mutex>
#include <functional>
#include <memory>
#include <cstdint>
#include <unordered_map>
#include <atomic>
#include <utility>

#include "../Reflection/ReflectionBase.h"

namespace Core {
    namespace MessageSystem {

        struct Message
        {
            // renamed enumerators to avoid collision with method names
            enum Flags : uint8_t {
                kHasString = 1 << 0,
                kHasInt = 1 << 1,
                kHasFloat = 1 << 2,
                kHasObject = 1 << 3
            };

            std::string   topic;
            uint8_t       flags = 0;
            std::string   str;
            int           intVal = 0;
            float         floatVal = 0.0f;
            std::weak_ptr<CReflectedBase> object;

            explicit Message(const std::string& t) : topic(t) {}

            void SetString(const std::string& s) { str = s; flags |= kHasString; }
            void SetInt(int v) { intVal = v; flags |= kHasInt; }
            void SetFloat(float v) { floatVal = v; flags |= kHasFloat; }
            void SetObject(const std::weak_ptr<CReflectedBase>& o) { object = o; flags |= kHasObject; }

            bool HasString() const { return (flags & kHasString) != 0; }
            bool HasInt() const { return (flags & kHasInt) != 0; }
            bool HasFloat() const { return (flags & kHasFloat) != 0; }
            bool HasObject() const { return (flags & kHasObject) != 0; }
        };

        using MessageHandler = std::function<void(const Message&)>;

        class MessageBus
        {
        public:
            static MessageBus& Get();

            // Posting
            void Post(const Message& msg);
            void PostString(const std::string& topic, const std::string& s);
            void PostInt(const std::string& topic, int v);
            void PostFloat(const std::string& topic, float v);
            void PostObject(const std::string& topic, const std::weak_ptr<CReflectedBase>& o);

            // Subscription API
            uint64_t Subscribe(const std::string& topic, MessageHandler handler);
            bool Unsubscribe(const std::string& topic, uint64_t subscriptionId);
            void UnsubscribeAll(const std::string& topic);

            // Process all pending messages; for each message, invoke handlers subscribed to its topic.
            void ProcessAll();
            void ProcessAll(const MessageHandler& defaultHandler);

            // Utilities
            size_t GetPendingCount();

        private:
            MessageBus() = default;
            ~MessageBus() = default;

            std::mutex              m_pendingMutex;
            std::vector<Message>    m_pending;

            std::mutex m_subMutex;
            std::unordered_map<std::string, std::vector<std::pair<uint64_t, MessageHandler>>> m_subscribers;
            std::atomic<uint64_t> m_nextSubscriberId{ 1 };
        };

    }
} // namespace Core::MessageSystem