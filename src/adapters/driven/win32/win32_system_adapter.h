#pragma once

#include "ports/driven_ports.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iphlpapi.h>
#include <netioapi.h>
#include <pdh.h>
#include <psapi.h>
#include <dxgi1_4.h>

namespace Rathon::Adapters::Driven {

// Win32 System Monitor Driven Adapter.
// Gathers low-level Windows kernel metrics, PDH disk performance, and network deltas.
class Win32SystemAdapter : public Ports::ISystemPort {
public:
    Win32SystemAdapter();
    ~Win32SystemAdapter() override;

    Domain::SystemSnapshot querySnapshot() override;

private:
    void initPdhQueries();
    void queryCpuUsage(Domain::SystemSnapshot& s);
    void queryRamMetrics(Domain::SystemSnapshot& s);
    void queryDiskMetrics(Domain::SystemSnapshot& s);
    void queryNetworkMetrics(Domain::SystemSnapshot& s);
    void queryKernelMetrics(Domain::SystemSnapshot& s);
    void queryHardwareSpecs(Domain::SystemSnapshot& s);

    // Helpers with CC <= 5
    void queryCpuSpec(Domain::SystemSnapshot& s);
    void queryGpuSpec(Domain::SystemSnapshot& s);
    void queryMotherboardSpec(Domain::SystemSnapshot& s);

    FILETIME m_prevIdleTime{};
    FILETIME m_prevKernelTime{};
    FILETIME m_prevUserTime{};

    ULONGLONG m_prevNetInBytes = 0;
    ULONGLONG m_prevNetOutBytes = 0;
    ULONGLONG m_lastNetQueryTime = 0;

    PDH_HQUERY m_pdhQuery = nullptr;
    PDH_HCOUNTER m_counterDiskRead = nullptr;
    PDH_HCOUNTER m_counterDiskWrite = nullptr;
    bool m_pdhInitialized = false;

    IDXGIFactory1* m_dxgiFactory = nullptr;
    IDXGIAdapter3* m_dxgiAdapter3 = nullptr;
    bool m_dxgiInitialized = false;

    // Cached hardware specs
    Domain::SystemSnapshot m_cachedSpecs;
    bool m_specsQueried = false;
};

} // namespace Rathon::Adapters::Driven
