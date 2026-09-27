#pragma once

#include "ports/driven_ports.h"
#include <windows.h>
#include <vector>

namespace Rathon::Adapters::Driven {

struct CoreTimeSample {
    LARGE_INTEGER idleTime;
    LARGE_INTEGER kernelTime;
    LARGE_INTEGER userTime;
};

class Win32CpuAdapter : public Ports::ICpuTopologyPort {
public:
    Win32CpuAdapter();
    ~Win32CpuAdapter() override = default;

    std::vector<Domain::CpuCoreMetric> queryCoreMetrics() override;

private:
    void initTopology();
    std::string calculateHeatColor(double load);
    void sampleKernelTimes(std::vector<Domain::CpuCoreMetric>& metrics);

    int m_coreCount = 1;
    uint64_t m_eCoreMask = 0;
    uint64_t m_pCoreMask = 0;
    bool m_hasHybridCores = false;

    std::vector<CoreTimeSample> m_prevSamples;
    std::vector<Domain::CpuCoreMetric> m_staticTopology;
};

} // namespace Rathon::Adapters::Driven
