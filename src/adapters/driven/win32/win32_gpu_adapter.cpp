#include "win32_gpu_adapter.h"
#include <tlhelp32.h>
#include <algorithm>
#include <sstream>

namespace Rathon::Adapters::Driven {

typedef int nvmlReturn_t;
typedef void* nvmlDevice_t;

struct nvmlUtilization_t {
    unsigned int gpu;
    unsigned int memory;
};

struct nvmlMemory_t {
    unsigned long long total;
    unsigned long long free;
    unsigned long long used;
};

struct nvmlProcessInfo_t {
    unsigned int pid;
    unsigned long long usedGpuMemory;
};

typedef nvmlReturn_t (*pfn_nvmlInit_v2)();
typedef nvmlReturn_t (*pfn_nvmlShutdown)();
typedef nvmlReturn_t (*pfn_nvmlDeviceGetHandleByIndex_v2)(unsigned int, nvmlDevice_t*);
typedef nvmlReturn_t (*pfn_nvmlDeviceGetName)(nvmlDevice_t, char*, unsigned int);
typedef nvmlReturn_t (*pfn_nvmlDeviceGetUtilizationRates)(nvmlDevice_t, nvmlUtilization_t*);
typedef nvmlReturn_t (*pfn_nvmlDeviceGetTemperature)(nvmlDevice_t, int, unsigned int*);
typedef nvmlReturn_t (*pfn_nvmlDeviceGetMemoryInfo)(nvmlDevice_t, nvmlMemory_t*);
typedef nvmlReturn_t (*pfn_nvmlDeviceGetPowerUsage)(nvmlDevice_t, unsigned int*);
typedef nvmlReturn_t (*pfn_nvmlDeviceGetEnforcedPowerLimit)(nvmlDevice_t, unsigned int*);
typedef nvmlReturn_t (*pfn_nvmlDeviceGetClockInfo)(nvmlDevice_t, int, unsigned int*);
typedef nvmlReturn_t (*pfn_nvmlDeviceGetCurrentClocksThrottleReasons)(nvmlDevice_t, unsigned long long*);
typedef nvmlReturn_t (*pfn_nvmlDeviceGetComputeRunningProcesses)(nvmlDevice_t, unsigned int*, nvmlProcessInfo_t*);
typedef nvmlReturn_t (*pfn_nvmlDeviceGetGraphicsRunningProcesses)(nvmlDevice_t, unsigned int*, nvmlProcessInfo_t*);

static pfn_nvmlInit_v2 nvmlInit = nullptr;
static pfn_nvmlShutdown nvmlShutdown = nullptr;
static pfn_nvmlDeviceGetHandleByIndex_v2 nvmlDeviceGetHandleByIndex = nullptr;
static pfn_nvmlDeviceGetName nvmlDeviceGetName = nullptr;
static pfn_nvmlDeviceGetUtilizationRates nvmlDeviceGetUtilizationRates = nullptr;
static pfn_nvmlDeviceGetTemperature nvmlDeviceGetTemperature = nullptr;
static pfn_nvmlDeviceGetMemoryInfo nvmlDeviceGetMemoryInfo = nullptr;
static pfn_nvmlDeviceGetPowerUsage nvmlDeviceGetPowerUsage = nullptr;
static pfn_nvmlDeviceGetEnforcedPowerLimit nvmlDeviceGetEnforcedPowerLimit = nullptr;
static pfn_nvmlDeviceGetClockInfo nvmlDeviceGetClockInfo = nullptr;
static pfn_nvmlDeviceGetCurrentClocksThrottleReasons nvmlDeviceGetCurrentClocksThrottleReasons = nullptr;
static pfn_nvmlDeviceGetComputeRunningProcesses nvmlDeviceGetComputeRunningProcesses = nullptr;
static pfn_nvmlDeviceGetGraphicsRunningProcesses nvmlDeviceGetGraphicsRunningProcesses = nullptr;

static std::string WideToUtf8(const wchar_t* wstr) {
    if (!wstr || !*wstr) return "";
    int len = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, nullptr, 0, nullptr, nullptr);
    if (len <= 1) return "";
    std::string out(len - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wstr, -1, out.data(), len, nullptr, nullptr);
    return out;
}

static std::string GetProcNameFromPid(uint32_t pid) {
    if (pid <= 4) return "System";
    std::string name = "Unknown";
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32W pe;
        pe.dwSize = sizeof(PROCESSENTRY32W);
        if (Process32FirstW(hSnapshot, &pe)) {
            do {
                if (pe.th32ProcessID == pid) {
                    name = WideToUtf8(pe.szExeFile);
                    break;
                }
            } while (Process32NextW(hSnapshot, &pe));
        }
        CloseHandle(hSnapshot);
    }
    return name;
}

Win32GpuAdapter::Win32GpuAdapter() {
    initNvml();
    if (!m_hasNvidiaGpu) {
        initDxgi();
    }
}

Win32GpuAdapter::~Win32GpuAdapter() {
    if (m_nvmlLib && nvmlShutdown) {
        nvmlShutdown();
        FreeLibrary(m_nvmlLib);
    }
    if (m_dxgiAdapter3) m_dxgiAdapter3->Release();
    if (m_dxgiFactory) m_dxgiFactory->Release();
}

void Win32GpuAdapter::initNvml() {
    m_nvmlLib = LoadLibraryW(L"nvml.dll");
    if (!m_nvmlLib) return;

    nvmlInit = reinterpret_cast<pfn_nvmlInit_v2>(GetProcAddress(m_nvmlLib, "nvmlInit_v2"));
    nvmlShutdown = reinterpret_cast<pfn_nvmlShutdown>(GetProcAddress(m_nvmlLib, "nvmlShutdown"));
    nvmlDeviceGetHandleByIndex = reinterpret_cast<pfn_nvmlDeviceGetHandleByIndex_v2>(GetProcAddress(m_nvmlLib, "nvmlDeviceGetHandleByIndex_v2"));
    nvmlDeviceGetName = reinterpret_cast<pfn_nvmlDeviceGetName>(GetProcAddress(m_nvmlLib, "nvmlDeviceGetName"));
    nvmlDeviceGetUtilizationRates = reinterpret_cast<pfn_nvmlDeviceGetUtilizationRates>(GetProcAddress(m_nvmlLib, "nvmlDeviceGetUtilizationRates"));
    nvmlDeviceGetTemperature = reinterpret_cast<pfn_nvmlDeviceGetTemperature>(GetProcAddress(m_nvmlLib, "nvmlDeviceGetTemperature"));
    nvmlDeviceGetMemoryInfo = reinterpret_cast<pfn_nvmlDeviceGetMemoryInfo>(GetProcAddress(m_nvmlLib, "nvmlDeviceGetMemoryInfo"));
    nvmlDeviceGetPowerUsage = reinterpret_cast<pfn_nvmlDeviceGetPowerUsage>(GetProcAddress(m_nvmlLib, "nvmlDeviceGetPowerUsage"));
    nvmlDeviceGetEnforcedPowerLimit = reinterpret_cast<pfn_nvmlDeviceGetEnforcedPowerLimit>(GetProcAddress(m_nvmlLib, "nvmlDeviceGetEnforcedPowerLimit"));
    nvmlDeviceGetClockInfo = reinterpret_cast<pfn_nvmlDeviceGetClockInfo>(GetProcAddress(m_nvmlLib, "nvmlDeviceGetClockInfo"));
    nvmlDeviceGetCurrentClocksThrottleReasons = reinterpret_cast<pfn_nvmlDeviceGetCurrentClocksThrottleReasons>(GetProcAddress(m_nvmlLib, "nvmlDeviceGetCurrentClocksThrottleReasons"));
    nvmlDeviceGetComputeRunningProcesses = reinterpret_cast<pfn_nvmlDeviceGetComputeRunningProcesses>(GetProcAddress(m_nvmlLib, "nvmlDeviceGetComputeRunningProcesses"));
    nvmlDeviceGetGraphicsRunningProcesses = reinterpret_cast<pfn_nvmlDeviceGetGraphicsRunningProcesses>(GetProcAddress(m_nvmlLib, "nvmlDeviceGetGraphicsRunningProcesses"));

    if (nvmlInit && nvmlInit() == 0 && nvmlDeviceGetHandleByIndex) {
        if (nvmlDeviceGetHandleByIndex(0, &m_nvmlDevice) == 0) {
            m_hasNvidiaGpu = true;
        }
    }
}

void Win32GpuAdapter::initDxgi() {
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

Domain::GpuTelemetry Win32GpuAdapter::queryTelemetry() {
    Domain::GpuTelemetry t;
    if (m_hasNvidiaGpu) {
        queryNvmlTelemetry(t);
    } else if (m_dxgiInitialized) {
        queryDxgiTelemetry(t);
    }
    return t;
}

void Win32GpuAdapter::queryNvmlTelemetry(Domain::GpuTelemetry& t) {
    t.hasGpu = true;
    t.hasNvidiaGpu = true;
    t.gpuBackend = "NVIDIA NVML";

    char nameBuf[128] = {0};
    if (nvmlDeviceGetName && nvmlDeviceGetName(m_nvmlDevice, nameBuf, sizeof(nameBuf)) == 0) {
        t.gpuName = nameBuf;
    }

    nvmlUtilization_t util{};
    if (nvmlDeviceGetUtilizationRates && nvmlDeviceGetUtilizationRates(m_nvmlDevice, &util) == 0) {
        t.gpuUsage = static_cast<double>(util.gpu);
    }

    unsigned int temp = 0;
    if (nvmlDeviceGetTemperature && nvmlDeviceGetTemperature(m_nvmlDevice, 0, &temp) == 0) {
        t.gpuTemp = static_cast<double>(temp);
    }

    nvmlMemory_t mem{};
    if (nvmlDeviceGetMemoryInfo && nvmlDeviceGetMemoryInfo(m_nvmlDevice, &mem) == 0) {
        t.vramTotalGB = mem.total / (1024.0 * 1024.0 * 1024.0);
        t.vramUsedGB = mem.used / (1024.0 * 1024.0 * 1024.0);
        t.vramFreeGB = mem.free / (1024.0 * 1024.0 * 1024.0);
        t.vramUsagePercent = (t.vramTotalGB > 0) ? (t.vramUsedGB / t.vramTotalGB) * 100.0 : 0.0;
    }

    unsigned int power = 0, powerLimit = 0;
    if (nvmlDeviceGetPowerUsage && nvmlDeviceGetPowerUsage(m_nvmlDevice, &power) == 0) {
        t.powerUsageW = power / 1000.0;
    }
    if (nvmlDeviceGetEnforcedPowerLimit && nvmlDeviceGetEnforcedPowerLimit(m_nvmlDevice, &powerLimit) == 0) {
        t.powerLimitW = powerLimit / 1000.0;
    }

    unsigned int gClock = 0, mClock = 0;
    if (nvmlDeviceGetClockInfo && nvmlDeviceGetClockInfo(m_nvmlDevice, 0, &gClock) == 0) {
        t.graphicsClockMHz = static_cast<int>(gClock);
    }
    if (nvmlDeviceGetClockInfo && nvmlDeviceGetClockInfo(m_nvmlDevice, 2, &mClock) == 0) {
        t.memoryClockMHz = static_cast<int>(mClock);
    }

    unsigned long long reasons = 0;
    if (nvmlDeviceGetCurrentClocksThrottleReasons && nvmlDeviceGetCurrentClocksThrottleReasons(m_nvmlDevice, &reasons) == 0) {
        t.throttleReason = decodeThrottleReason(reasons);
        t.throttleStatusLevel = (reasons == 0 || reasons == 0x0000000000000001ULL) ? "normal" : "warning";
    }
}

void Win32GpuAdapter::queryDxgiTelemetry(Domain::GpuTelemetry& t) {
    if (!m_dxgiAdapter3) return;

    DXGI_ADAPTER_DESC desc;
    if (SUCCEEDED(m_dxgiAdapter3->GetDesc(&desc))) {
        t.hasGpu = true;
        t.hasNvidiaGpu = false;
        t.gpuBackend = "DXGI 1.4";
        t.gpuName = WideToUtf8(desc.Description);
    }

    DXGI_QUERY_VIDEO_MEMORY_INFO memInfo{};
    if (SUCCEEDED(m_dxgiAdapter3->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &memInfo))) {
        t.vramTotalGB = memInfo.Budget / (1024.0 * 1024.0 * 1024.0);
        t.vramUsedGB = memInfo.CurrentUsage / (1024.0 * 1024.0 * 1024.0);
        t.vramFreeGB = (t.vramTotalGB > t.vramUsedGB) ? (t.vramTotalGB - t.vramUsedGB) : 0.0;
        t.vramUsagePercent = (t.vramTotalGB > 0) ? (t.vramUsedGB / t.vramTotalGB) * 100.0 : 0.0;
    }
}

std::string Win32GpuAdapter::decodeThrottleReason(uint64_t mask) {
    if (mask == 0 || mask == 0x0000000000000001ULL) return "None / Full Boost";
    if (mask & 0x0000000000000002ULL) return "GPU Idle";
    if (mask & 0x0000000000000004ULL) return "Thermal Cap Exceeded (Thermal Throttle)";
    if (mask & 0x0000000000000008ULL) return "Power Limit Reached";
    if (mask & 0x0000000000000080ULL) return "Hardware Slowdown (VRel/Current/Thermal)";
    return "Throttled (Bitmask: " + std::to_string(mask) + ")";
}

std::string Win32GpuAdapter::detectAiWorkload(const std::string& name) {
    std::string lower = name;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    if (lower.find("python") != std::string::npos || lower.find("ollama") != std::string::npos ||
        lower.find("vllm") != std::string::npos || lower.find("llama") != std::string::npos ||
        lower.find("torch") != std::string::npos) {
        return "CUDA Compute / AI";
    }
    return "DirectX / 3D App";
}

void Win32GpuAdapter::analyzeLeak(uint32_t pid, Domain::GpuProcess& proc, ULONGLONG now) {
    auto& hist = m_vramHistory[pid];
    hist.samplesMB.push_back(proc.vramMB);
    if (hist.samplesMB.size() > 60) {
        hist.samplesMB.erase(hist.samplesMB.begin()); // Bounded FIFO eviction
    }

    if (hist.firstSeenTime == 0) hist.firstSeenTime = now;
    hist.lastSeenTime = now;

    if (hist.samplesMB.size() >= 8) {
        double diffMB = hist.samplesMB.back() - hist.samplesMB.front();
        double elapsedMins = (now - hist.firstSeenTime) / 60000.0;
        if (diffMB > 50.0 && hist.samplesMB.front() > 0.0 && (diffMB / hist.samplesMB.front()) > 0.25 && elapsedMins > 0.2) {
            proc.leakSuspected = true;
            proc.growthRateMB = (elapsedMins > 0.0) ? (diffMB / elapsedMins) : 0.0;
        }
    }
}

std::vector<Domain::GpuProcess> Win32GpuAdapter::queryGpuProcesses() {
    std::vector<Domain::GpuProcess> procs;
    if (!m_hasNvidiaGpu) return procs;

    std::unordered_map<uint32_t, Domain::GpuProcess> procMap;

    unsigned int count = 64;
    std::vector<nvmlProcessInfo_t> compute(count);
    if (nvmlDeviceGetComputeRunningProcesses && nvmlDeviceGetComputeRunningProcesses(m_nvmlDevice, &count, compute.data()) == 0) {
        for (unsigned int i = 0; i < count; ++i) {
            Domain::GpuProcess p;
            p.pid = compute[i].pid;
            p.vramMB = compute[i].usedGpuMemory / (1024.0 * 1024.0);
            p.vramGB = p.vramMB / 1024.0;
            p.isCompute = true;
            procMap[p.pid] = p;
        }
    }

    count = 64;
    std::vector<nvmlProcessInfo_t> graphics(count);
    if (nvmlDeviceGetGraphicsRunningProcesses && nvmlDeviceGetGraphicsRunningProcesses(m_nvmlDevice, &count, graphics.data()) == 0) {
        for (unsigned int i = 0; i < count; ++i) {
            uint32_t pid = graphics[i].pid;
            double mb = graphics[i].usedGpuMemory / (1024.0 * 1024.0);
            if (procMap.find(pid) != procMap.end()) {
                procMap[pid].vramMB += mb;
                procMap[pid].vramGB = procMap[pid].vramMB / 1024.0;
            } else {
                Domain::GpuProcess p;
                p.pid = pid;
                p.vramMB = mb;
                p.vramGB = mb / 1024.0;
                procMap[pid] = p;
            }
        }
    }

    ULONGLONG now = GetTickCount64();
    for (auto& [pid, p] : procMap) {
        p.name = GetProcNameFromPid(pid);
        p.category = p.isCompute ? "CUDA Compute / AI" : detectAiWorkload(p.name);
        analyzeLeak(pid, p, now);
        procs.push_back(p);
    }

    // Purge dead PIDs from history
    for (auto it = m_vramHistory.begin(); it != m_vramHistory.end();) {
        if (procMap.find(it->first) == procMap.end()) {
            it = m_vramHistory.erase(it);
        } else {
            ++it;
        }
    }

    std::sort(procs.begin(), procs.end(), [](const Domain::GpuProcess& a, const Domain::GpuProcess& b) {
        return a.vramMB > b.vramMB;
    });

    return procs;
}

bool Win32GpuAdapter::terminateGpuProcess(uint32_t pid) {
    if (pid <= 4) return false;
    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
    if (!hProcess) return false;
    bool ok = TerminateProcess(hProcess, 1);
    CloseHandle(hProcess);
    return ok;
}

} // namespace Rathon::Adapters::Driven
