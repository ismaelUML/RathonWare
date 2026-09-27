#pragma once

#include <QAbstractListModel>
#include <QVector>
#include <QTimer>
#include <memory>
#include "usecases/process_usecases.h"
#include "common/bounded_executor.h"

// Reactive QAbstractListModel driving adapter bridging ProcessUseCases into Qt Quick / QML.
// Employs BoundedExecutor to run low-level PEB traversal asynchronously,
// ensuring the UI rendering thread never freezes even when compiling with hundreds of processes.
class ProcessModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(bool hasHybridCores READ hasHybridCores CONSTANT)

public:
    enum ProcessRoles {
        PidRole = Qt::UserRole + 1,
        ParentPidRole,
        NameRole,
        CpuRole,
        RamRole,
        PrivateRole,
        PeakRole,
        ThreadsRole,
        UsernameRole,
        CmdLineRole,
        PriorityRole,
        IsSuspendedRole,
        IsEcoQosRole
    };

    explicit ProcessModel(std::shared_ptr<Rathon::UseCases::ProcessUseCases> useCases, QObject *parent = nullptr);
    ~ProcessModel() override;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    bool hasHybridCores() const;

    Q_INVOKABLE void refresh();
    Q_INVOKABLE bool killProcess(int pid);
    Q_INVOKABLE bool killProcessTree(int pid);
    Q_INVOKABLE bool setPriority(int pid, int priorityClassValue);
    Q_INVOKABLE bool suspendProcess(int pid);
    Q_INVOKABLE bool resumeProcess(int pid);
    Q_INVOKABLE bool pinToECores(int pid);
    Q_INVOKABLE bool pinToPCores(int pid);
    Q_INVOKABLE bool resetAffinity(int pid);

private:
    void applyProcessUpdates(std::vector<Rathon::Domain::Process> newProcs);

    std::shared_ptr<Rathon::UseCases::ProcessUseCases> m_useCases;
    std::unique_ptr<Rathon::Common::BoundedExecutor> m_executor;
    std::vector<Rathon::Domain::Process> m_processes;
    QTimer *m_timer = nullptr;
    std::atomic<bool> m_isRefreshing{false};
};
