#pragma once

#include "ports/driven_ports.h"
#include "domain/process_tree.h"
#include <memory>
#include <vector>

namespace Rathon::UseCases {

class PortUseCases {
public:
    explicit PortUseCases(std::shared_ptr<Ports::IPortPort> port)
        : m_port(std::move(port))
    {
    }

    std::vector<Domain::PortBinding> getAllPorts() {
        if (!m_port) return {};
        return m_port->queryAllPortBindings();
    }

    std::vector<Domain::PortBinding> getListeningPorts() {
        auto all = getAllPorts();
        std::vector<Domain::PortBinding> filtered;
        for (const auto& entry : all) {
            if (entry.state == "LISTENING" || entry.state == "ESTABLISHED" || entry.state == "BOUND") {
                filtered.push_back(entry);
            }
        }
        return filtered;
    }

    std::vector<Domain::PortBinding> getProcessesByPort(int portNumber) {
        auto all = getAllPorts();
        std::vector<Domain::PortBinding> filtered;
        for (const auto& entry : all) {
            if (entry.port == portNumber) {
                filtered.push_back(entry);
            }
        }
        return filtered;
    }

    bool killProcessOnPort(int portNumber) {
        auto targets = getProcessesByPort(portNumber);
        if (targets.empty()) return false;

        bool allKilled = true;
        for (const auto& target : targets) {
            if (Domain::ProcessTreeResolver::isProtectedPid(target.pid)) {
                continue;
            }
            if (!m_port->terminateProcessOnPort(target.pid)) {
                allKilled = false;
            }
        }
        return allKilled;
    }

    bool killProcessByPid(uint32_t pid) {
        if (Domain::ProcessTreeResolver::isProtectedPid(pid) || !m_port) {
            return false;
        }
        return m_port->terminateProcessOnPort(pid);
    }

private:
    std::shared_ptr<Ports::IPortPort> m_port;
};

} // namespace Rathon::UseCases
