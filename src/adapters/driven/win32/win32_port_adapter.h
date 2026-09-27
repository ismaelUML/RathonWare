#pragma once

#include "ports/driven_ports.h"
#include <vector>

namespace Rathon::Adapters::Driven {

class Win32PortAdapter : public Ports::IPortPort {
public:
    Win32PortAdapter() = default;
    ~Win32PortAdapter() override = default;

    std::vector<Domain::PortBinding> queryAllPortBindings() override;
    bool terminateProcessOnPort(uint32_t pid) override;

private:
    void queryTcp4(std::vector<Domain::PortBinding>& entries);
    void queryTcp6(std::vector<Domain::PortBinding>& entries);
    void queryUdp4(std::vector<Domain::PortBinding>& entries);
    void queryUdp6(std::vector<Domain::PortBinding>& entries);

    std::string getProcessName(uint32_t pid);
    double getProcessMemoryMB(uint32_t pid);
    std::string getProcessCommandLine(uint32_t pid);
};

} // namespace Rathon::Adapters::Driven
