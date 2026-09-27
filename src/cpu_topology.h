#pragma once

#include <QObject>
#include <QAbstractListModel>
#include <QVector>
#include <QVariantList>
#include <QTimer>
#include <memory>
#include "usecases/cpu_usecases.h"
#include "usecases/process_usecases.h"
#include "common/bounded_executor.h"

class CpuCoreModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum CpuCoreRoles {
        CoreIndexRole = Qt::UserRole + 1,
        CoreLabelRole,
        CoreTypeRole,
        IsPCoreRole,
        IsECoreRole,
        LoadRole,
        HeatColorRole
    };

    explicit CpuCoreModel(QObject *parent = nullptr);
    ~CpuCoreModel() override = default;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    void updateCores(std::vector<Rathon::Domain::CpuCoreMetric> cores);

private:
    std::vector<Rathon::Domain::CpuCoreMetric> m_cores;
};

class CpuTopology : public QObject
{
    Q_OBJECT

    Q_PROPERTY(int totalCores READ totalCores NOTIFY statsChanged)
    Q_PROPERTY(int pCoreCount READ pCoreCount NOTIFY statsChanged)
    Q_PROPERTY(int eCoreCount READ eCoreCount NOTIFY statsChanged)
    Q_PROPERTY(bool hasHybridArchitecture READ hasHybridArchitecture NOTIFY statsChanged)
    Q_PROPERTY(double pCoreAvgUsage READ pCoreAvgUsage NOTIFY statsChanged)
    Q_PROPERTY(double eCoreAvgUsage READ eCoreAvgUsage NOTIFY statsChanged)

public:
    explicit CpuTopology(std::shared_ptr<Rathon::UseCases::CpuUseCases> cpuUseCases,
                         std::shared_ptr<Rathon::UseCases::ProcessUseCases> processUseCases,
                         CpuCoreModel *coreModel,
                         QObject *parent = nullptr);
    ~CpuTopology() override;

    int totalCores() const { return m_totalCores; }
    int pCoreCount() const { return m_pCoreCount; }
    int eCoreCount() const { return m_eCoreCount; }
    bool hasHybridArchitecture() const { return m_hasHybridArchitecture; }
    double pCoreAvgUsage() const { return m_pCoreAvgUsage; }
    double eCoreAvgUsage() const { return m_eCoreAvgUsage; }

    Q_INVOKABLE bool pinProcessToECores(int pid);
    Q_INVOKABLE bool pinProcessToPCores(int pid);
    Q_INVOKABLE bool resetProcessAffinity(int pid);
    Q_INVOKABLE QVariantList getBackgroundApps();

signals:
    void statsChanged();

private slots:
    void updateCoreStats();

private:
    std::shared_ptr<Rathon::UseCases::CpuUseCases> m_cpuUseCases;
    std::shared_ptr<Rathon::UseCases::ProcessUseCases> m_processUseCases;
    CpuCoreModel *m_coreModel = nullptr;
    std::unique_ptr<Rathon::Common::BoundedExecutor> m_executor;
    QTimer *m_timer = nullptr;

    int m_totalCores = 0;
    int m_pCoreCount = 0;
    int m_eCoreCount = 0;
    bool m_hasHybridArchitecture = false;
    double m_pCoreAvgUsage = 0.0;
    double m_eCoreAvgUsage = 0.0;
    std::atomic<bool> m_isUpdating{false};
};
