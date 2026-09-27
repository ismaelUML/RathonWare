#pragma once

#include "ports/driven_ports.h"
#include <memory>

namespace Rathon::UseCases {

class SystemUseCases {
public:
    explicit SystemUseCases(std::shared_ptr<Ports::ISystemPort> port)
        : m_port(std::move(port))
    {
    }

    Domain::SystemSnapshot getSnapshot() {
        if (!m_port) {
            return Domain::SystemSnapshot{};
        }
        return m_port->querySnapshot();
    }

private:
    std::shared_ptr<Ports::ISystemPort> m_port;
};

} // namespace Rathon::UseCases
