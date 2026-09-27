#pragma once

#include <QObject>
#include <QVariantList>
#include <memory>
#include "usecases/port_usecases.h"

// PortManager Driving Adapter bridging PortUseCases into Qt Quick / QML.
// Zero direct Win32 dependencies.
class PortManager : public QObject
{
    Q_OBJECT

public:
    explicit PortManager(std::shared_ptr<Rathon::UseCases::PortUseCases> useCases, QObject *parent = nullptr);
    ~PortManager() override = default;

    Q_INVOKABLE QVariantList getProcessesByPort(int port);
    Q_INVOKABLE QVariantList getAllListeningPorts();
    Q_INVOKABLE bool killProcessOnPort(int port);
    Q_INVOKABLE bool killProcessByPid(int pid);

private:
    std::shared_ptr<Rathon::UseCases::PortUseCases> m_useCases;
};
