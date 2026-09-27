#pragma once

#include <chrono>
#include <mutex>
#include <functional>
#include <string>
#include <type_traits>

namespace Rathon::Common {

// Resilient Circuit Breaker pattern.
// Prevents cascade failures when interrogating flaky or conditionally-available OS subsystems (e.g., NVML, DXGI).
// States:
// - Closed: Normal operation. Requests pass through.
// - Open: Subsystem has failed repeatedly (exceeded threshold). Requests are short-circuited to fallback.
// - HalfOpen: Cooldown has elapsed. A single probe request is permitted to test subsystem recovery.
class CircuitBreaker {
public:
    enum class State {
        Closed,
        Open,
        HalfOpen
    };

    explicit CircuitBreaker(int failureThreshold = 3, std::chrono::milliseconds cooldown = std::chrono::milliseconds(5000))
        : m_failureThreshold(failureThreshold)
        , m_cooldown(cooldown)
        , m_state(State::Closed)
        , m_failureCount(0)
        , m_lastStateChange(std::chrono::steady_clock::now())
    {
    }

    State state() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_state;
    }

    std::string stateString() const {
        switch (state()) {
            case State::Closed: return "Closed (Healthy)";
            case State::Open: return "Open (Failing - Diverted)";
            case State::HalfOpen: return "HalfOpen (Probing)";
            default: return "Unknown";
        }
    }

    // Executes action protected by circuit breaker. If tripped, executes fallback.
    template <typename Func, typename FallbackFunc>
    auto execute(Func&& action, FallbackFunc&& fallback) -> decltype(action()) {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            auto now = std::chrono::steady_clock::now();

            if (m_state == State::Open) {
                if (now - m_lastStateChange >= m_cooldown) {
                    m_state = State::HalfOpen;
                    m_lastStateChange = now;
                } else {
                    return fallback();
                }
            }
        }

        try {
            if constexpr (std::is_void_v<decltype(action())>) {
                action();
                onSuccess();
            } else {
                auto result = action();
                onSuccess();
                return result;
            }
        } catch (...) {
            onFailure();
            return fallback();
        }
    }

    void recordSuccess() {
        onSuccess();
    }

    void recordFailure() {
        onFailure();
    }

    void reset() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_state = State::Closed;
        m_failureCount = 0;
        m_lastStateChange = std::chrono::steady_clock::now();
    }

private:
    void onSuccess() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_failureCount = 0;
        if (m_state == State::HalfOpen) {
            m_state = State::Closed;
            m_lastStateChange = std::chrono::steady_clock::now();
        }
    }

    void onFailure() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_failureCount++;
        if (m_state == State::HalfOpen || m_failureCount >= m_failureThreshold) {
            m_state = State::Open;
            m_lastStateChange = std::chrono::steady_clock::now();
        }
    }

    mutable std::mutex m_mutex;
    const int m_failureThreshold;
    const std::chrono::milliseconds m_cooldown;
    State m_state;
    int m_failureCount;
    std::chrono::steady_clock::time_point m_lastStateChange;
};

} // namespace Rathon::Common
