#include "system_monitor.h"
#include <QMetaObject>

SystemMonitor::SystemMonitor(std::shared_ptr<Rathon::UseCases::SystemUseCases> useCases, QObject *parent)
    : QObject(parent)
    , m_useCases(std::move(useCases))
    , m_executor(std::make_unique<Rathon::Common::BoundedExecutor>(1, 4))
    , m_stream(std::make_shared<Rathon::Common::ReactiveStream<Rathon::Domain::SystemSnapshot>>())
{
    // Subscribe to reactive stream
    m_subId = m_stream->subscribe([this](const Rathon::Domain::SystemSnapshot& snapshot) {
        QMetaObject::invokeMethod(this, [this, snapshot]() {
            applySnapshot(snapshot);
            m_isSampling.store(false);
        }, Qt::QueuedConnection);
    });

    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &SystemMonitor::sampleTelemetry);
    m_timer->start(1000);

    sampleTelemetry();
}

SystemMonitor::~SystemMonitor() {
    if (m_timer) m_timer->stop();
    if (m_stream && m_subId != 0) m_stream->unsubscribe(m_subId);
    if (m_executor) m_executor->shutdown();
}

void SystemMonitor::sampleTelemetry() {
    if (!m_useCases || !m_executor || !m_stream) return;

    bool expected = false;
    if (!m_isSampling.compare_exchange_strong(expected, true)) {
        return; // Avoid queuing duplicate tasks if a sample is already in flight
    }

    bool submitted = m_executor->submit([this]() {
        auto snapshot = m_useCases->getSnapshot();
        m_stream->publish(snapshot);
    });

    if (!submitted) {
        m_isSampling.store(false); // Backpressure drop
    }
}

void SystemMonitor::applySnapshot(const Rathon::Domain::SystemSnapshot& snapshot) {
    m_snapshot = snapshot;

    emit cpuUsageChanged();
    emit ramUsageChanged();
    emit ramTotalChanged();
    emit ramUsedChanged();
    emit gpuUsageChanged();
    emit gpuTempChanged();
    emit gpuVramUsageChanged();
    emit gpuVramTotalChanged();
    emit gpuVramUsedChanged();

    emit uptimeChanged();
    emit diskReadSpeedChanged();
    emit diskWriteSpeedChanged();
    emit diskUsageChanged();
    emit netDownloadSpeedChanged();
    emit netUploadSpeedChanged();

    emit commitStatsChanged();
    emit systemCountsChanged();
}
