#pragma once

#include "ports/driven_ports.h"
#include <string>
#include <vector>

namespace Rathon::Adapters::Driven {

class Win32FileUnlockerAdapter : public Ports::IFileUnlockerPort {
public:
    Win32FileUnlockerAdapter() = default;
    ~Win32FileUnlockerAdapter() override = default;

    std::vector<Domain::FileLock> queryLockingProcesses(const std::string& filePath) override;
    bool terminateLockingProcess(uint32_t pid) override;

private:
    std::wstring normalizeToNativePath(const std::string& path);
    std::string getProcessName(uint32_t pid);
    double getProcessMemoryMB(uint32_t pid);
    std::string getProcessCommandLine(uint32_t pid);
};

} // namespace Rathon::Adapters::Driven
