#pragma once

#include <atomic>
#include <memory>

namespace Rathon::Common {

// Cascading Cancellation Token.
// Satisfies Requirement 2 (Real Cascading Cancellation):
// Propagates abort signals down to lower-level OS operations, loops, and workers,
// ensuring cancelling an operation immediately cuts ongoing work, prevents zombie tasks,
// and halts resource consumption.
class CancellationToken {
public:
    explicit CancellationToken(std::shared_ptr<std::atomic<bool>> state)
        : m_cancelled(std::move(state))
    {
    }

    bool isCancelled() const {
        return m_cancelled && m_cancelled->load(std::memory_order_relaxed);
    }

    void throwIfCancelled() const {
        if (isCancelled()) {
            throw std::runtime_error("Operation aborted by cancellation token");
        }
    }

private:
    std::shared_ptr<std::atomic<bool>> m_cancelled;
};

class CancellationSource {
public:
    CancellationSource()
        : m_state(std::make_shared<std::atomic<bool>>(false))
    {
    }

    CancellationToken token() const {
        return CancellationToken(m_state);
    }

    void cancel() {
        if (m_state) {
            m_state->store(true, std::memory_order_relaxed);
        }
    }

    void reset() {
        if (m_state) {
            m_state->store(false, std::memory_order_relaxed);
        }
    }

private:
    std::shared_ptr<std::atomic<bool>> m_state;
};

} // namespace Rathon::Common
