#include <iostream>
#include <cassert>
#include <vector>
#include <thread>
#include <chrono>

#include "domain/process_tree.h"
#include "common/circuit_breaker.h"
#include "common/bounded_executor.h"
#include "common/ring_buffer.h"
#include "common/cancellation_token.h"

using namespace Rathon;

void test_process_tree_resolution() {
    std::cout << "[TEST] Running test_process_tree_resolution..." << std::endl;

    // Invariant: Protected PIDs (<= 4) cannot be killed
    assert(Domain::ProcessTreeResolver::isProtectedPid(0));
    assert(Domain::ProcessTreeResolver::isProtectedPid(4));
    assert(!Domain::ProcessTreeResolver::isProtectedPid(100));

    auto emptyOrder = Domain::ProcessTreeResolver::resolveBottomUpKillOrder(4, {});
    assert(emptyOrder.empty());

    // Build process tree:
    // Root: 100
    //   Children: 200, 300
    //     Child of 300: 400
    std::vector<std::pair<uint32_t, uint32_t>> pairs = {
        {100, 200},
        {100, 300},
        {300, 400}
    };

    auto killOrder = Domain::ProcessTreeResolver::resolveBottomUpKillOrder(100, pairs);

    // Expected bottom-up order: leaf children first, root last
    // Root (100) must be the very last element in the kill list
    assert(!killOrder.empty());
    assert(killOrder.back() == 100);

    // 400 must appear before 300 in kill order
    auto it400 = std::find(killOrder.begin(), killOrder.end(), 400);
    auto it300 = std::find(killOrder.begin(), killOrder.end(), 300);
    assert(it400 != killOrder.end());
    assert(it300 != killOrder.end());
    assert(std::distance(killOrder.begin(), it400) < std::distance(killOrder.begin(), it300));

    std::cout << " -> PASSED: Process tree reverse BFS order validated." << std::endl;
}

void test_circuit_breaker() {
    std::cout << "[TEST] Running test_circuit_breaker..." << std::endl;

    Common::CircuitBreaker cb(3, std::chrono::milliseconds(50));
    assert(cb.state() == Common::CircuitBreaker::State::Closed);

    // Fail 1 & 2: still Closed
    cb.recordFailure();
    cb.recordFailure();
    assert(cb.state() == Common::CircuitBreaker::State::Closed);

    // Fail 3: Trips to Open
    cb.recordFailure();
    assert(cb.state() == Common::CircuitBreaker::State::Open);

    // When Open, execute() must immediately divert to fallback
    bool fallbackCalled = false;
    cb.execute(
        []() { assert(false && "Action should not be called when circuit is Open"); },
        [&fallbackCalled]() { fallbackCalled = true; }
    );
    assert(fallbackCalled);

    // Wait for cooldown to expire (50ms)
    std::this_thread::sleep_for(std::chrono::milliseconds(60));

    // Next call probes in HalfOpen
    bool probeSucceeded = false;
    cb.execute(
        [&probeSucceeded]() { probeSucceeded = true; },
        []() { assert(false && "Fallback should not be called if probe succeeds"); }
    );
    assert(probeSucceeded);
    assert(cb.state() == Common::CircuitBreaker::State::Closed);

    std::cout << " -> PASSED: Circuit breaker Closed -> Open -> HalfOpen -> Closed validated." << std::endl;
}

void test_bounded_executor_backpressure() {
    std::cout << "[TEST] Running test_bounded_executor_backpressure..." << std::endl;

    // 1 worker, max queue capacity of 2
    Common::BoundedExecutor executor(1, 2);

    std::atomic<bool> blockWorker{true};
    std::atomic<int> completedTasks{0};

    // Task 1: will be picked up and blocked by worker
    bool sub1 = executor.submit([&]() {
        while (blockWorker.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        completedTasks++;
    });
    assert(sub1);

    // Let the worker pick it up
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Now fill queue: Task 2 and Task 3
    bool sub2 = executor.submit([&]() { completedTasks++; });
    bool sub3 = executor.submit([&]() { completedTasks++; });
    assert(sub2);
    assert(sub3);

    // Task 4: Queue is now saturated (size >= 2). Must be rejected immediately!
    bool sub4 = executor.submit([&]() { completedTasks++; });
    assert(!sub4); // BACKPRESSURE REJECTION CONFIRMED
    assert(executor.rejectedTasksCount() >= 1);

    // Unblock worker and let tasks complete
    blockWorker.store(false);
    executor.shutdown();

    assert(completedTasks.load() == 3);
    std::cout << " -> PASSED: Bounded executor backpressure and queue capacity limits validated." << std::endl;
}

void test_eviction_ring_buffer() {
    std::cout << "[TEST] Running test_eviction_ring_buffer..." << std::endl;

    Common::EvictionRingBuffer<int> buffer(5);

    for (int i = 1; i <= 8; ++i) {
        buffer.push(i);
    }

    // Capacity was 5. We pushed 1,2,3,4,5,6,7,8.
    // 1, 2, 3 must have been evicted.
    assert(buffer.size() == 5);

    auto vec = buffer.toVector();
    assert(vec.size() == 5);
    assert(vec[0] == 4);
    assert(vec[1] == 5);
    assert(vec[2] == 6);
    assert(vec[3] == 7);
    assert(vec[4] == 8);

    std::cout << " -> PASSED: EvictionRingBuffer FIFO memory eviction validated." << std::endl;
}

void test_cancellation_token() {
    std::cout << "[TEST] Running test_cancellation_token..." << std::endl;

    Common::CancellationSource source;
    auto token = source.token();

    assert(!token.isCancelled());
    source.cancel();
    assert(token.isCancelled());

    bool threw = false;
    try {
        token.throwIfCancelled();
    } catch (const std::runtime_error&) {
        threw = true;
    }
    assert(threw);

    std::cout << " -> PASSED: CancellationToken cascade abort validated." << std::endl;
}

int main() {
    std::cout << "==========================================" << std::endl;
    std::cout << "  RathonWare Domain & Resilience Unit Tests" << std::endl;
    std::cout << "==========================================" << std::endl;

    test_process_tree_resolution();
    test_circuit_breaker();
    test_bounded_executor_backpressure();
    test_eviction_ring_buffer();
    test_cancellation_token();

    std::cout << "==========================================" << std::endl;
    std::cout << "  ALL UNIT TESTS PASSED SUCCESSFULLY!     " << std::endl;
    std::cout << "==========================================" << std::endl;
    return 0;
}
