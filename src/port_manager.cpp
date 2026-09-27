#include "port_manager.h"
#include <QVariantMap>

PortManager::PortManager(std::shared_ptr<Rathon::UseCases::PortUseCases> useCases, QObject *parent)
    : QObject(parent)
    , m_useCases(std::move(useCases))
{
}

QVariantList PortManager::getProcessesByPort(int port) {
    QVariantList list;
    if (!m_useCases) return list;

    auto entries = m_useCases->getProcessesByPort(port);
    for (const auto& e : entries) {
        QVariantMap map;
        map["port"] = e.port;
        map["pid"] = static_cast<qlonglong>(e.pid);
        map["processName"] = QString::fromStdString(e.processName);
        map["protocol"] = QString::fromStdString(e.protocol);
        map["state"] = QString::fromStdString(e.state);
        map["localAddress"] = QString::fromStdString(e.localAddress);
        map["ramUsageMB"] = e.ramUsageMB;
        map["cmdLine"] = QString::fromStdString(e.cmdLine);
        list.append(map);
    }
    return list;
}

QVariantList PortManager::getAllListeningPorts() {
    QVariantList list;
    if (!m_useCases) return list;

    auto entries = m_useCases->getListeningPorts();
    for (const auto& e : entries) {
        QVariantMap map;
        map["port"] = e.port;
        map["pid"] = static_cast<qlonglong>(e.pid);
        map["processName"] = QString::fromStdString(e.processName);
        map["protocol"] = QString::fromStdString(e.protocol);
        map["state"] = QString::fromStdString(e.state);
        map["localAddress"] = QString::fromStdString(e.localAddress);
        map["ramUsageMB"] = e.ramUsageMB;
        map["cmdLine"] = QString::fromStdString(e.cmdLine);
        list.append(map);
    }
    return list;
}

bool PortManager::killProcessOnPort(int port) {
    if (!m_useCases) return false;
    return m_useCases->killProcessOnPort(port);
}

bool PortManager::killProcessByPid(int pid) {
    if (!m_useCases || pid <= 0) return false;
    return m_useCases->killProcessByPid(static_cast<uint32_t>(pid));
}
