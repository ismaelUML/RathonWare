#include "win32_file_unlocker_adapter.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <restartmanager.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <algorithm>

namespace Rathon::Adapters::Driven {

static std::string WideToUtf8(const wchar_t* wstr) {
    if (!wstr || !*wstr) return "";
    int len = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, nullptr, 0, nullptr, nullptr);
    if (len <= 1) return "";
    std::string out(len - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wstr, -1, out.data(), len, nullptr, nullptr);
    return out;
}

std::wstring Win32FileUnlockerAdapter::normalizeToNativePath(const std::string& path) {
    std::string clean = path;
    if (clean.rfind("file:///", 0) == 0) {
        clean = clean.substr(8);
    }
    std::replace(clean.begin(), clean.end(), '/', '\\');

    int len = MultiByteToWideChar(CP_UTF8, 0, clean.c_str(), -1, nullptr, 0);
    if (len <= 1) return L"";
    std::wstring wpath(len - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, clean.c_str(), -1, wpath.data(), len);
    return wpath;
}

std::string Win32FileUnlockerAdapter::getProcessName(uint32_t pid) {
    if (pid == 0) return "System Idle";
    if (pid == 4) return "System";

    std::string name = "Unknown";
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32W pe32;
        pe32.dwSize = sizeof(PROCESSENTRY32W);
        if (Process32FirstW(hSnapshot, &pe32)) {
            do {
                if (pe32.th32ProcessID == pid) {
                    name = WideToUtf8(pe32.szExeFile);
                    break;
                }
            } while (Process32NextW(hSnapshot, &pe32));
        }
        CloseHandle(hSnapshot);
    }
    return name;
}

double Win32FileUnlockerAdapter::getProcessMemoryMB(uint32_t pid) {
    if (pid <= 4) return 0.0;
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (!hProcess) {
        hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    }
    if (hProcess) {
        PROCESS_MEMORY_COUNTERS pmc;
        if (GetProcessMemoryInfo(hProcess, &pmc, sizeof(pmc))) {
            CloseHandle(hProcess);
            return pmc.WorkingSetSize / (1024.0 * 1024.0);
        }
        CloseHandle(hProcess);
    }
    return 0.0;
}

std::string Win32FileUnlockerAdapter::getProcessCommandLine(uint32_t pid) {
    if (pid <= 4) return "";
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (hProcess) {
        wchar_t path[MAX_PATH] = {0};
        DWORD size = MAX_PATH;
        if (QueryFullProcessImageNameW(hProcess, 0, path, &size)) {
            CloseHandle(hProcess);
            return WideToUtf8(path);
        }
        CloseHandle(hProcess);
    }
    return "";
}

std::vector<Domain::FileLock> Win32FileUnlockerAdapter::queryLockingProcesses(const std::string& filePath) {
    std::vector<Domain::FileLock> results;
    std::wstring nativePath = normalizeToNativePath(filePath);
    if (nativePath.empty()) return results;

    DWORD dwSession = 0;
    WCHAR szSessionKey[CCH_RM_SESSION_KEY + 1] = {0};
    if (RmStartSession(&dwSession, 0, szSessionKey) != ERROR_SUCCESS) {
        return results;
    }

    PCWSTR pszFile = nativePath.c_str();
    if (RmRegisterResources(dwSession, 1, &pszFile, 0, nullptr, 0, nullptr) == ERROR_SUCCESS) {
        UINT nProcInfoNeeded = 0;
        UINT nProcInfo = 0;
        DWORD dwRebootReasons = RmRebootReasonNone;

        DWORD dwError = RmGetList(dwSession, &nProcInfoNeeded, &nProcInfo, nullptr, &dwRebootReasons);
        if (dwError == ERROR_MORE_DATA && nProcInfoNeeded > 0) {
            std::vector<RM_PROCESS_INFO> rgProcesses(nProcInfoNeeded);
            nProcInfo = nProcInfoNeeded;

            if (RmGetList(dwSession, &nProcInfoNeeded, &nProcInfo, rgProcesses.data(), &dwRebootReasons) == ERROR_SUCCESS) {
                for (UINT i = 0; i < nProcInfo; ++i) {
                    const auto& info = rgProcesses[i];
                    Domain::FileLock lock;
                    lock.pid = info.Process.dwProcessId;
                    lock.processName = getProcessName(lock.pid);
                    lock.appName = WideToUtf8(info.strAppName);
                    lock.isService = (info.ApplicationType == RmService);
                    lock.ramMB = getProcessMemoryMB(lock.pid);
                    lock.cmdLine = getProcessCommandLine(lock.pid);
                    results.push_back(std::move(lock));
                }
            }
        }
    }

    RmEndSession(dwSession);
    return results;
}

bool Win32FileUnlockerAdapter::terminateLockingProcess(uint32_t pid) {
    if (pid <= 4) return false;
    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, static_cast<DWORD>(pid));
    if (!hProcess) return false;
    bool ok = TerminateProcess(hProcess, 1);
    CloseHandle(hProcess);
    return ok;
}

} // namespace Rathon::Adapters::Driven
