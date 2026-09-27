#include "gpu_monitor.h"
#include <QMetaObject>

GpuProcessModel::GpuProcessModel(std::shared_ptr<Rathon::UseCases::GpuUseCases> useCases, QObject *parent)
    : QAbstractListModel(parent)
    , m_useCases(std::move(useCases))
{
}

int GpuProcessModel::rowCount(const QModelIndex &parent) const {
    if (parent.isValid()) return 0;
    return static_cast<int>(m_processes.size());
}

QVariant GpuProcessModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(m_processes.size())) {
        return QVariant();
    }

    const auto &proc = m_processes[static_cast<size_t>(index.row())];

    switch (role) {
        case PidRole: return static_cast<qlonglong>(proc.pid);
        case NameRole: return QString::fromStdString(proc.name);
        case CategoryRole: return QString::fromStdString(proc.category);
        case VramMBRole: return proc.vramMB;
        case VramGBRole: return proc.vramGB;
        case VramPercentRole: return proc.vramPercent;
        case IsComputeRole: return proc.isCompute;
        case LeakSuspectedRole: return proc.leakSuspected;
        case GrowthRateMBRole: return proc.growthRateMB;
        default: return QVariant();
    }
}

QHash<int, QByteArray> GpuProcessModel::roleNames() const {
    QHash<int, QByteArray> roles;
    roles[PidRole] = "pid";
    roles[NameRole] = "name";
    roles[CategoryRole] = "category";
    roles[VramMBRole] = "vramMB";
    roles[VramGBRole] = "vramGB";
    roles[VramPercentRole] = "vramPercent";
    roles[IsComputeRole] = "isCompute";
    roles[LeakSuspectedRole] = "leakSuspected";
    roles[GrowthRateMBRole] = "growthRateMB";
    return roles;
}

void GpuProcessModel::updateProcesses(std::vector<Rathon::Domain::GpuProcess> processes) {
    beginResetModel();
    m_processes = std::move(processes);
    endResetModel();
}

bool GpuProcessModel::killProcess(int pid) {
    if (!m_useCases || pid <= 0) return false;
    return m_useCases->terminateProcess(static_cast<uint32_t>(pid));
}

// --- GpuMonitor ---

GpuMonitor::GpuMonitor(std::shared_ptr<Rathon::UseCases::GpuUseCases> useCases, GpuProcessModel *processModel, QObject *parent)
    : QObject(parent)
    , m_useCases(std::move(useCases))
    , m_processModel(processModel)
    , m_executor(std::make_unique<Rathon::Common::BoundedExecutor>(1, 4))
{
    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &GpuMonitor::updateTelemetry);
    m_timer->start(1500);

    updateTelemetry();
}

GpuMonitor::~GpuMonitor() {
    if (m_timer) m_timer->stop();
    if (m_executor) m_executor->shutdown();
}

void GpuMonitor::refresh() {
    updateTelemetry();
}

void GpuMonitor::updateTelemetry() {
    if (!m_useCases || !m_executor) return;

    bool expected = false;
    if (!m_isUpdating.compare_exchange_strong(expected, true)) {
        return;
    }

    bool ok = m_executor->submit([this]() {
        auto telem = m_useCases->getTelemetry();
        auto procs = m_useCases->getProcesses();

        QMetaObject::invokeMethod(this, [this, t = std::move(telem), p = std::move(procs)]() mutable {
            m_telemetry = std::move(t);
            emit statsChanged();
            if (m_processModel) {
                m_processModel->updateProcesses(std::move(p));
            }
            m_isUpdating.store(false);
        }, Qt::QueuedConnection);
    });

    if (!ok) {
        m_isUpdating.store(false);
    }
}
