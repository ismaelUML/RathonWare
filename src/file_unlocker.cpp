#include "file_unlocker.h"
#include <QVariantMap>

FileUnlocker::FileUnlocker(std::shared_ptr<Rathon::UseCases::FileUnlockerUseCases> useCases, QObject *parent)
    : QObject(parent)
    , m_useCases(std::move(useCases))
{
}

QVariantList FileUnlocker::findLockingProcesses(const QString& filePath) {
    QVariantList list;
    if (!m_useCases) return list;

    auto lockers = m_useCases->findLockingProcesses(filePath.toStdString());
    for (const auto& l : lockers) {
        QVariantMap map;
        map["pid"] = static_cast<qlonglong>(l.pid);
        map["name"] = QString::fromStdString(l.processName);
        map["appName"] = QString::fromStdString(l.appName);
        map["isService"] = l.isService;
        map["ramMB"] = l.ramMB;
        map["cmdLine"] = QString::fromStdString(l.cmdLine);
        list.append(map);
    }
    return list;
}

bool FileUnlocker::unlockFile(const QString& filePath) {
    if (!m_useCases) return false;
    return m_useCases->unlockFile(filePath.toStdString());
}

bool FileUnlocker::killLockingProcess(int pid) {
    if (!m_useCases || pid <= 0) return false;
    return m_useCases->killLockingProcess(static_cast<uint32_t>(pid));
}
