#include "win32_process_adapter.h"
#include <tlhelp32.h>
#include <psapi.h>
#include <processthreadsapi.h>
#include <vector>
#include <string>

namespace Rathon::Adapters::Driven {

// NT internal prototypes
typedef NTSTATUS(NTAPI* pfnNtQueryInformationProcess)(
    HANDLE ProcessHandle,
    DWORD ProcessInformationClass,
    PVOID ProcessInformation,
    ULONG ProcessInformationLength,
    PULONG ReturnLength
);

typedef NTSTATUS(NTAPI* pfnNtSuspendProcess)(HANDLE ProcessHandle);
typedef NTSTATUS(NTAPI* pfnNtResumeProcess)(HANDLE ProcessHandle);

static pfnNtSuspendProcess g_NtSuspendProcess = nullptr;
static pfnNtResumeProcess g_NtResumeProcess = nullptr;

struct PROCESS_BASIC_INFORMATION {
    NTSTATUS ExitStatus;
    PVOID PebBaseAddress;
    ULONG_PTR AffinityMask;
    LONG BasePriority;
    ULONG_PTR UniqueProcessId;
    ULONG_PTR InheritedFromUniqueProcessId;
};

struct UNICODE_STRING {
    USHORT Length;
    USHORT MaximumLength;
    PWSTR Buffer;
};

struct RTL_USER_PROCESS_PARAMETERS {
    BYTE Reserved1[16];
    PVOID Reserved2[10];
    UNICODE_STRING ImagePathName;
    UNICODE_STRING CommandLine;
};

struct PEB {
    BYTE Reserved1[2];
    BYTE BeingDebugged;
    BYTE Reserved2[1];
    PVOID Reserved3[2];
    PVOID Ldr;
    RTL_USER_PROCESS_PARAMETERS* ProcessParameters;
};

static ULONGLONG SubtractFileTime(const FILETIME& ftA, const FILETIME& ftB) {
    ULARGE_INTEGER a, b;
    a.LowPart = ftA.dwLowDateTime;
    a.HighPart = ftA.dwHighDateTime;
    b.LowPart = ftB.dwLowDateTime;
    b.HighPart = ftB.dwHighDateTime;
    return a.QuadPart - b.QuadPart;
}

static std::string WideToUtf8(const wchar_t* wstr) {
    if (!wstr || !*wstr) return "";
    int len = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, nullptr, 0, nullptr, nullptr);
    if (len <= 1) return "";
    std::string out(len - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wstr, -1, out.data(), len, nullptr, nullptr);
    return out;
}

static std::string GetPriorityString(HANDLE hProcess) {
    DWORD pc = GetPriorityClass(hProcess);
    switch (pc) {
        case IDLE_PRIORITY_CLASS: return "Idle";
        case BELOW_NORMAL_PRIORITY_CLASS: return "Below Normal";
        case NORMAL_PRIORITY_CLASS: return "Normal";
        case ABOVE_NORMAL_PRIORITY_CLASS: return "Above Normal";
        case HIGH_PRIORITY_CLASS: return "High";
        case REALTIME_PRIORITY_CLASS: return "Realtime";
        default: return "Normal";
    }
}

Win32ProcessAdapter::Win32ProcessAdapter() {
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    m_numCores = sysInfo.dwNumberOfProcessors > 0 ? sysInfo.dwNumberOfProcessors : 1;

    HMODULE hNtDll = GetModuleHandleW(L"ntdll.dll");
    if (hNtDll) {
        g_NtSuspendProcess = reinterpret_cast<pfnNtSuspendProcess>(GetProcAddress(hNtDll, "NtSuspendProcess"));
        g_NtResumeProcess = reinterpret_cast<pfnNtResumeProcess>(GetProcAddress(hNtDll, "NtResumeProcess"));
    }

    initCpuTopology();
}

void Win32ProcessAdapter::initCpuTopology() {
    DWORD length = 0;
    GetLogicalProcessorInformationEx(RelationProcessorCore, nullptr, &length);
    if (length == 0) {
        applyUniformTopologyFallback();
        return;
    }

    std::vector<BYTE> buffer(length);
    auto info = reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(buffer.data());
    if (GetLogicalProcessorInformationEx(RelationProcessorCore, info, &length)) {
        BYTE* ptr = buffer.data();
        while (ptr < buffer.data() + length) {
            auto item = reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(ptr);
            if (item->Relationship == RelationProcessorCore) {
                parseProcessorCoreRelation(ptr, item->Size);
            }
            ptr += item->Size;
        }
    }

    if (m_pCoreMask != 0 && m_eCoreMask != 0) {
        m_hasHybridCores = true;
    } else {
        applyUniformTopologyFallback();
    }
}

void Win32ProcessAdapter::parseProcessorCoreRelation(const BYTE* ptr, DWORD) {
    auto item = reinterpret_cast<const SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*>(ptr);
    KAFFINITY mask = item->Processor.GroupMask[0].Mask;
    BYTE efficiencyClass = reinterpret_cast<const BYTE*>(&item->Processor)[1];

    if (efficiencyClass > 0) {
        m_pCoreMask |= mask;
    } else {
        m_eCoreMask |= mask;
    }
    m_allCoresMask |= mask;
}

void Win32ProcessAdapter::applyUniformTopologyFallback() {
    if (m_allCoresMask == 0) {
        m_allCoresMask = (m_numCores >= 64) ? ~0ULL : ((1ULL << m_numCores) - 1);
    }
    int half = m_numCores / 2;
    if (half >= 1) {
        m_pCoreMask = (1ULL << half) - 1;
        m_eCoreMask = m_allCoresMask & ~m_pCoreMask;
    } else {
        m_pCoreMask = m_allCoresMask;
        m_eCoreMask = m_allCoresMask;
    }
}

std::vector<Domain::Process> Win32ProcessAdapter::queryProcesses(Common::CancellationToken token) {
    std::vector<Domain::Process> list;
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) {
        return list;
    }

    PROCESSENTRY32W pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32W);

    if (Process32FirstW(hSnapshot, &pe32)) {
        do {
            if (token.isCancelled()) break;
            if (pe32.th32ProcessID <= 4) continue; // Skip Idle and System

            Domain::Process proc;
            proc.pid = pe32.th32ProcessID;
            proc.parentPid = pe32.th32ParentProcessID;
            proc.name = WideToUtf8(pe32.szExeFile);
            proc.threads = static_cast<int>(pe32.cntThreads);
            proc.isSuspended = m_suspendedPids.contains(proc.pid);

            populateProcessDetails(proc.pid, proc);
            list.push_back(std::move(proc));
        } while (Process32NextW(hSnapshot, &pe32));
    }

    CloseHandle(hSnapshot);
    return list;
}

bool Win32ProcessAdapter::populateProcessDetails(uint32_t pid, Domain::Process& info) {
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (!hProcess) {
        hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    }
    if (!hProcess) {
        return false;
    }

    queryMemoryCounters(hProcess, info);
    calculateCpuPercentage(hProcess, pid, info);
    queryAffinityAndEco(hProcess, info);
    info.cmdLine = readCommandLine(hProcess);
    info.username = readUsername(hProcess);
    info.priority = GetPriorityString(hProcess);

    CloseHandle(hProcess);
    return true;
}

void Win32ProcessAdapter::queryMemoryCounters(HANDLE hProcess, Domain::Process& info) {
    PROCESS_MEMORY_COUNTERS_EX pmc;
    if (GetProcessMemoryInfo(hProcess, reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc), sizeof(pmc))) {
        info.ramUsageMB = pmc.WorkingSetSize / (1024.0 * 1024.0);
        info.privateUsageMB = pmc.PrivateUsage / (1024.0 * 1024.0);
        info.peakUsageMB = pmc.PeakWorkingSetSize / (1024.0 * 1024.0);
    }
}

void Win32ProcessAdapter::calculateCpuPercentage(HANDLE hProcess, uint32_t pid, Domain::Process& info) {
    FILETIME creationTime, exitTime, kernelTime, userTime;
    if (!GetProcessTimes(hProcess, &creationTime, &exitTime, &kernelTime, &userTime)) {
        return;
    }

    FILETIME nowFt;
    GetSystemTimeAsFileTime(&nowFt);

    auto it = m_history.find(pid);
    if (it != m_history.end()) {
        ULONGLONG kDiff = SubtractFileTime(kernelTime, it->second.kernelTime);
        ULONGLONG uDiff = SubtractFileTime(userTime, it->second.userTime);
        ULONGLONG wallDiff = SubtractFileTime(nowFt, it->second.lastQueryTime);

        if (wallDiff > 0 && m_numCores > 0) {
            double rawPercent = (static_cast<double>(kDiff + uDiff) / static_cast<double>(wallDiff * m_numCores)) * 100.0;
            if (rawPercent < 0.0) rawPercent = 0.0;
            if (rawPercent > 100.0) rawPercent = 100.0;
            info.cpuUsage = rawPercent;
        }
    }

    m_history[pid] = {kernelTime, userTime, nowFt};
}

void Win32ProcessAdapter::queryAffinityAndEco(HANDLE hProcess, Domain::Process& info) {
    DWORD_PTR procMask = 0, sysMask = 0;
    if (GetProcessAffinityMask(hProcess, &procMask, &sysMask)) {
        info.affinityMask = static_cast<uint64_t>(procMask);
        if (m_eCoreMask != 0 && (info.affinityMask & ~m_eCoreMask) == 0) {
            info.isEcoQos = true;
        }
    }
}

std::string Win32ProcessAdapter::readCommandLine(HANDLE hProcess) {
    static auto NtQuery = reinterpret_cast<pfnNtQueryInformationProcess>(
        GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "NtQueryInformationProcess")
    );
    if (!NtQuery) return "N/A";

    PROCESS_BASIC_INFORMATION pbi{};
    ULONG retLen = 0;
    if (NtQuery(hProcess, 0, &pbi, sizeof(pbi), &retLen) != 0 || !pbi.PebBaseAddress) {
        wchar_t path[MAX_PATH] = {0};
        DWORD sz = MAX_PATH;
        if (QueryFullProcessImageNameW(hProcess, 0, path, &sz)) {
            return WideToUtf8(path);
        }
        return "N/A";
    }

    PEB peb;
    SIZE_T read = 0;
    if (!ReadProcessMemory(hProcess, pbi.PebBaseAddress, &peb, sizeof(peb), &read)) return "N/A";

    RTL_USER_PROCESS_PARAMETERS params;
    if (!ReadProcessMemory(hProcess, peb.ProcessParameters, &params, sizeof(params), &read)) return "N/A";

    std::vector<wchar_t> buf(params.CommandLine.Length / sizeof(wchar_t) + 1, 0);
    if (!ReadProcessMemory(hProcess, params.CommandLine.Buffer, buf.data(), params.CommandLine.Length, &read)) return "N/A";

    return WideToUtf8(buf.data());
}

std::string Win32ProcessAdapter::readUsername(HANDLE hProcess) {
    HANDLE hToken = nullptr;
    if (!OpenProcessToken(hProcess, TOKEN_QUERY, &hToken)) {
        return "SYSTEM";
    }

    DWORD size = 0;
    GetTokenInformation(hToken, TokenUser, nullptr, 0, &size);
    if (size == 0) {
        CloseHandle(hToken);
        return "SYSTEM";
    }

    std::vector<BYTE> buf(size);
    if (GetTokenInformation(hToken, TokenUser, buf.data(), size, &size)) {
        auto pTokenUser = reinterpret_cast<TOKEN_USER*>(buf.data());
        wchar_t name[256] = {0};
        wchar_t domain[256] = {0};
        DWORD nameSize = 256, domainSize = 256;
        SID_NAME_USE sidType;
        if (LookupAccountSidW(nullptr, pTokenUser->User.Sid, name, &nameSize, domain, &domainSize, &sidType)) {
            CloseHandle(hToken);
            return WideToUtf8(name);
        }
    }

    CloseHandle(hToken);
    return "SYSTEM";
}

std::vector<std::pair<uint32_t, uint32_t>> Win32ProcessAdapter::queryParentChildRelations() {
    std::vector<std::pair<uint32_t, uint32_t>> pairs;
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) return pairs;

    PROCESSENTRY32W pe;
    pe.dwSize = sizeof(PROCESSENTRY32W);
    if (Process32FirstW(hSnapshot, &pe)) {
        do {
            pairs.emplace_back(pe.th32ParentProcessID, pe.th32ProcessID);
        } while (Process32NextW(hSnapshot, &pe));
    }
    CloseHandle(hSnapshot);
    return pairs;
}

bool Win32ProcessAdapter::terminateSingleProcess(uint32_t pid) {
    if (pid <= 4) return false;
    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
    if (!hProcess) return false;
    bool ok = TerminateProcess(hProcess, 1);
    CloseHandle(hProcess);
    m_suspendedPids.erase(pid);
    m_history.erase(pid);
    return ok;
}

bool Win32ProcessAdapter::suspendProcess(uint32_t pid) {
    if (pid <= 4) return false;
    if (g_NtSuspendProcess) {
        HANDLE hProcess = OpenProcess(PROCESS_SUSPEND_RESUME, FALSE, pid);
        if (hProcess) {
            NTSTATUS st = g_NtSuspendProcess(hProcess);
            CloseHandle(hProcess);
            if (st == 0) {
                m_suspendedPids.insert(pid);
                return true;
            }
        }
    }
    return false;
}

bool Win32ProcessAdapter::resumeProcess(uint32_t pid) {
    if (pid <= 4) return false;
    if (g_NtResumeProcess) {
        HANDLE hProcess = OpenProcess(PROCESS_SUSPEND_RESUME, FALSE, pid);
        if (hProcess) {
            NTSTATUS st = g_NtResumeProcess(hProcess);
            CloseHandle(hProcess);
            if (st == 0) {
                m_suspendedPids.erase(pid);
                return true;
            }
        }
    }
    return false;
}

bool Win32ProcessAdapter::setProcessPriority(uint32_t pid, int priorityClass) {
    if (pid <= 4) return false;
    HANDLE hProcess = OpenProcess(PROCESS_SET_INFORMATION, FALSE, pid);
    if (!hProcess) return false;

    DWORD winClass = NORMAL_PRIORITY_CLASS;
    switch (priorityClass) {
        case 0: winClass = IDLE_PRIORITY_CLASS; break;
        case 1: winClass = BELOW_NORMAL_PRIORITY_CLASS; break;
        case 2: winClass = NORMAL_PRIORITY_CLASS; break;
        case 3: winClass = ABOVE_NORMAL_PRIORITY_CLASS; break;
        case 4: winClass = HIGH_PRIORITY_CLASS; break;
        case 5: winClass = REALTIME_PRIORITY_CLASS; break;
    }

    bool ok = SetPriorityClass(hProcess, winClass);
    CloseHandle(hProcess);
    return ok;
}

bool Win32ProcessAdapter::setProcessAffinity(uint32_t pid, uint64_t mask) {
    if (pid <= 4 || mask == 0) return false;
    HANDLE hProcess = OpenProcess(PROCESS_SET_INFORMATION, FALSE, pid);
    if (!hProcess) return false;
    bool ok = SetProcessAffinityMask(hProcess, static_cast<DWORD_PTR>(mask));
    CloseHandle(hProcess);
    return ok;
}

bool Win32ProcessAdapter::setEcoQos(uint32_t pid, bool enable) {
    if (pid <= 4) return false;
    HANDLE hProcess = OpenProcess(PROCESS_SET_INFORMATION, FALSE, pid);
    if (!hProcess) return false;

    PROCESS_POWER_THROTTLING_STATE state{};
    state.Version = PROCESS_POWER_THROTTLING_CURRENT_VERSION;
    state.ControlMask = PROCESS_POWER_THROTTLING_EXECUTION_SPEED;
    state.StateMask = enable ? PROCESS_POWER_THROTTLING_EXECUTION_SPEED : 0;

    bool ok = SetProcessInformation(hProcess, ProcessPowerThrottling, &state, sizeof(state));
    CloseHandle(hProcess);
    return ok;
}

} // namespace Rathon::Adapters::Driven
