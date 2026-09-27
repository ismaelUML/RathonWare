#include "win32_cpu_adapter.h"
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace Rathon::Adapters::Driven {

typedef struct _SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION {
    LARGE_INTEGER IdleTime;
    LARGE_INTEGER KernelTime;
    LARGE_INTEGER UserTime;
    LARGE_INTEGER DpcTime;
    LARGE_INTEGER InterruptTime;
    ULONG InterruptCount;
} SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION;

typedef NTSTATUS(NTAPI* pfnNtQuerySystemInformation)(
    ULONG SystemInformationClass,
    PVOID SystemInformation,
    ULONG SystemInformationLength,
    PULONG ReturnLength
);

Win32CpuAdapter::Win32CpuAdapter() {
    initTopology();
}

void Win32CpuAdapter::initTopology() {
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    m_coreCount = sysInfo.dwNumberOfProcessors > 0 ? sysInfo.dwNumberOfProcessors : 1;

    DWORD length = 0;
    GetLogicalProcessorInformationEx(RelationProcessorCore, nullptr, &length);
    if (length > 0) {
        std::vector<BYTE> buffer(length);
        auto info = reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(buffer.data());
        if (GetLogicalProcessorInformationEx(RelationProcessorCore, info, &length)) {
            BYTE* ptr = buffer.data();
            while (ptr < buffer.data() + length) {
                auto item = reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(ptr);
                if (item->Relationship == RelationProcessorCore) {
                    KAFFINITY mask = item->Processor.GroupMask[0].Mask;
                    BYTE eff = reinterpret_cast<const BYTE*>(&item->Processor)[1];
                    if (eff > 0) m_pCoreMask |= mask;
                    else m_eCoreMask |= mask;
                }
                ptr += item->Size;
            }
        }
    }

    if (m_pCoreMask != 0 && m_eCoreMask != 0) {
        m_hasHybridCores = true;
    }

    m_staticTopology.resize(m_coreCount);
    for (int i = 0; i < m_coreCount; ++i) {
        Domain::CpuCoreMetric c;
        c.coreIndex = i;
        uint64_t mask = (1ULL << i);
        if (m_hasHybridCores) {
            c.isPCore = (m_pCoreMask & mask) != 0;
            c.isECore = (m_eCoreMask & mask) != 0;
            c.coreType = c.isPCore ? "P-Core" : (c.isECore ? "E-Core" : "Core");
            c.coreLabel = (c.isPCore ? "P#" : "E#") + std::to_string(i);
        } else {
            c.coreType = "Core";
            c.coreLabel = "CPU " + std::to_string(i);
        }
        c.heatColor = "#2b6cb0";
        m_staticTopology[i] = c;
    }

    m_prevSamples.resize(m_coreCount);
}

std::string Win32CpuAdapter::calculateHeatColor(double load) {
    int r = 0, g = 0, b = 0;
    if (load <= 50.0) {
        double ratio = load / 50.0;
        r = static_cast<int>(43 + ratio * (217 - 43));
        g = static_cast<int>(108 + ratio * (119 - 108));
        b = static_cast<int>(176 - ratio * 170);
    } else {
        double ratio = (load - 50.0) / 50.0;
        r = static_cast<int>(217 - ratio * 19);
        g = static_cast<int>(119 - ratio * 79);
        b = static_cast<int>(6 + ratio * 34);
    }

    std::ostringstream ss;
    ss << "#" << std::hex << std::setfill('0')
       << std::setw(2) << std::clamp(r, 0, 255)
       << std::setw(2) << std::clamp(g, 0, 255)
       << std::setw(2) << std::clamp(b, 0, 255);
    return ss.str();
}

void Win32CpuAdapter::sampleKernelTimes(std::vector<Domain::CpuCoreMetric>& metrics) {
    static auto NtQuery = reinterpret_cast<pfnNtQuerySystemInformation>(
        GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "NtQuerySystemInformation")
    );
    if (!NtQuery) return;

    std::vector<SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION> perf(m_coreCount);
    ULONG retLen = 0;
    // SystemProcessorPerformanceInformation = 8
    if (NtQuery(8, perf.data(), sizeof(SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION) * m_coreCount, &retLen) != 0) {
        return;
    }

    for (int i = 0; i < m_coreCount; ++i) {
        auto& prev = m_prevSamples[i];
        ULONGLONG idle = perf[i].IdleTime.QuadPart - prev.idleTime.QuadPart;
        ULONGLONG kernel = perf[i].KernelTime.QuadPart - prev.kernelTime.QuadPart;
        ULONGLONG user = perf[i].UserTime.QuadPart - prev.userTime.QuadPart;
        ULONGLONG total = kernel + user;

        if (total > 0) {
            double load = (static_cast<double>(total - idle) / static_cast<double>(total)) * 100.0;
            metrics[i].load = std::clamp(load, 0.0, 100.0);
            metrics[i].heatColor = calculateHeatColor(metrics[i].load);
        }

        prev.idleTime = perf[i].IdleTime;
        prev.kernelTime = perf[i].KernelTime;
        prev.userTime = perf[i].UserTime;
    }
}

std::vector<Domain::CpuCoreMetric> Win32CpuAdapter::queryCoreMetrics() {
    auto current = m_staticTopology;
    sampleKernelTimes(current);
    return current;
}

} // namespace Rathon::Adapters::Driven
