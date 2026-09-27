#pragma once

#include "ports/driven_ports.h"
#include "domain/process_tree.h"
#include <memory>
#include <vector>
#include <string>

namespace Rathon::UseCases {

class FileUnlockerUseCases {
public:
    explicit FileUnlockerUseCases(std::shared_ptr<Ports::IFileUnlockerPort> port)
        : m_port(std::move(port))
    {
    }

    std::vector<Domain::FileLock> findLockingProcesses(const std::string& filePath) {
        if (filePath.empty() || !m_port) {
            return {};
        }
        return m_port->queryLockingProcesses(filePath);
    }

    bool unlockFile(const std::string& filePath) {
        auto lockers = findLockingProcesses(filePath);
        if (lockers.empty()) return true;

        bool allKilled = true;
        for (const auto& locker : lockers) {
            if (Domain::ProcessTreeResolver::isProtectedPid(locker.pid)) {
                continue;
            }
            if (!m_port->terminateLockingProcess(locker.pid)) {
                allKilled = false;
            }
        }
        return allKilled;
    }

    bool killLockingProcess(uint32_t pid) {
        if (Domain::ProcessTreeResolver::isProtectedPid(pid) || !m_port) {
            return false;
        }
        return m_port->terminateLockingProcess(pid);
    }

private:
    std::shared_ptr<Ports::IFileUnlockerPort> m_port;
};

} // namespace Rathon::UseCases
