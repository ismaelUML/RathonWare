#pragma once

#include "ports/driven_ports.h"
#include "domain/process_tree.h"
#include <memory>
#include <vector>

namespace Rathon::UseCases {

// Process Management Use Cases (Orchestrator Core).
// Coordinates business logic: validates security invariants (never kill PID <= 4),
// computes bottom-up BFS tree kill ordering, and commands the Driven Output Port.
class ProcessUseCases {
public:
    explicit ProcessUseCases(std::shared_ptr<Ports::IProcessPort> port)
        : m_port(std::move(port))
    {
    }

    std::vector<Domain::Process> getProcesses(Common::CancellationToken token = Common::CancellationToken(nullptr)) {
        if (!m_port) return {};
        return m_port->queryProcesses(token);
    }

    bool terminateProcess(uint32_t pid) {
        if (Domain::ProcessTreeResolver::isProtectedPid(pid) || !m_port) {
            return false;
        }
        return m_port->terminateSingleProcess(pid);
    }

    bool terminateProcessTree(uint32_t pid);

    bool freezeProcess(uint32_t pid) {
        if (Domain::ProcessTreeResolver::isProtectedPid(pid) || !m_port) {
            return false;
        }
        return m_port->suspendProcess(pid);
    }

    bool resumeProcess(uint32_t pid) {
        if (Domain::ProcessTreeResolver::isProtectedPid(pid) || !m_port) {
            return false;
        }
        return m_port->resumeProcess(pid);
    }

    bool setPriority(uint32_t pid, int priorityClass) {
        if (Domain::ProcessTreeResolver::isProtectedPid(pid) || !m_port) {
            return false;
        }
        return m_port->setProcessPriority(pid, priorityClass);
    }

    bool pinToECores(uint32_t pid) {
        if (Domain::ProcessTreeResolver::isProtectedPid(pid) || !m_port) {
            return false;
        }
        uint64_t mask = m_port->getECoreMask();
        if (mask == 0) return false;
        m_port->setEcoQos(pid, true);
        return m_port->setProcessAffinity(pid, mask);
    }

    bool pinToPCores(uint32_t pid) {
        if (Domain::ProcessTreeResolver::isProtectedPid(pid) || !m_port) {
            return false;
        }
        uint64_t mask = m_port->getPCoreMask();
        if (mask == 0) return false;
        m_port->setEcoQos(pid, false);
        return m_port->setProcessAffinity(pid, mask);
    }

    bool resetAffinity(uint32_t pid) {
        if (Domain::ProcessTreeResolver::isProtectedPid(pid) || !m_port) {
            return false;
        }
        m_port->setEcoQos(pid, false);
        uint64_t allMask = m_port->getPCoreMask() | m_port->getECoreMask();
        return m_port->setProcessAffinity(pid, allMask);
    }

    bool hasHybridCores() const {
        return m_port ? m_port->hasHybridCores() : false;
    }

private:
    std::shared_ptr<Ports::IProcessPort> m_port;
};

} // namespace Rathon::UseCases
