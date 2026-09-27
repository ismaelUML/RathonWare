# RathonWare

Hardware diagnostics, AI/GPU telemetry, and process management utility for Windows developers. Built with C++20 and Qt 6 (Qt Quick).

[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.wikipedia.org/wiki/C%2B%2B20)
[![Qt 6](https://img.shields.io/badge/Qt-6.6%2B-green.svg)](https://www.qt.io/)
[![Platform](https://img.shields.io/badge/Platform-Windows%2010%20%2F%2011%20(x64)-0078d6.svg)](https://www.microsoft.com/windows)

---

## Why This Exists

Windows Task Manager is built for general desktop use, not active software development. It falls short during common engineering workflows:

1. **`EADDRINUSE` friction**: When a local dev server (Node, Vite, Python, Rails, Docker) crashes ungracefully, Task Manager does not show listening ports. Resolving conflicts requires `netstat -ano | findstr :3000` and `taskkill /PID <PID> /F` in a shell.
2. **Locked files and build artifacts**: Windows blocks file deletion when a handle remains open, but Explorer never identifies which process owns it.
3. **Opaque AI/CUDA VRAM allocation**: Task Manager clusters GPU allocations into generic totals, hiding per-process VRAM growth rates during local LLM (Ollama, PyTorch, ComfyUI) experimentation.
4. **Hybrid CPU core contention**: On Intel Alder Lake and Raptor Lake processors (12th–14th Gen), background processes (browsers, chat apps, torrents) routinely compete with compilers and games for Performance Cores (P-Cores).

RathonWare addresses these specific pain points by pairing direct Win32/NT kernel APIs with an accelerated Qt Quick interface.

---

## Quick Start

### Prerequisites

- Windows 10 (21H2+) or Windows 11 (64-bit)
- Qt 6.6+ with MinGW 64-bit (or MSVC 2022)
- CMake 3.16+ and Ninja build system
- Administrative privileges (required to inspect remote process PEBs and manipulate core affinities)

### Build from Source

```powershell
# 1. Clone repository
git clone https://github.com/ismaelUML/RathonWare.git
cd RathonWare

# 2. Add Qt and MinGW toolchain to PATH (adjust paths to your Qt installation)
$env:PATH = "C:\Qt\Tools\mingw1310_64\bin;C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\Ninja;C:\Qt\6.6.3\mingw_64\bin;" + $env:PATH

# 3. Configure and compile in Release mode
cmake -B build -G "Ninja" -DCMAKE_BUILD_TYPE=Release
cmake --build build

# 4. Launch binary
./build/RathonWare.exe
```

### Packaging a Standalone Installer

To collect runtime DLLs with `windeployqt` and compile an Inno Setup installer:

```powershell
./deploy.ps1
```

The resulting standalone setup executable will be generated at `build/RathonWareSetup.exe`.

---

## Key Features

### 1. Port-to-Process Killer
Type `:port` (for example, `:3000`, `:8080`, `:5432`) in the search bar or Command Palette (`Ctrl+K`). RathonWare resolves the owning process using `GetExtendedTcpTable` / `GetExtendedUdpTable` and provides a one-click termination button.

### 2. File Unlocker
Drag and drop any locked file or directory into the File Unlocker dialog, or type `unlock <path>` in the Command Palette. RathonWare uses the Windows Restart Manager API (`rstrtmgr.dll`) to identify holding PIDs and terminate them cleanly.

### 3. AI & GPU Telemetry (NVML + DXGI)
- **Per-Process VRAM Breakdown**: Displays dedicated memory per process, tagging compute processes (PyTorch, Ollama, TensorRT) versus standard 3D graphics.
- **Hardware Throttle Reasons**: Decodes NVIDIA hardware bitmasks in real time (*Thermal Limit*, *Power Cap Limit*, *Hardware Slowdown*).
- **VRAM Leak Detection**: Runs a sliding-window heuristic tracking allocation slope to identify runaway memory growth.
- **Portable Fallback**: Late-binds `nvml.dll` dynamically; automatically falls back to DXGI 1.4 (`IDXGIAdapter3`) on AMD and Intel GPUs.

### 4. CPU Topology & E-Core Pinning
- **Per-Core Heatmap Grid**: Visualizes every logical core with real-time utilization via `NtQuerySystemInformation(SystemProcessorPerformanceInformation)`.
- **Hybrid Core Detection**: Distinguishes Intel P-Cores from E-Cores via `GetLogicalProcessorInformationEx`.
- **Quarantine Background Apps**: 1-click action to lock secondary processes (Discord, Slack, Spotify, Chrome) to E-Cores with Windows 11 EcoQoS (`ProcessPowerThrottling`), preserving 100% of P-Cores for compilers and foreground compute.

### 5. Process Trees & Freezing
- **Kill Process Tree**: Breadth-First Search (BFS) discovery of child processes, terminated bottom-up (leaves first) to avoid orphaned worker processes.
- **Process Suspend / Resume**: Freezes runaway processes atomically using `NtSuspendProcess` without losing memory or debug state.

---

## Technical Architecture

| Component | Implementation | Rationale |
| :--- | :--- | :--- |
| **Frontend** | Qt Quick / QML 6 (`QQuickStyle::Basic`) | Hardware-accelerated rendering; avoids styling conflicts with custom delegates. |
| **Port Resolution** | `iphlpapi.dll` (`GetExtendedTcpTable`) | Sub-millisecond socket-to-PID correlation without shelling out to `netstat`. |
| **File Locking** | `rstrtmgr.dll` (Restart Manager) | Direct kernel query; avoids thread deadlocks associated with full handle scans. |
| **Process Inspection** | `ntdll.dll` (`NtQueryInformationProcess` + PEB) | Reads CLI arguments via `ReadProcessMemory`; avoids 400ms COM delays from WMI. |
| **Core Telemetry** | `ntdll.dll` (`NtQuerySystemInformation`) | Reads per-core kernel/user ticks in under 50 microseconds. |
| **GPU Telemetry** | Late-bound `nvml.dll` with DXGI 1.4 fallback | Prevents missing-DLL crashes on non-NVIDIA hardware. |

---

## Architectural Foundations

RathonWare is engineered following strict Clean / Hexagonal Architecture (Ports & Adapters) and Production Resilience principles:

1. **Hexagonal Core (`src/domain/`, `src/ports/`, `src/usecases/`)**:
   - **Pure Domain (`domain/`)**: Immutable entities and business algorithms (e.g. `ProcessTreeResolver`) implemented using 100% standard C++20 with **zero** external libraries, zero Qt, and zero Win32 headers.
   - **Contracts (`ports/`)**: Pure abstract interfaces (`IProcessPort`, `ISystemPort`, `IGpuPort`, `ICpuTopologyPort`, `IPortPort`, `IFileUnlockerPort`).
   - **Use Cases (`usecases/`)**: Orchestrator core enforcing invariant checks (e.g., kernel protection for PID <= 4, bottom-up tree kill scheduling, and circuit breaker trip logic).
   - **Adapters (`adapters/`)**:
     - *Driven Adapters (`adapters/driven/win32/`)*: Low-level Win32, NT kernel, and DirectX/NVML hardware implementations.
     - *Driving Adapters (`adapters/driving/qt/`)*: Qt Quick QML controllers and models (`ProcessModel`, `SystemMonitor`, etc.).

2. **Resilience & Concurrency Limits**:
   - **Circuit Breaker (`common/circuit_breaker.h`)**: Closed/Open/HalfOpen state machine with automatic failure trip threshold and cooldown probing for GPU telemetry.
   - **Backpressure & Bounded Worker Pool (`common/bounded_executor.h`)**: Fixed worker threads and bounded task queue that rejects excess requests when saturated, eliminating runaway OOM risks.
   - **Memory Eviction (`common/ring_buffer.h`)**: Strict FIFO eviction ring buffers guaranteeing bounded memory usage for 60-second telemetry histories.
   - **Cascading Cancellation (`common/cancellation_token.h`)**: Propagates cancellation signals down to OS loops and background tasks.

3. **Continuous Integration & Quality**:
   - Automated GitHub Actions quality gate ([`.github/workflows/ci.yml`](.github/workflows/ci.yml)) running unit tests (`ctest`) on Windows runners.
   - Automated cyclomatic complexity audit ($\le 5$ per function rule).
   - SonarCloud continuous inspection integration (`sonar-project.properties`).

## Non-Goals

- **Not an overclocking or voltage utility**: Frequency and fan curves belong in firmware or vendor-specific tools (MSI Afterburner).
- **Not cross-platform**: RathonWare is deeply coupled to Windows NT kernel APIs, Win32 handle tables, and NVML/DXGI. Linux and macOS are explicitly unsupported.
- **Not a kernel driver**: Operates entirely in user mode with standard administrator elevation (`requireAdministrator` manifest). No third-party drivers (`.sys`) are installed.
