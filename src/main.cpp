#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QIcon>
#include <memory>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

// Hexagonal Architecture: Driven Win32 Adapters
#include "adapters/driven/win32/win32_process_adapter.h"
#include "adapters/driven/win32/win32_system_adapter.h"
#include "adapters/driven/win32/win32_gpu_adapter.h"
#include "adapters/driven/win32/win32_cpu_adapter.h"
#include "adapters/driven/win32/win32_port_adapter.h"
#include "adapters/driven/win32/win32_file_unlocker_adapter.h"

// Hexagonal Architecture: Use Cases (Orchestrators)
#include "usecases/process_usecases.h"
#include "usecases/system_usecases.h"
#include "usecases/gpu_usecases.h"
#include "usecases/cpu_usecases.h"
#include "usecases/port_usecases.h"
#include "usecases/file_unlocker_usecases.h"

// Hexagonal Architecture: Driving Qt/QML Controllers
#include "system_monitor.h"
#include "process_model.h"
#include "port_manager.h"
#include "file_unlocker.h"
#include "gpu_monitor.h"
#include "cpu_topology.h"

// Requirement 3: Protection of Standard Outputs in Silent / Detached Mode.
// In detached GUI environments, writing to invalid or null stdout/stderr handles triggers fatal crashes.
// This safe message handler redirects all log traffic securely to Windows OutputDebugStringW.
static void safeLogHandler(QtMsgType, const QMessageLogContext&, const QString& msg) {
#ifdef _WIN32
    std::wstring wmsg = msg.toStdWString() + L"\n";
    OutputDebugStringW(wmsg.c_str());
#endif
}

int main(int argc, char *argv[])
{
    // Install defensive logger for decoupled/silent GUI execution
    qInstallMessageHandler(safeLogHandler);

    // Fix High DPI fractional scaling fuzziness on modern Windows displays (125%, 150%, 175%).
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::Round);

    QGuiApplication app(argc, argv);

    // Retro Y2K custom styling
    QQuickStyle::setStyle("Basic");

    app.setWindowIcon(QIcon(":/RathonWare/assets/logo.png"));
    app.setOrganizationName("Rathon");
    app.setApplicationName("RathonWare");

    // -------------------------------------------------------------
    // Composition Root: Hexagonal Dependency Injection Wiring
    // -------------------------------------------------------------

    // 1. Driven Adapters (Win32 Infrastructure)
    auto win32Process = std::make_shared<Rathon::Adapters::Driven::Win32ProcessAdapter>();
    auto win32System = std::make_shared<Rathon::Adapters::Driven::Win32SystemAdapter>();
    auto win32Gpu = std::make_shared<Rathon::Adapters::Driven::Win32GpuAdapter>();
    auto win32Cpu = std::make_shared<Rathon::Adapters::Driven::Win32CpuAdapter>();
    auto win32Port = std::make_shared<Rathon::Adapters::Driven::Win32PortAdapter>();
    auto win32FileUnlocker = std::make_shared<Rathon::Adapters::Driven::Win32FileUnlockerAdapter>();

    // 2. Use Cases (Application Core)
    auto processUseCases = std::make_shared<Rathon::UseCases::ProcessUseCases>(win32Process);
    auto systemUseCases = std::make_shared<Rathon::UseCases::SystemUseCases>(win32System);
    auto gpuUseCases = std::make_shared<Rathon::UseCases::GpuUseCases>(win32Gpu);
    auto cpuUseCases = std::make_shared<Rathon::UseCases::CpuUseCases>(win32Cpu);
    auto portUseCases = std::make_shared<Rathon::UseCases::PortUseCases>(win32Port);
    auto fileUnlockerUseCases = std::make_shared<Rathon::UseCases::FileUnlockerUseCases>(win32FileUnlocker);

    // 3. Driving Adapters (Qt / QML Presentation Controllers)
    SystemMonitor monitor(systemUseCases);
    ProcessModel processModel(processUseCases);
    PortManager portManager(portUseCases);
    FileUnlocker fileUnlocker(fileUnlockerUseCases);

    GpuProcessModel gpuProcessModel(gpuUseCases);
    GpuMonitor gpuMonitor(gpuUseCases, &gpuProcessModel);

    CpuCoreModel cpuCoreModel;
    CpuTopology cpuTopology(cpuUseCases, processUseCases, &cpuCoreModel);

    // 4. Bind driving controllers to QML root context
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("systemMonitor", &monitor);
    engine.rootContext()->setContextProperty("processModel", &processModel);
    engine.rootContext()->setContextProperty("portManager", &portManager);
    engine.rootContext()->setContextProperty("fileUnlocker", &fileUnlocker);
    engine.rootContext()->setContextProperty("gpuProcessModel", &gpuProcessModel);
    engine.rootContext()->setContextProperty("gpuMonitor", &gpuMonitor);
    engine.rootContext()->setContextProperty("cpuCoreModel", &cpuCoreModel);
    engine.rootContext()->setContextProperty("cpuTopology", &cpuTopology);

    const QUrl url(QStringLiteral("qrc:/RathonWare/qml/main.qml"));
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
                     &app, [url](QObject *obj, const QUrl &objUrl) {
        if (!obj && url == objUrl)
            QCoreApplication::exit(-1);
    }, Qt::QueuedConnection);

    engine.load(url);

    return app.exec();
}
