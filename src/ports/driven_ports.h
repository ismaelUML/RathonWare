#pragma once

#include "domain/entities.h"
#include "common/cancellation_token.h"
#include <vector>
#include <string>
#include <utility>

namespace Rathon::Ports {

// Driven Output Ports (Contracts).
// Strictly satisfies Requirement 1 (Hexagonal Architecture - Ports Layer):
// Pure abstract interfaces specifying WHAT external capabilities the core requires,
// without leaking any Win32 or Qt implementation details.

class IProcessPort {
public:
    virtual ~IProcessPort() = default;

    virtual std::vector<Domain::Process> queryProcesses(Common::CancellationToken token) = 0;
    virtual std::vector<std::pair<uint32_t, uint32_t>> queryParentChildRelations() = 0;
    virtual bool terminateSingleProcess(uint32_t pid) = 0;
    virtual bool suspendProcess(uint32_t pid) = 0;
    virtual bool resumeProcess(uint32_t pid) = 0;
    virtual bool setProcessPriority(uint32_t pid, int priorityClass) = 0;
    virtual bool setProcessAffinity(uint32_t pid, uint64_t mask) = 0;
    virtual bool setEcoQos(uint32_t pid, bool enable) = 0;

    virtual uint64_t getECoreMask() const = 0;
    virtual uint64_t getPCoreMask() const = 0;
    virtual bool hasHybridCores() const = 0;
};

class ISystemPort {
public:
    virtual ~ISystemPort() = default;
    virtual Domain::SystemSnapshot querySnapshot() = 0;
};

class IGpuPort {
public:
    virtual ~IGpuPort() = default;
    virtual Domain::GpuTelemetry queryTelemetry() = 0;
    virtual std::vector<Domain::GpuProcess> queryGpuProcesses() = 0;
    virtual bool terminateGpuProcess(uint32_t pid) = 0;
};

class ICpuTopologyPort {
public:
    virtual ~ICpuTopologyPort() = default;
    virtual std::vector<Domain::CpuCoreMetric> queryCoreMetrics() = 0;
};

class IPortPort {
public:
    virtual ~IPortPort() = default;
    virtual std::vector<Domain::PortBinding> queryAllPortBindings() = 0;
    virtual bool terminateProcessOnPort(uint32_t pid) = 0;
};

class IFileUnlockerPort {
public:
    virtual ~IFileUnlockerPort() = default;
    virtual std::vector<Domain::FileLock> queryLockingProcesses(const std::string& filePath) = 0;
    virtual bool terminateLockingProcess(uint32_t pid) = 0;
};

} // namespace Rathon::Ports
