#pragma once

#include <functional>
#include <vector>
#include <mutex>
#include <memory>
#include <atomic>

namespace Rathon::Common {

// Reactive Event Stream Publisher / Subscriber.
// Satisfies Requirement 6 (Reactive Streaming vs Repetitive Polling):
// Decouples telemetry sources from consumers. Instead of polling every 1-2 seconds with
// blocking questions ("Is there data yet?"), the reactive telemetry publisher pushes
// updates unidirectionally to subscribers the moment updates occur.
template <typename TEvent>
class ReactiveStream {
public:
    using Subscriber = std::function<void(const TEvent&)>;
    using SubscriptionId = uint64_t;

    ReactiveStream()
        : m_nextId(1)
    {
    }

    SubscriptionId subscribe(Subscriber subscriber) {
        std::lock_guard<std::mutex> lock(m_mutex);
        SubscriptionId id = m_nextId++;
        m_subscribers.emplace_back(id, std::move(subscriber));
        return id;
    }

    void unsubscribe(SubscriptionId id) {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (auto it = m_subscribers.begin(); it != m_subscribers.end(); ++it) {
            if (it->first == id) {
                m_subscribers.erase(it);
                break;
            }
        }
    }

    void publish(const TEvent& event) {
        std::vector<Subscriber> callbacks;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            callbacks.reserve(m_subscribers.size());
            for (const auto& [id, sub] : m_subscribers) {
                callbacks.push_back(sub);
            }
        }

        for (const auto& callback : callbacks) {
            if (callback) {
                callback(event);
            }
        }
    }

    size_t subscriberCount() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_subscribers.size();
    }

private:
    mutable std::mutex m_mutex;
    std::atomic<SubscriptionId> m_nextId;
    std::vector<std::pair<SubscriptionId, Subscriber>> m_subscribers;
};

} // namespace Rathon::Common
