#pragma once

#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <future>
#include <atomic>
#include <memory>

namespace Rathon::Common {

// Concurrency Backpressure & Bounded Worker Pool.
// Enforces strict upper bounds on memory and execution concurrency:
// 1. Fixed worker threads (prevents thread thrashing and memory ballooning).
// 2. Maximum queue size limit (Backpressure): when queue is saturated, rejects new tasks
//    immediately instead of buffering infinitely and causing OOM or OS freezes.
class BoundedExecutor {
public:
    explicit BoundedExecutor(size_t workerCount = 2, size_t maxQueueCapacity = 16)
        : m_maxQueueCapacity(maxQueueCapacity)
        , m_stopping(false)
        , m_rejectedCount(0)
    {
        for (size_t i = 0; i < workerCount; ++i) {
            m_workers.emplace_back([this]() {
                workerLoop();
            });
        }
    }

    ~BoundedExecutor() {
        shutdown();
    }

    // Submits task with backpressure check. Returns false if queue is saturated.
    bool submit(std::function<void()> task) {
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            if (m_stopping || m_tasks.size() >= m_maxQueueCapacity) {
                m_rejectedCount++;
                return false; // Backpressure: capacity rejected
            }
            m_tasks.push(std::move(task));
        }
        m_cv.notify_one();
        return true;
    }

    size_t queueSize() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_tasks.size();
    }

    size_t capacity() const {
        return m_maxQueueCapacity;
    }

    uint64_t rejectedTasksCount() const {
        return m_rejectedCount.load();
    }

    void shutdown() {
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            if (m_stopping) return;
            m_stopping = true;
        }
        m_cv.notify_all();
        for (std::thread& worker : m_workers) {
            if (worker.joinable()) {
                worker.join();
            }
        }
        m_workers.clear();
    }

private:
    void workerLoop() {
        while (true) {
            std::function<void()> task;
            {
                std::unique_lock<std::mutex> lock(m_mutex);
                m_cv.wait(lock, [this]() {
                    return m_stopping || !m_tasks.empty();
                });

                if (m_stopping && m_tasks.empty()) {
                    return;
                }

                task = std::move(m_tasks.front());
                m_tasks.pop();
            }

            if (task) {
                try {
                    task();
                } catch (...) {
                    // Suppress unhandled exceptions from worker threads to prevent abort()
                }
            }
        }
    }

    const size_t m_maxQueueCapacity;
    mutable std::mutex m_mutex;
    std::condition_variable m_cv;
    std::queue<std::function<void()>> m_tasks;
    std::atomic<bool> m_stopping;
    std::atomic<uint64_t> m_rejectedCount;
    std::vector<std::thread> m_workers;
};

} // namespace Rathon::Common
