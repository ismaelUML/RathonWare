#include "win32_system_adapter.h"
#include <vector>
#include <string>
#include <sstream>
#include <iomanip>

namespace Rathon::Adapters::Driven {

static ULONGLONG FileTimeToUlong(const FILETIME& ft) {
    ULARGE_INTEGER u;
    u.LowPart = ft.dwLowDateTime;
    u.HighPart = ft.dwHighDateTime;
    return u.QuadPart;
}

static std::string WideToUtf8(const wchar_t* wstr) {
    if (!wstr || !*wstr) return "";
    int len = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, nullptr, 0, nullptr, nullptr);
    if (len <= 1) return "";
    std::string out(len - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wstr, -1, out.data(), len, nullptr, nullptr);
    return out;
}

Win32SystemAdapter::Win32SystemAdapter() {
    GetSystemTimes(&m_prevIdleTime, &m_prevKernelTime, &m_prevUserTime);
    initPdhQueries();

    if (SUCCEEDED(CreateDXGIFactory1(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(&m_dxgiFactory)))) {
        IDXGIAdapter1* adapter = nullptr;
        if (SUCCEEDED(m_dxgiFactory->EnumAdapters1(0, &adapter))) {
            if (SUCCEEDED(adapter->QueryInterface(__uuidof(IDXGIAdapter3), reinterpret_cast<void**>(&m_dxgiAdapter3)))) {
                m_dxgiInitialized = true;
            }
            adapter->Release();
        }
    }
}

Win32SystemAdapter::~Win32SystemAdapter() {
    if (m_pdhInitialized && m_pdhQuery) {
        PdhCloseQuery(m_pdhQuery);
    }
    if (m_dxgiAdapter3) m_dxgiAdapter3->Release();
    if (m_dxgiFactory) m_dxgiFactory->Release();
}

void Win32SystemAdapter::initPdhQueries() {
    if (PdhOpenQueryW(nullptr, 0, &m_pdhQuery) == ERROR_SUCCESS) {
        PDH_STATUS s1 = PdhAddCounterW(m_pdhQuery, L"\\PhysicalDisk(_Total)\\Disk Read Bytes/sec", 0, &m_counterDiskRead);
        PDH_STATUS s2 = PdhAddCounterW(m_pdhQuery, L"\\PhysicalDisk(_Total)\\Disk Write Bytes/sec", 0, &m_counterDiskWrite);
        if (s1 == ERROR_SUCCESS && s2 == ERROR_SUCCESS) {
            PdhCollectQueryData(m_pdhQuery);
            m_pdhInitialized = true;
        }
    }
}

Domain::SystemSnapshot Win32SystemAdapter::querySnapshot() {
    Domain::SystemSnapshot s;

    queryCpuUsage(s);
    queryRamMetrics(s);
    queryDiskMetrics(s);
    queryNetworkMetrics(s);
    queryKernelMetrics(s);
    queryHardwareSpecs(s);

    // Compute system uptime
    ULONGLONG upMs = GetTickCount64();
    uint64_t totalSec = upMs / 1000;
    uint32_t hours = static_cast<uint32_t>(totalSec / 3600);
    uint32_t mins = static_cast<uint32_t>((totalSec % 3600) / 60);
    uint32_t secs = static_cast<uint32_t>(totalSec % 60);

    std::ostringstream ss;
    ss << std::setfill('0') << std::setw(2) << hours << ":"
       << std::setfill('0') << std::setw(2) << mins << ":"
       << std::setfill('0') << std::setw(2) << secs;
    s.uptime = ss.str();

    return s;
}

void Win32SystemAdapter::queryCpuUsage(Domain::SystemSnapshot& s) {
    FILETIME idleTime, kernelTime, userTime;
    if (!GetSystemTimes(&idleTime, &kernelTime, &userTime)) return;

    ULONGLONG idle = FileTimeToUlong(idleTime) - FileTimeToUlong(m_prevIdleTime);
    ULONGLONG kernel = FileTimeToUlong(kernelTime) - FileTimeToUlong(m_prevKernelTime);
    ULONGLONG user = FileTimeToUlong(userTime) - FileTimeToUlong(m_prevUserTime);
    ULONGLONG total = kernel + user;

    if (total > 0) {
        double cpu = (static_cast<double>(total - idle) / static_cast<double>(total)) * 100.0;
        s.cpuUsage = (cpu < 0.0) ? 0.0 : ((cpu > 100.0) ? 100.0 : cpu);
    }

    m_prevIdleTime = idleTime;
    m_prevKernelTime = kernelTime;
    m_prevUserTime = userTime;
}

void Win32SystemAdapter::queryRamMetrics(Domain::SystemSnapshot& s) {
    MEMORYSTATUSEX mem{};
    mem.dwLength = sizeof(mem);
    if (GlobalMemoryStatusEx(&mem)) {
        s.ramTotal = mem.ullTotalPhys / (1024.0 * 1024.0 * 1024.0);
        s.ramUsed = (mem.ullTotalPhys - mem.ullAvailPhys) / (1024.0 * 1024.0 * 1024.0);
        s.ramUsage = static_cast<double>(mem.dwMemoryLoad);
    }
}

void Win32SystemAdapter::queryDiskMetrics(Domain::SystemSnapshot& s) {
    if (m_pdhInitialized && m_pdhQuery) {
        if (PdhCollectQueryData(m_pdhQuery) == ERROR_SUCCESS) {
            PDH_FMT_COUNTERVALUE valRead, valWrite;
            if (PdhGetFormattedCounterValue(m_counterDiskRead, PDH_FMT_DOUBLE, nullptr, &valRead) == ERROR_SUCCESS) {
                s.diskReadSpeed = valRead.doubleValue / (1024.0 * 1024.0);
            }
            if (PdhGetFormattedCounterValue(m_counterDiskWrite, PDH_FMT_DOUBLE, nullptr, &valWrite) == ERROR_SUCCESS) {
                s.diskWriteSpeed = valWrite.doubleValue / (1024.0 * 1024.0);
            }
        }
    }

    ULARGE_INTEGER freeBytes, totalBytes, totalFree;
    if (GetDiskFreeSpaceExW(L"C:\\", &freeBytes, &totalBytes, &totalFree)) {
        s.diskTotalCapacityGB = totalBytes.QuadPart / (1024.0 * 1024.0 * 1024.0);
        s.diskTotalFreeGB = totalFree.QuadPart / (1024.0 * 1024.0 * 1024.0);
        double used = s.diskTotalCapacityGB - s.diskTotalFreeGB;
        s.diskUsage = (s.diskTotalCapacityGB > 0) ? (used / s.diskTotalCapacityGB) * 100.0 : 0.0;
        s.diskDriveSummary = "C: " + std::to_string(static_cast<int>(s.diskUsage)) + "%";
    }
}

void Win32SystemAdapter::queryNetworkMetrics(Domain::SystemSnapshot& s) {
    PMIB_IF_TABLE2 pTable = nullptr;
    if (GetIfTable2(&pTable) != NO_ERROR || !pTable) return;

    ULONGLONG currentIn = 0, currentOut = 0;
    for (ULONG i = 0; i < pTable->NumEntries; ++i) {
        const MIB_IF_ROW2& row = pTable->Table[i];
        if (row.InterfaceAndOperStatusFlags.HardwareInterface && row.OperStatus == IfOperStatusUp) {
            currentIn += row.InOctets;
            currentOut += row.OutOctets;
        }
    }
    FreeMibTable(pTable);

    ULONGLONG now = GetTickCount64();
    if (m_lastNetQueryTime > 0 && now > m_lastNetQueryTime) {
        double elapsedSec = (now - m_lastNetQueryTime) / 1000.0;
        if (elapsedSec > 0.0) {
            s.netDownloadSpeed = ((currentIn - m_prevNetInBytes) / (1024.0 * 1024.0)) / elapsedSec;
            s.netUploadSpeed = ((currentOut - m_prevNetOutBytes) / (1024.0 * 1024.0)) / elapsedSec;
        }
    }

    m_prevNetInBytes = currentIn;
    m_prevNetOutBytes = currentOut;
    m_lastNetQueryTime = now;
}

void Win32SystemAdapter::queryKernelMetrics(Domain::SystemSnapshot& s) {
    PERFORMANCE_INFORMATION pi{};
    pi.cb = sizeof(pi);
    if (GetPerformanceInfo(&pi, sizeof(pi))) {
        double pageSizeGB = pi.PageSize / (1024.0 * 1024.0 * 1024.0);
        s.commitTotalGB = pi.CommitTotal * pageSizeGB;
        s.commitLimitGB = pi.CommitLimit * pageSizeGB;
        s.commitPeakGB = pi.CommitPeak * pageSizeGB;
        s.commitUsagePercent = (s.commitLimitGB > 0) ? (s.commitTotalGB / s.commitLimitGB) * 100.0 : 0.0;

        s.kernelPagedMB = (pi.KernelPaged * pi.PageSize) / (1024.0 * 1024.0);
        s.kernelNonpagedMB = (pi.KernelNonpaged * pi.PageSize) / (1024.0 * 1024.0);
        s.processCount = static_cast<int>(pi.ProcessCount);
        s.threadCount = static_cast<int>(pi.ThreadCount);
        s.handleCount = static_cast<int>(pi.HandleCount);
    }
}

void Win32SystemAdapter::queryHardwareSpecs(Domain::SystemSnapshot& s) {
    if (!m_specsQueried) {
        queryCpuSpec(m_cachedSpecs);
        queryGpuSpec(m_cachedSpecs);
        queryMotherboardSpec(m_cachedSpecs);
        m_specsQueried = true;
    }

    s.cpuModel = m_cachedSpecs.cpuModel;
    s.cpuBaseClockMHz = m_cachedSpecs.cpuBaseClockMHz;
    s.gpuModel = m_cachedSpecs.gpuModel;
    s.gpuBackend = m_cachedSpecs.gpuBackend;
    s.motherboardModel = m_cachedSpecs.motherboardModel;
    s.biosVersion = m_cachedSpecs.biosVersion;
    s.ramSpeed = m_cachedSpecs.ramSpeed;
}

void Win32SystemAdapter::queryCpuSpec(Domain::SystemSnapshot& s) {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        wchar_t buf[256] = {0};
        DWORD sz = sizeof(buf);
        if (RegQueryValueExW(hKey, L"ProcessorNameString", nullptr, nullptr, reinterpret_cast<LPBYTE>(buf), &sz) == ERROR_SUCCESS) {
            s.cpuModel = WideToUtf8(buf);
        }
        DWORD mhz = 0;
        sz = sizeof(mhz);
        if (RegQueryValueExW(hKey, L"~MHz", nullptr, nullptr, reinterpret_cast<LPBYTE>(&mhz), &sz) == ERROR_SUCCESS) {
            s.cpuBaseClockMHz = static_cast<int>(mhz);
        }
        RegCloseKey(hKey);
    }
}

void Win32SystemAdapter::queryGpuSpec(Domain::SystemSnapshot& s) {
    if (m_dxgiInitialized && m_dxgiAdapter3) {
        DXGI_ADAPTER_DESC desc;
        if (SUCCEEDED(m_dxgiAdapter3->GetDesc(&desc))) {
            s.gpuModel = WideToUtf8(desc.Description);
            s.gpuBackend = "DXGI 1.4";
        }
    }
}

void Win32SystemAdapter::queryMotherboardSpec(Domain::SystemSnapshot& s) {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"HARDWARE\\DESCRIPTION\\System\\BIOS", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        wchar_t prod[256] = {0}, bios[256] = {0};
        DWORD sz = sizeof(prod);
        if (RegQueryValueExW(hKey, L"BaseBoardProduct", nullptr, nullptr, reinterpret_cast<LPBYTE>(prod), &sz) == ERROR_SUCCESS) {
            s.motherboardModel = WideToUtf8(prod);
        }
        sz = sizeof(bios);
        if (RegQueryValueExW(hKey, L"BIOSVersion", nullptr, nullptr, reinterpret_cast<LPBYTE>(bios), &sz) == ERROR_SUCCESS) {
            s.biosVersion = WideToUtf8(bios);
        }
        RegCloseKey(hKey);
    }
}

} // namespace Rathon::Adapters::Driven
