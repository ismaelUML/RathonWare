#include "cpu_topology.h"
#include <QMetaObject>
#include <QVariantMap>

CpuCoreModel::CpuCoreModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int CpuCoreModel::rowCount(const QModelIndex &parent) const {
    if (parent.isValid()) return 0;
    return static_cast<int>(m_cores.size());
}

QVariant CpuCoreModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(m_cores.size())) {
        return QVariant();
    }

    const auto &core = m_cores[static_cast<size_t>(index.row())];

    switch (role) {
        case CoreIndexRole: return core.coreIndex;
        case CoreLabelRole: return QString::fromStdString(core.coreLabel);
        case CoreTypeRole: return QString::fromStdString(core.coreType);
        case IsPCoreRole: return core.isPCore;
        case IsECoreRole: return core.isECore;
        case LoadRole: return core.load;
        case HeatColorRole: return QString::fromStdString(core.heatColor);
        default: return QVariant();
    }
}

QHash<int, QByteArray> CpuCoreModel::roleNames() const {
    QHash<int, QByteArray> roles;
    roles[CoreIndexRole] = "coreIndex";
    roles[CoreLabelRole] = "coreLabel";
    roles[CoreTypeRole] = "coreType";
    roles[IsPCoreRole] = "isPCore";
    roles[IsECoreRole] = "isECore";
    roles[LoadRole] = "load";
    roles[HeatColorRole] = "heatColor";
    return roles;
}

void CpuCoreModel::updateCores(std::vector<Rathon::Domain::CpuCoreMetric> cores) {
    beginResetModel();
    m_cores = std::move(cores);
    endResetModel();
}

// --- CpuTopology ---

CpuTopology::CpuTopology(std::shared_ptr<Rathon::UseCases::CpuUseCases> cpuUseCases,
                         std::shared_ptr<Rathon::UseCases::ProcessUseCases> processUseCases,
                         CpuCoreModel *coreModel,
                         QObject *parent)
    : QObject(parent)
    , m_cpuUseCases(std::move(cpuUseCases))
    , m_processUseCases(std::move(processUseCases))
    , m_coreModel(coreModel)
    , m_executor(std::make_unique<Rathon::Common::BoundedExecutor>(1, 4))
{
    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &CpuTopology::updateCoreStats);
    m_timer->start(1000);

    updateCoreStats();
}

CpuTopology::~CpuTopology() {
    if (m_timer) m_timer->stop();
    if (m_executor) m_executor->shutdown();
}

void CpuTopology::updateCoreStats() {
    if (!m_cpuUseCases || !m_executor) return;

    bool expected = false;
    if (!m_isUpdating.compare_exchange_strong(expected, true)) {
        return;
    }

    m_executor->submit([this]() {
        auto metrics = m_cpuUseCases->getCoreMetrics();

        int total = static_cast<int>(metrics.size());
        int pCount = 0, eCount = 0;
        double pSum = 0.0, eSum = 0.0;

        for (const auto& c : metrics) {
            if (c.isPCore) {
                pCount++;
                pSum += c.load;
            } else if (c.isECore) {
                eCount++;
                eSum += c.load;
            }
        }

        bool hasHybrid = (pCount > 0 && eCount > 0);
        double pAvg = pCount > 0 ? (pSum / pCount) : 0.0;
        double eAvg = eCount > 0 ? (eSum / eCount) : 0.0;

        QMetaObject::invokeMethod(this, [this, m = std::move(metrics), total, pCount, eCount, hasHybrid, pAvg, eAvg]() mutable {
            m_totalCores = total;
            m_pCoreCount = pCount;
            m_eCoreCount = eCount;
            m_hasHybridArchitecture = hasHybrid;
            m_pCoreAvgUsage = pAvg;
            m_eCoreAvgUsage = eAvg;

            emit statsChanged();
            if (m_coreModel) {
                m_coreModel->updateCores(std::move(m));
            }
            m_isUpdating.store(false);
        }, Qt::QueuedConnection);
    });
}

bool CpuTopology::pinProcessToECores(int pid) {
    if (!m_processUseCases || pid <= 0) return false;
    return m_processUseCases->pinToECores(static_cast<uint32_t>(pid));
}

bool CpuTopology::pinProcessToPCores(int pid) {
    if (!m_processUseCases || pid <= 0) return false;
    return m_processUseCases->pinToPCores(static_cast<uint32_t>(pid));
}

bool CpuTopology::resetProcessAffinity(int pid) {
    if (!m_processUseCases || pid <= 0) return false;
    return m_processUseCases->resetAffinity(static_cast<uint32_t>(pid));
}

QVariantList CpuTopology::getBackgroundApps() {
    QVariantList list;
    if (!m_processUseCases) return list;

    auto procs = m_processUseCases->getProcesses();
    for (const auto& p : procs) {
        if (p.cpuUsage > 0.5) {
            QVariantMap map;
            map["pid"] = static_cast<qlonglong>(p.pid);
            map["name"] = QString::fromStdString(p.name);
            map["cpuUsage"] = p.cpuUsage;
            map["ramMB"] = p.ramUsageMB;
            map["isEcoQos"] = p.isEcoQos;
            list.append(map);
        }
    }
    return list;
}
