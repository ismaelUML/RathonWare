#pragma once

#include "ports/driven_ports.h"
#include <memory>
#include <vector>

namespace Rathon::UseCases {

class CpuUseCases {
public:
    explicit CpuUseCases(std::shared_ptr<Ports::ICpuTopologyPort> port)
        : m_port(std::move(port))
    {
    }

    std::vector<Domain::CpuCoreMetric> getCoreMetrics() {
        if (!m_port) return {};
        return m_port->queryCoreMetrics();
    }

private:
    std::shared_ptr<Ports::ICpuTopologyPort> m_port;
};

} // namespace Rathon::UseCases
