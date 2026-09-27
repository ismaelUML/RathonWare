#include "process_model.h"
#include <QMetaObject>

ProcessModel::ProcessModel(std::shared_ptr<Rathon::UseCases::ProcessUseCases> useCases, QObject *parent)
    : QAbstractListModel(parent)
    , m_useCases(std::move(useCases))
    , m_executor(std::make_unique<Rathon::Common::BoundedExecutor>(1, 4))
{
    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &ProcessModel::refresh);
    m_timer->start(2000);

    refresh();
}

ProcessModel::~ProcessModel() {
    if (m_timer) {
        m_timer->stop();
    }
    if (m_executor) {
        m_executor->shutdown();
    }
}

int ProcessModel::rowCount(const QModelIndex &parent) const {
    if (parent.isValid()) return 0;
    return static_cast<int>(m_processes.size());
}

QVariant ProcessModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(m_processes.size())) {
        return QVariant();
    }

    const auto &proc = m_processes[static_cast<size_t>(index.row())];

    switch (role) {
        case PidRole: return static_cast<qlonglong>(proc.pid);
        case ParentPidRole: return static_cast<qlonglong>(proc.parentPid);
        case NameRole: return QString::fromStdString(proc.name);
        case CpuRole: return proc.cpuUsage;
        case RamRole: return proc.ramUsageMB;
        case PrivateRole: return proc.privateUsageMB;
        case PeakRole: return proc.peakUsageMB;
        case ThreadsRole: return proc.threads;
        case UsernameRole: return QString::fromStdString(proc.username);
        case CmdLineRole: return QString::fromStdString(proc.cmdLine);
        case PriorityRole: return QString::fromStdString(proc.priority);
        case IsSuspendedRole: return proc.isSuspended;
        case IsEcoQosRole: return proc.isEcoQos;
        default: return QVariant();
    }
}

QHash<int, QByteArray> ProcessModel::roleNames() const {
    QHash<int, QByteArray> roles;
    roles[PidRole] = "pid";
    roles[ParentPidRole] = "parentPid";
    roles[NameRole] = "name";
    roles[CpuRole] = "cpuUsage";
    roles[RamRole] = "ramUsage";
    roles[PrivateRole] = "privateUsage";
    roles[PeakRole] = "peakUsage";
    roles[ThreadsRole] = "threads";
    roles[UsernameRole] = "username";
    roles[CmdLineRole] = "cmdLine";
    roles[PriorityRole] = "priority";
    roles[IsSuspendedRole] = "isSuspended";
    roles[IsEcoQosRole] = "isEcoQos";
    return roles;
}

bool ProcessModel::hasHybridCores() const {
    return m_useCases ? m_useCases->hasHybridCores() : false;
}

void ProcessModel::refresh() {
    if (!m_useCases || !m_executor) return;

    // Backpressure & Debounce: skip if already executing a refresh cycle
    bool expected = false;
    if (!m_isRefreshing.compare_exchange_strong(expected, true)) {
        return;
    }

    // Submit heavy Win32 snapshot query to bounded worker pool
    bool submitted = m_executor->submit([this]() {
        auto list = m_useCases->getProcesses();
        QMetaObject::invokeMethod(this, [this, procs = std::move(list)]() mutable {
            applyProcessUpdates(std::move(procs));
            m_isRefreshing.store(false);
        }, Qt::QueuedConnection);
    });

    if (!submitted) {
        m_isRefreshing.store(false); // Capacity exceeded / backpressure dropped
    }
}

void ProcessModel::applyProcessUpdates(std::vector<Rathon::Domain::Process> newProcs) {
    beginResetModel();
    m_processes = std::move(newProcs);
    endResetModel();
}

bool ProcessModel::killProcess(int pid) {
    if (!m_useCases || pid <= 0) return false;
    bool ok = m_useCases->terminateProcess(static_cast<uint32_t>(pid));
    if (ok) refresh();
    return ok;
}

bool ProcessModel::killProcessTree(int pid) {
    if (!m_useCases || pid <= 0) return false;
    bool ok = m_useCases->terminateProcessTree(static_cast<uint32_t>(pid));
    if (ok) refresh();
    return ok;
}

bool ProcessModel::setPriority(int pid, int priorityClassValue) {
    if (!m_useCases || pid <= 0) return false;
    bool ok = m_useCases->setPriority(static_cast<uint32_t>(pid), priorityClassValue);
    if (ok) refresh();
    return ok;
}

bool ProcessModel::suspendProcess(int pid) {
    if (!m_useCases || pid <= 0) return false;
    bool ok = m_useCases->freezeProcess(static_cast<uint32_t>(pid));
    if (ok) refresh();
    return ok;
}

bool ProcessModel::resumeProcess(int pid) {
    if (!m_useCases || pid <= 0) return false;
    bool ok = m_useCases->resumeProcess(static_cast<uint32_t>(pid));
    if (ok) refresh();
    return ok;
}

bool ProcessModel::pinToECores(int pid) {
    if (!m_useCases || pid <= 0) return false;
    bool ok = m_useCases->pinToECores(static_cast<uint32_t>(pid));
    if (ok) refresh();
    return ok;
}

bool ProcessModel::pinToPCores(int pid) {
    if (!m_useCases || pid <= 0) return false;
    bool ok = m_useCases->pinToPCores(static_cast<uint32_t>(pid));
    if (ok) refresh();
    return ok;
}

bool ProcessModel::resetAffinity(int pid) {
    if (!m_useCases || pid <= 0) return false;
    bool ok = m_useCases->resetAffinity(static_cast<uint32_t>(pid));
    if (ok) refresh();
    return ok;
}
