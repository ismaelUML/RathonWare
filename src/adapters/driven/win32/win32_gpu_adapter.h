#pragma once

#include "ports/driven_ports.h"
#include <windows.h>
#include <dxgi1_4.h>
#include <unordered_map>
#include <vector>

namespace Rathon::Adapters::Driven {

struct VramHistory {
    std::vector<double> samplesMB;
    ULONGLONG firstSeenTime = 0;
    ULONGLONG lastSeenTime = 0;
};

class Win32GpuAdapter : public Ports::IGpuPort {
public:
    Win32GpuAdapter();
    ~Win32GpuAdapter() override;

    Domain::GpuTelemetry queryTelemetry() override;
    std::vector<Domain::GpuProcess> queryGpuProcesses() override;
    bool terminateGpuProcess(uint32_t pid) override;

private:
    void initNvml();
    void initDxgi();
    void queryNvmlTelemetry(Domain::GpuTelemetry& t);
    void queryDxgiTelemetry(Domain::GpuTelemetry& t);
    std::string decodeThrottleReason(uint64_t mask);

    // Helpers with CC <= 5
    void analyzeLeak(uint32_t pid, Domain::GpuProcess& proc, ULONGLONG now);
    std::string detectAiWorkload(const std::string& name);

    bool m_hasNvidiaGpu = false;
    bool m_dxgiInitialized = false;
    HMODULE m_nvmlLib = nullptr;
    void* m_nvmlDevice = nullptr;
    IDXGIFactory1* m_dxgiFactory = nullptr;
    IDXGIAdapter3* m_dxgiAdapter3 = nullptr;

    std::unordered_map<uint32_t, VramHistory> m_vramHistory;
};

} // namespace Rathon::Adapters::Driven
