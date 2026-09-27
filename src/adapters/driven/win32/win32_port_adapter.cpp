#include "win32_port_adapter.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iphlpapi.h>
#include <tlhelp32.h>
#include <psapi.h>

namespace Rathon::Adapters::Driven {

static std::string WideToUtf8(const wchar_t* wstr) {
    if (!wstr || !*wstr) return "";
    int len = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, nullptr, 0, nullptr, nullptr);
    if (len <= 1) return "";
    std::string out(len - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wstr, -1, out.data(), len, nullptr, nullptr);
    return out;
}

static std::string FormatIpv4(DWORD ip) {
    in_addr in;
    in.S_un.S_addr = ip;
    char buf[INET_ADDRSTRLEN] = {0};
    inet_ntop(AF_INET, &in, buf, INET_ADDRSTRLEN);
    return buf;
}

static std::string TcpStateToString(DWORD state) {
    switch (state) {
        case MIB_TCP_STATE_CLOSED: return "CLOSED";
        case MIB_TCP_STATE_LISTEN: return "LISTENING";
        case MIB_TCP_STATE_SYN_SENT: return "SYN_SENT";
        case MIB_TCP_STATE_SYN_RCVD: return "SYN_RCVD";
        case MIB_TCP_STATE_ESTAB: return "ESTABLISHED";
        case MIB_TCP_STATE_FIN_WAIT1: return "FIN_WAIT1";
        case MIB_TCP_STATE_FIN_WAIT2: return "FIN_WAIT2";
        case MIB_TCP_STATE_CLOSE_WAIT: return "CLOSE_WAIT";
        case MIB_TCP_STATE_CLOSING: return "CLOSING";
        case MIB_TCP_STATE_LAST_ACK: return "LAST_ACK";
        case MIB_TCP_STATE_TIME_WAIT: return "TIME_WAIT";
        case MIB_TCP_STATE_DELETE_TCB: return "DELETE_TCB";
        default: return "UNKNOWN";
    }
}

std::string Win32PortAdapter::getProcessName(uint32_t pid) {
    if (pid == 0) return "System Idle";
    if (pid == 4) return "System";

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

double Win32PortAdapter::getProcessMemoryMB(uint32_t pid) {
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

std::string Win32PortAdapter::getProcessCommandLine(uint32_t pid) {
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

void Win32PortAdapter::queryTcp4(std::vector<Domain::PortBinding>& entries) {
    DWORD size = 0;
    GetExtendedTcpTable(nullptr, &size, FALSE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0);
    if (size == 0) return;

    std::vector<BYTE> buffer(size);
    if (GetExtendedTcpTable(buffer.data(), &size, FALSE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0) == NO_ERROR) {
        auto pTable = reinterpret_cast<PMIB_TCPTABLE_OWNER_PID>(buffer.data());
        for (DWORD i = 0; i < pTable->dwNumEntries; ++i) {
            const auto& row = pTable->table[i];
            Domain::PortBinding b;
            b.port = ntohs(static_cast<u_short>(row.dwLocalPort));
            b.pid = row.dwOwningPid;
            b.protocol = "TCP";
            b.state = TcpStateToString(row.dwState);
            b.localAddress = FormatIpv4(row.dwLocalAddr);
            b.processName = getProcessName(b.pid);
            b.ramUsageMB = getProcessMemoryMB(b.pid);
            b.cmdLine = getProcessCommandLine(b.pid);
            entries.push_back(std::move(b));
        }
    }
}

void Win32PortAdapter::queryTcp6(std::vector<Domain::PortBinding>& entries) {
    DWORD size = 0;
    GetExtendedTcpTable(nullptr, &size, FALSE, AF_INET6, TCP_TABLE_OWNER_PID_ALL, 0);
    if (size == 0) return;

    std::vector<BYTE> buffer(size);
    if (GetExtendedTcpTable(buffer.data(), &size, FALSE, AF_INET6, TCP_TABLE_OWNER_PID_ALL, 0) == NO_ERROR) {
        auto pTable = reinterpret_cast<PMIB_TCP6TABLE_OWNER_PID>(buffer.data());
        for (DWORD i = 0; i < pTable->dwNumEntries; ++i) {
            const auto& row = pTable->table[i];
            Domain::PortBinding b;
            b.port = ntohs(static_cast<u_short>(row.dwLocalPort));
            b.pid = row.dwOwningPid;
            b.protocol = "TCP6";
            b.state = TcpStateToString(row.dwState);
            b.localAddress = "[::]";
            b.processName = getProcessName(b.pid);
            b.ramUsageMB = getProcessMemoryMB(b.pid);
            b.cmdLine = getProcessCommandLine(b.pid);
            entries.push_back(std::move(b));
        }
    }
}

void Win32PortAdapter::queryUdp4(std::vector<Domain::PortBinding>& entries) {
    DWORD size = 0;
    GetExtendedUdpTable(nullptr, &size, FALSE, AF_INET, UDP_TABLE_OWNER_PID, 0);
    if (size == 0) return;

    std::vector<BYTE> buffer(size);
    if (GetExtendedUdpTable(buffer.data(), &size, FALSE, AF_INET, UDP_TABLE_OWNER_PID, 0) == NO_ERROR) {
        auto pTable = reinterpret_cast<PMIB_UDPTABLE_OWNER_PID>(buffer.data());
        for (DWORD i = 0; i < pTable->dwNumEntries; ++i) {
            const auto& row = pTable->table[i];
            Domain::PortBinding b;
            b.port = ntohs(static_cast<u_short>(row.dwLocalPort));
            b.pid = row.dwOwningPid;
            b.protocol = "UDP";
            b.state = "BOUND";
            b.localAddress = FormatIpv4(row.dwLocalAddr);
            b.processName = getProcessName(b.pid);
            b.ramUsageMB = getProcessMemoryMB(b.pid);
            b.cmdLine = getProcessCommandLine(b.pid);
            entries.push_back(std::move(b));
        }
    }
}

void Win32PortAdapter::queryUdp6(std::vector<Domain::PortBinding>& entries) {
    DWORD size = 0;
    GetExtendedUdpTable(nullptr, &size, FALSE, AF_INET6, UDP_TABLE_OWNER_PID, 0);
    if (size == 0) return;

    std::vector<BYTE> buffer(size);
    if (GetExtendedUdpTable(buffer.data(), &size, FALSE, AF_INET6, UDP_TABLE_OWNER_PID, 0) == NO_ERROR) {
        auto pTable = reinterpret_cast<PMIB_UDP6TABLE_OWNER_PID>(buffer.data());
        for (DWORD i = 0; i < pTable->dwNumEntries; ++i) {
            const auto& row = pTable->table[i];
            Domain::PortBinding b;
            b.port = ntohs(static_cast<u_short>(row.dwLocalPort));
            b.pid = row.dwOwningPid;
            b.protocol = "UDP6";
            b.state = "BOUND";
            b.localAddress = "[::]";
            b.processName = getProcessName(b.pid);
            b.ramUsageMB = getProcessMemoryMB(b.pid);
            b.cmdLine = getProcessCommandLine(b.pid);
            entries.push_back(std::move(b));
        }
    }
}

std::vector<Domain::PortBinding> Win32PortAdapter::queryAllPortBindings() {
    std::vector<Domain::PortBinding> entries;
    queryTcp4(entries);
    queryTcp6(entries);
    queryUdp4(entries);
    queryUdp6(entries);
    return entries;
}

bool Win32PortAdapter::terminateProcessOnPort(uint32_t pid) {
    if (pid <= 4) return false;
    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
    if (!hProcess) return false;
    bool ok = TerminateProcess(hProcess, 1);
    CloseHandle(hProcess);
    return ok;
}

} // namespace Rathon::Adapters::Driven
