#pragma once

#include "ports/driven_ports.h"
#include "common/circuit_breaker.h"
#include "domain/process_tree.h"
#include <memory>
#include <vector>

namespace Rathon::UseCases {

class GpuUseCases {
public:
    explicit GpuUseCases(std::shared_ptr<Ports::IGpuPort> port)
        : m_port(std::move(port))
        , m_circuitBreaker(3, std::chrono::milliseconds(5000))
    {
    }

    Domain::GpuTelemetry getTelemetry() {
        if (!m_port) {
            return Domain::GpuTelemetry{};
        }

        // Resilient query wrapped in Circuit Breaker
        return m_circuitBreaker.execute(
            [this]() {
                return m_port->queryTelemetry();
            },
            []() {
                Domain::GpuTelemetry fallback;
                fallback.hasGpu = false;
                fallback.throttleReason = "Circuit Breaker Active (Probing Driver)";
                fallback.throttleStatusLevel = "warning";
                return fallback;
            }
        );
    }

    std::vector<Domain::GpuProcess> getProcesses() {
        if (!m_port) return {};
        return m_port->queryGpuProcesses();
    }

    bool terminateProcess(uint32_t pid) {
        if (Domain::ProcessTreeResolver::isProtectedPid(pid) || !m_port) {
            return false;
        }
        return m_port->terminateGpuProcess(pid);
    }

    std::string circuitBreakerState() const {
        return m_circuitBreaker.stateString();
    }

private:
    std::shared_ptr<Ports::IGpuPort> m_port;
    Common::CircuitBreaker m_circuitBreaker;
};

} // namespace Rathon::UseCases
