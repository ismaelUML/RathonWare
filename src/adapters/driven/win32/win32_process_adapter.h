#pragma once

#include "ports/driven_ports.h"
#include <windows.h>
#include <unordered_map>
#include <unordered_set>

namespace Rathon::Adapters::Driven {

struct ProcessTimeDelta {
    FILETIME kernelTime;
    FILETIME userTime;
    FILETIME lastQueryTime;
};

// Win32 & NT Kernel Process Driven Adapter.
// Implements Ports::IProcessPort with low-level Win32 APIs:
// - Toolhelp32 snapshot iteration
// - PEB traversal via ntdll!NtQueryInformationProcess and ReadProcessMemory
// - Atomic process freeze via ntdll!NtSuspendProcess
// - EcoQoS throttling and hybrid core affinity
class Win32ProcessAdapter : public Ports::IProcessPort {
public:
    Win32ProcessAdapter();
    ~Win32ProcessAdapter() override = default;

    std::vector<Domain::Process> queryProcesses(Common::CancellationToken token) override;
    std::vector<std::pair<uint32_t, uint32_t>> queryParentChildRelations() override;
    bool terminateSingleProcess(uint32_t pid) override;
    bool suspendProcess(uint32_t pid) override;
    bool resumeProcess(uint32_t pid) override;
    bool setProcessPriority(uint32_t pid, int priorityClass) override;
    bool setProcessAffinity(uint32_t pid, uint64_t mask) override;
    bool setEcoQos(uint32_t pid, bool enable) override;

    uint64_t getECoreMask() const override { return m_eCoreMask; }
    uint64_t getPCoreMask() const override { return m_pCoreMask; }
    bool hasHybridCores() const override { return m_hasHybridCores; }

private:
    void initCpuTopology();
    void parseProcessorCoreRelation(const BYTE* ptr, DWORD size);
    void applyUniformTopologyFallback();

    // Decomposed single-responsibility helpers with Cyclomatic Complexity <= 5
    bool populateProcessDetails(uint32_t pid, Domain::Process& info);
    void queryMemoryCounters(HANDLE hProcess, Domain::Process& info);
    void calculateCpuPercentage(HANDLE hProcess, uint32_t pid, Domain::Process& info);
    void queryAffinityAndEco(HANDLE hProcess, Domain::Process& info);
    std::string readCommandLine(HANDLE hProcess);
    std::string readUsername(HANDLE hProcess);

    int m_numCores = 1;
    uint64_t m_eCoreMask = 0;
    uint64_t m_pCoreMask = 0;
    uint64_t m_allCoresMask = 0;
    bool m_hasHybridCores = false;

    std::unordered_map<uint32_t, ProcessTimeDelta> m_history;
    std::unordered_set<uint32_t> m_suspendedPids;
};

} // namespace Rathon::Adapters::Driven
