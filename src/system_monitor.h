#pragma once

#include <QObject>
#include <QTimer>
#include <QString>
#include <memory>
#include "usecases/system_usecases.h"
#include "common/reactive_stream.h"
#include "common/bounded_executor.h"

// Reactive Driving Adapter bridging SystemUseCases into Qt Quick / QML.
// Satisfies Hexagonal Architecture: Contains ZERO Win32 / PDH / DXGI calls.
// Consumes telemetry events pushed by the background reactive stream and exposes
// typed Q_PROPERTY bindings to QML delegates.
class SystemMonitor : public QObject
{
    Q_OBJECT

    // Core monitors
    Q_PROPERTY(double cpuUsage READ cpuUsage NOTIFY cpuUsageChanged)
    Q_PROPERTY(double ramUsage READ ramUsage NOTIFY ramUsageChanged)
    Q_PROPERTY(double ramTotal READ ramTotal NOTIFY ramTotalChanged)
    Q_PROPERTY(double ramUsed READ ramUsed NOTIFY ramUsedChanged)
    Q_PROPERTY(double gpuUsage READ gpuUsage NOTIFY gpuUsageChanged)
    Q_PROPERTY(double gpuTemp READ gpuTemp NOTIFY gpuTempChanged)
    Q_PROPERTY(double gpuVramUsage READ gpuVramUsage NOTIFY gpuVramUsageChanged)
    Q_PROPERTY(double gpuVramTotal READ gpuVramTotal NOTIFY gpuVramTotalChanged)
    Q_PROPERTY(double gpuVramUsed READ gpuVramUsed NOTIFY gpuVramUsedChanged)

    // Extended Metrics
    Q_PROPERTY(QString uptime READ uptime NOTIFY uptimeChanged)
    Q_PROPERTY(double diskReadSpeed READ diskReadSpeed NOTIFY diskReadSpeedChanged)
    Q_PROPERTY(double diskWriteSpeed READ diskWriteSpeed NOTIFY diskWriteSpeedChanged)
    Q_PROPERTY(double diskUsage READ diskUsage NOTIFY diskUsageChanged)
    Q_PROPERTY(double diskTotalCapacityGB READ diskTotalCapacityGB NOTIFY diskUsageChanged)
    Q_PROPERTY(double diskTotalFreeGB READ diskTotalFreeGB NOTIFY diskUsageChanged)
    Q_PROPERTY(QString diskDriveSummary READ diskDriveSummary NOTIFY diskUsageChanged)
    Q_PROPERTY(double netDownloadSpeed READ netDownloadSpeed NOTIFY netDownloadSpeedChanged)
    Q_PROPERTY(double netUploadSpeed READ netUploadSpeed NOTIFY netUploadSpeedChanged)

    // Kernel & Memory Commit Metrics
    Q_PROPERTY(double commitTotalGB READ commitTotalGB NOTIFY commitStatsChanged)
    Q_PROPERTY(double commitLimitGB READ commitLimitGB NOTIFY commitStatsChanged)
    Q_PROPERTY(double commitUsagePercent READ commitUsagePercent NOTIFY commitStatsChanged)
    Q_PROPERTY(double commitPeakGB READ commitPeakGB NOTIFY commitStatsChanged)
    Q_PROPERTY(double kernelPagedMB READ kernelPagedMB NOTIFY commitStatsChanged)
    Q_PROPERTY(double kernelNonpagedMB READ kernelNonpagedMB NOTIFY commitStatsChanged)
    Q_PROPERTY(int processCount READ processCount NOTIFY systemCountsChanged)
    Q_PROPERTY(int threadCount READ threadCount NOTIFY systemCountsChanged)
    Q_PROPERTY(int handleCount READ handleCount NOTIFY systemCountsChanged)

    // Hardware Details
    Q_PROPERTY(QString cpuModel READ cpuModel CONSTANT)
    Q_PROPERTY(int cpuBaseClockMHz READ cpuBaseClockMHz CONSTANT)
    Q_PROPERTY(QString gpuModel READ gpuModel CONSTANT)
    Q_PROPERTY(QString gpuBackend READ gpuBackend CONSTANT)
    Q_PROPERTY(QString motherboardModel READ motherboardModel CONSTANT)
    Q_PROPERTY(QString biosVersion READ biosVersion CONSTANT)
    Q_PROPERTY(int ramSpeed READ ramSpeed CONSTANT)

public:
    explicit SystemMonitor(std::shared_ptr<Rathon::UseCases::SystemUseCases> useCases, QObject *parent = nullptr);
    ~SystemMonitor() override;

    double cpuUsage() const { return m_snapshot.cpuUsage; }
    double ramUsage() const { return m_snapshot.ramUsage; }
    double ramTotal() const { return m_snapshot.ramTotal; }
    double ramUsed() const { return m_snapshot.ramUsed; }
    double gpuUsage() const { return m_snapshot.gpuUsage; }
    double gpuTemp() const { return m_snapshot.gpuTemp; }
    double gpuVramUsage() const { return m_snapshot.gpuVramUsage; }
    double gpuVramTotal() const { return m_snapshot.gpuVramTotal; }
    double gpuVramUsed() const { return m_snapshot.gpuVramUsed; }

    QString uptime() const { return QString::fromStdString(m_snapshot.uptime); }
    double diskReadSpeed() const { return m_snapshot.diskReadSpeed; }
    double diskWriteSpeed() const { return m_snapshot.diskWriteSpeed; }
    double diskUsage() const { return m_snapshot.diskUsage; }
    double diskTotalCapacityGB() const { return m_snapshot.diskTotalCapacityGB; }
    double diskTotalFreeGB() const { return m_snapshot.diskTotalFreeGB; }
    QString diskDriveSummary() const { return QString::fromStdString(m_snapshot.diskDriveSummary); }
    double netDownloadSpeed() const { return m_snapshot.netDownloadSpeed; }
    double netUploadSpeed() const { return m_snapshot.netUploadSpeed; }

    double commitTotalGB() const { return m_snapshot.commitTotalGB; }
    double commitLimitGB() const { return m_snapshot.commitLimitGB; }
    double commitUsagePercent() const { return m_snapshot.commitUsagePercent; }
    double commitPeakGB() const { return m_snapshot.commitPeakGB; }
    double kernelPagedMB() const { return m_snapshot.kernelPagedMB; }
    double kernelNonpagedMB() const { return m_snapshot.kernelNonpagedMB; }
    int processCount() const { return m_snapshot.processCount; }
    int threadCount() const { return m_snapshot.threadCount; }
    int handleCount() const { return m_snapshot.handleCount; }

    QString cpuModel() const { return QString::fromStdString(m_snapshot.cpuModel); }
    int cpuBaseClockMHz() const { return m_snapshot.cpuBaseClockMHz; }
    QString gpuModel() const { return QString::fromStdString(m_snapshot.gpuModel); }
    QString gpuBackend() const { return QString::fromStdString(m_snapshot.gpuBackend); }
    QString motherboardModel() const { return QString::fromStdString(m_snapshot.motherboardModel); }
    QString biosVersion() const { return QString::fromStdString(m_snapshot.biosVersion); }
    int ramSpeed() const { return m_snapshot.ramSpeed; }

signals:
    void cpuUsageChanged();
    void ramUsageChanged();
    void ramTotalChanged();
    void ramUsedChanged();
    void gpuUsageChanged();
    void gpuTempChanged();
    void gpuVramUsageChanged();
    void gpuVramTotalChanged();
    void gpuVramUsedChanged();

    void uptimeChanged();
    void diskReadSpeedChanged();
    void diskWriteSpeedChanged();
    void diskUsageChanged();
    void netDownloadSpeedChanged();
    void netUploadSpeedChanged();

    void commitStatsChanged();
    void systemCountsChanged();

private slots:
    void sampleTelemetry();

private:
    void applySnapshot(const Rathon::Domain::SystemSnapshot& snapshot);

    std::shared_ptr<Rathon::UseCases::SystemUseCases> m_useCases;
    std::unique_ptr<Rathon::Common::BoundedExecutor> m_executor;
    std::shared_ptr<Rathon::Common::ReactiveStream<Rathon::Domain::SystemSnapshot>> m_stream;
    Rathon::Common::ReactiveStream<Rathon::Domain::SystemSnapshot>::SubscriptionId m_subId = 0;

    Rathon::Domain::SystemSnapshot m_snapshot;
    QTimer *m_timer = nullptr;
    std::atomic<bool> m_isSampling{false};
};
