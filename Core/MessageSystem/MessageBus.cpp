#include "MessageBus.h"
#include <utility>

namespace Core {
    namespace MessageSystem {

        MessageBus& MessageBus::Get()
        {
            static MessageBus g_instance;
            return g_instance;
        }

        void MessageBus::Post(const Message& msg)
        {
            std::lock_guard<std::mutex> lock(m_pendingMutex);
            m_pending.push_back(msg);
        }

        void MessageBus::PostString(const std::string& topic, const std::string& s)
        {
            Message m(topic);
            m.SetString(s);
            Post(m);
        }

        void MessageBus::PostInt(const std::string& topic, int v)
        {
            Message m(topic);
            m.SetInt(v);
            Post(m);
        }

        void MessageBus::PostFloat(const std::string& topic, float v)
        {
            Message m(topic);
            m.SetFloat(v);
            Post(m);
        }

        void MessageBus::PostObject(const std::string& topic, const std::weak_ptr<CReflectedBase>& o)
        {
            Message m(topic);
            m.SetObject(o);
            Post(m);
        }

        uint64_t MessageBus::Subscribe(const std::string& topic, MessageHandler handler)
        {
            uint64_t id = m_nextSubscriberId.fetch_add(1, std::memory_order_relaxed);
            std::lock_guard<std::mutex> lock(m_subMutex);
            m_subscribers[topic].emplace_back(id, std::move(handler));
            return id;
        }

        bool MessageBus::Unsubscribe(const std::string& topic, uint64_t subscriptionId)
        {
            std::lock_guard<std::mutex> lock(m_subMutex);
            auto it = m_subscribers.find(topic);
            if (it == m_subscribers.end()) return false;
            auto& vec = it->second;
            for (auto vit = vec.begin(); vit != vec.end(); ++vit) {
                if (vit->first == subscriptionId) {
                    vec.erase(vit);
                    if (vec.empty()) m_subscribers.erase(it);
                    return true;
                }
            }
            return false;
        }

        void MessageBus::UnsubscribeAll(const std::string& topic)
        {
            std::lock_guard<std::mutex> lock(m_subMutex);
            m_subscribers.erase(topic);
        }

        size_t MessageBus::GetPendingCount()
        {
            std::lock_guard<std::mutex> lock(m_pendingMutex);
            return m_pending.size();
        }

        void MessageBus::ProcessAll()
        {
            ProcessAll(MessageHandler());
        }

        void MessageBus::ProcessAll(const MessageHandler& defaultHandler)
        {
            // Inline swap of pending queue to avoid accessing private members from a non-member helper.
            std::vector<Message> work;
            {
                std::lock_guard<std::mutex> lock(m_pendingMutex);
                if (m_pending.empty()) return;
                work.swap(m_pending);
            }

            for (const Message& msg : work) {
                // snapshot handlers for this topic
                std::vector<MessageHandler> handlersToCall;
                {
                    std::lock_guard<std::mutex> lock(m_subMutex);
                    auto it = m_subscribers.find(msg.topic);
                    if (it != m_subscribers.end()) {
                        handlersToCall.reserve(it->second.size());
                        for (const auto& pair : it->second) {
                            handlersToCall.push_back(pair.second);
                        }
                    }
                }

                if (!handlersToCall.empty()) {
                    for (const auto& h : handlersToCall) {
                        if (h) h(msg);
                    }
                }
                else {
                    if (defaultHandler) defaultHandler(msg);
                }
            }
        }

    }
} // namespace Core::MessageSystem