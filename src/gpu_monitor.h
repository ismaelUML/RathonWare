#pragma once

#include <QObject>
#include <QAbstractListModel>
#include <QVector>
#include <QTimer>
#include <memory>
#include "usecases/gpu_usecases.h"
#include "common/bounded_executor.h"

// Model backing the dedicated AI / GPU Telemetry process table in QML
class GpuProcessModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum GpuProcessRoles {
        PidRole = Qt::UserRole + 1,
        NameRole,
        CategoryRole,
        VramMBRole,
        VramGBRole,
        VramPercentRole,
        IsComputeRole,
        LeakSuspectedRole,
        GrowthRateMBRole
    };

    explicit GpuProcessModel(std::shared_ptr<Rathon::UseCases::GpuUseCases> useCases, QObject *parent = nullptr);
    ~GpuProcessModel() override = default;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    void updateProcesses(std::vector<Rathon::Domain::GpuProcess> processes);
    Q_INVOKABLE bool killProcess(int pid);

private:
    std::shared_ptr<Rathon::UseCases::GpuUseCases> m_useCases;
    std::vector<Rathon::Domain::GpuProcess> m_processes;
};

class GpuMonitor : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool hasNvidiaGpu READ hasNvidiaGpu NOTIFY statsChanged)
    Q_PROPERTY(bool hasGpu READ hasGpu NOTIFY statsChanged)
    Q_PROPERTY(QString gpuBackend READ gpuBackend NOTIFY statsChanged)
    Q_PROPERTY(QString gpuName READ gpuName NOTIFY statsChanged)
    Q_PROPERTY(double gpuUsage READ gpuUsage NOTIFY statsChanged)
    Q_PROPERTY(double gpuTemp READ gpuTemp NOTIFY statsChanged)
    Q_PROPERTY(double vramTotalGB READ vramTotalGB NOTIFY statsChanged)
    Q_PROPERTY(double vramUsedGB READ vramUsedGB NOTIFY statsChanged)
    Q_PROPERTY(double vramFreeGB READ vramFreeGB NOTIFY statsChanged)
    Q_PROPERTY(double vramUsagePercent READ vramUsagePercent NOTIFY statsChanged)
    Q_PROPERTY(double powerUsageW READ powerUsageW NOTIFY statsChanged)
    Q_PROPERTY(double powerLimitW READ powerLimitW NOTIFY statsChanged)
    Q_PROPERTY(int graphicsClockMHz READ graphicsClockMHz NOTIFY statsChanged)
    Q_PROPERTY(int memoryClockMHz READ memoryClockMHz NOTIFY statsChanged)
    Q_PROPERTY(QString throttleReason READ throttleReason NOTIFY statsChanged)
    Q_PROPERTY(QString throttleStatusLevel READ throttleStatusLevel NOTIFY statsChanged)

public:
    explicit GpuMonitor(std::shared_ptr<Rathon::UseCases::GpuUseCases> useCases, GpuProcessModel *processModel, QObject *parent = nullptr);
    ~GpuMonitor() override;

    bool hasNvidiaGpu() const { return m_telemetry.hasNvidiaGpu; }
    bool hasGpu() const { return m_telemetry.hasGpu; }
    QString gpuBackend() const { return QString::fromStdString(m_telemetry.gpuBackend); }
    QString gpuName() const { return QString::fromStdString(m_telemetry.gpuName); }
    double gpuUsage() const { return m_telemetry.gpuUsage; }
    double gpuTemp() const { return m_telemetry.gpuTemp; }
    double vramTotalGB() const { return m_telemetry.vramTotalGB; }
    double vramUsedGB() const { return m_telemetry.vramUsedGB; }
    double vramFreeGB() const { return m_telemetry.vramFreeGB; }
    double vramUsagePercent() const { return m_telemetry.vramUsagePercent; }
    double powerUsageW() const { return m_telemetry.powerUsageW; }
    double powerLimitW() const { return m_telemetry.powerLimitW; }
    int graphicsClockMHz() const { return m_telemetry.graphicsClockMHz; }
    int memoryClockMHz() const { return m_telemetry.memoryClockMHz; }
    QString throttleReason() const { return QString::fromStdString(m_telemetry.throttleReason); }
    QString throttleStatusLevel() const { return QString::fromStdString(m_telemetry.throttleStatusLevel); }

    Q_INVOKABLE void refresh();

signals:
    void statsChanged();

private slots:
    void updateTelemetry();

private:
    std::shared_ptr<Rathon::UseCases::GpuUseCases> m_useCases;
    GpuProcessModel *m_processModel = nullptr;
    std::unique_ptr<Rathon::Common::BoundedExecutor> m_executor;
    QTimer *m_timer = nullptr;

    Rathon::Domain::GpuTelemetry m_telemetry;
    std::atomic<bool> m_isUpdating{false};
};
