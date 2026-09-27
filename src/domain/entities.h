#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <optional>

namespace Rathon::Domain {

// Pure Domain Entities & Value Objects.
// Strictly satisfies Requirement 1 (Hexagonal Architecture - Domain Core):
// ZERO external dependencies. ZERO Qt dependencies. ZERO Win32 dependencies.
// Only standard C++ native primitives.

// Process entity containing OS-agnostic scheduling, memory, and telemetry attributes.
struct Process {
    uint32_t pid = 0;
    uint32_t parentPid = 0;
    std::string name;
    double cpuUsage = 0.0;
    double ramUsageMB = 0.0;      // Resident working set in MB
    double privateUsageMB = 0.0;  // Private commit charge in MB
    double peakUsageMB = 0.0;     // Lifetime peak working set in MB
    int threads = 0;
    std::string username;         // User or SYSTEM
    std::string cmdLine;          // CLI arguments
    std::string priority;         // Normal, High, Realtime, Idle, etc.
    bool isSuspended = false;
    bool isEcoQos = false;
    uint64_t affinityMask = 0;
};

// Network socket to process association.
struct PortBinding {
    int port = 0;
    uint32_t pid = 0;
    std::string processName;
    std::string protocol;       // TCP, UDP, TCP6
    std::string state;          // LISTENING, ESTABLISHED, BOUND
    std::string localAddress;
    double ramUsageMB = 0.0;
    std::string cmdLine;
};

// Process locking an exclusive file or resource.
struct FileLock {
    uint32_t pid = 0;
    std::string processName;
    std::string appName;
    std::string cmdLine;
    double ramMB = 0.0;
    bool isService = false;
};

// Dedicated GPU & AI telemetry metrics.
struct GpuTelemetry {
    bool hasGpu = false;
    bool hasNvidiaGpu = false;
    std::string gpuBackend = "Standard";
    std::string gpuName = "Standard GPU";
    double gpuUsage = 0.0;
    double gpuTemp = 0.0;
    double vramTotalGB = 0.0;
    double vramUsedGB = 0.0;
    double vramFreeGB = 0.0;
    double vramUsagePercent = 0.0;
    double powerUsageW = 0.0;
    double powerLimitW = 0.0;
    int graphicsClockMHz = 0;
    int memoryClockMHz = 0;
    std::string throttleReason = "None / Full Boost";
    std::string throttleStatusLevel = "normal";
};

// Process utilizing GPU resources (VRAM and compute).
struct GpuProcess {
    uint32_t pid = 0;
    std::string name;
    std::string category;      // "CUDA Compute / AI", "DirectX Graphics", etc.
    double vramMB = 0.0;
    double vramGB = 0.0;
    double vramPercent = 0.0;
    bool isCompute = false;
    bool leakSuspected = false;
    double growthRateMB = 0.0;
};

// Logical CPU core telemetry and classification.
struct CpuCoreMetric {
    int coreIndex = 0;
    std::string coreLabel;
    std::string coreType;     // "P-Core", "E-Core", or "Core"
    bool isPCore = false;
    bool isECore = false;
    double load = 0.0;        // 0.0 - 100.0%
    std::string heatColor;    // Hex color gradient
};

// Comprehensive system snapshot.
struct SystemSnapshot {
    // Core CPU & RAM
    double cpuUsage = 0.0;
    double ramUsage = 0.0;
    double ramTotal = 0.0;
    double ramUsed = 0.0;

    // GPU metrics
    double gpuUsage = 0.0;
    double gpuTemp = 0.0;
    double gpuVramUsage = 0.0;
    double gpuVramTotal = 0.0;
    double gpuVramUsed = 0.0;

    // Storage & Network
    std::string uptime = "00:00:00";
    double diskReadSpeed = 0.0;
    double diskWriteSpeed = 0.0;
    double diskUsage = 0.0;
    double diskTotalCapacityGB = 0.0;
    double diskTotalFreeGB = 0.0;
    std::string diskDriveSummary;
    double netDownloadSpeed = 0.0;
    double netUploadSpeed = 0.0;

    // Kernel & Commit Metrics
    double commitTotalGB = 0.0;
    double commitLimitGB = 0.0;
    double commitUsagePercent = 0.0;
    double commitPeakGB = 0.0;
    double kernelPagedMB = 0.0;
    double kernelNonpagedMB = 0.0;
    int processCount = 0;
    int threadCount = 0;
    int handleCount = 0;

    // Hardware Specs
    std::string cpuModel = "Unknown CPU";
    int cpuBaseClockMHz = 0;
    std::string gpuModel = "Unknown GPU";
    std::string gpuBackend = "Standard";
    std::string motherboardModel = "Unknown Motherboard";
    std::string biosVersion = "Unknown BIOS";
    int ramSpeed = 0;
};

} // namespace Rathon::Domain
