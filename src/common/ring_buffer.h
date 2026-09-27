#pragma once

#include <vector>
#include <deque>
#include <mutex>
#include <cstddef>

namespace Rathon::Common {

// Memory Eviction Ring Buffer.
// Satisfies Requirement 2 (Memory Eviction Policy):
// Unbounded long-running background tasks leak memory if historical metrics accumulate.
// This container guarantees an immutable upper bound on memory consumption by strictly
// discarding the oldest elements (FIFO eviction) whenever the threshold capacity is reached.
template <typename T>
class EvictionRingBuffer {
public:
    explicit EvictionRingBuffer(size_t maxCapacity = 60)
        : m_maxCapacity(maxCapacity > 0 ? maxCapacity : 1)
    {
    }

    void push(const T& item) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_buffer.size() >= m_maxCapacity) {
            m_buffer.pop_front(); // Evict oldest entry
        }
        m_buffer.push_back(item);
    }

    void push(T&& item) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_buffer.size() >= m_maxCapacity) {
            m_buffer.pop_front(); // Evict oldest entry
        }
        m_buffer.push_back(std::move(item));
    }

    std::vector<T> toVector() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return std::vector<T>(m_buffer.begin(), m_buffer.end());
    }

    size_t size() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_buffer.size();
    }

    size_t capacity() const {
        return m_maxCapacity;
    }

    void clear() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_buffer.clear();
    }

private:
    const size_t m_maxCapacity;
    std::deque<T> m_buffer;
    mutable std::mutex m_mutex;
};

} // namespace Rathon::Common
