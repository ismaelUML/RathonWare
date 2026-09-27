#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>
#include <memory>
#include "usecases/file_unlocker_usecases.h"

// FileUnlocker Driving Adapter bridging FileUnlockerUseCases into Qt Quick / QML.
// Zero direct Win32 / RestartManager dependencies.
class FileUnlocker : public QObject
{
    Q_OBJECT

public:
    explicit FileUnlocker(std::shared_ptr<Rathon::UseCases::FileUnlockerUseCases> useCases, QObject *parent = nullptr);
    ~FileUnlocker() override = default;

    Q_INVOKABLE QVariantList findLockingProcesses(const QString& filePath);
    Q_INVOKABLE bool unlockFile(const QString& filePath);
    Q_INVOKABLE bool killLockingProcess(int pid);

private:
    std::shared_ptr<Rathon::UseCases::FileUnlockerUseCases> m_useCases;
};
