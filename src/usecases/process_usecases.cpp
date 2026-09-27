#include "process_usecases.h"

namespace Rathon::UseCases {

bool ProcessUseCases::terminateProcessTree(uint32_t pid)
{
    if (Domain::ProcessTreeResolver::isProtectedPid(pid) || !m_port) {
        return false;
    }

    auto relations = m_port->queryParentChildRelations();
    auto killOrder = Domain::ProcessTreeResolver::resolveBottomUpKillOrder(pid, relations);

    if (killOrder.empty()) {
        return m_port->terminateSingleProcess(pid);
    }

    bool allSuccess = true;
    for (uint32_t p : killOrder) {
        if (!m_port->terminateSingleProcess(p)) {
            allSuccess = false;
        }
    }
    return allSuccess;
}

} // namespace Rathon::UseCases
