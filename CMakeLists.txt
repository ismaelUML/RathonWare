cmake_minimum_required(VERSION 3.16)

project(RathonWare LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Target Windows 10 / 11 to expose modern Win32 APIs (e.g. MIB_IF_TABLE2, RestartManager)
add_compile_definitions(_WIN32_WINNT=0x0A00 WINVER=0x0A00)

# Qt 6 configuration
find_package(Qt6 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2)

qt_standard_project_setup()

# Hexagonal Core Sources: Domain, Ports, UseCases, Driven Adapters, and Driving Controllers
set(SOURCES
    src/main.cpp
    src/common/circuit_breaker.h
    src/common/bounded_executor.h
    src/common/ring_buffer.h
    src/common/cancellation_token.h
    src/common/reactive_stream.h

    src/domain/entities.h
    src/domain/process_tree.h
    src/domain/process_tree.cpp

    src/ports/driven_ports.h

    src/usecases/process_usecases.h
    src/usecases/process_usecases.cpp
    src/usecases/system_usecases.h
    src/usecases/gpu_usecases.h
    src/usecases/cpu_usecases.h
    src/usecases/port_usecases.h
    src/usecases/file_unlocker_usecases.h

    src/adapters/driven/win32/win32_process_adapter.h
    src/adapters/driven/win32/win32_process_adapter.cpp
    src/adapters/driven/win32/win32_system_adapter.h
    src/adapters/driven/win32/win32_system_adapter.cpp
    src/adapters/driven/win32/win32_gpu_adapter.h
    src/adapters/driven/win32/win32_gpu_adapter.cpp
    src/adapters/driven/win32/win32_cpu_adapter.h
    src/adapters/driven/win32/win32_cpu_adapter.cpp
    src/adapters/driven/win32/win32_port_adapter.h
    src/adapters/driven/win32/win32_port_adapter.cpp
    src/adapters/driven/win32/win32_file_unlocker_adapter.h
    src/adapters/driven/win32/win32_file_unlocker_adapter.cpp

    src/system_monitor.h
    src/system_monitor.cpp
    src/process_model.h
    src/process_model.cpp
    src/port_manager.h
    src/port_manager.cpp
    src/file_unlocker.h
    src/file_unlocker.cpp
    src/gpu_monitor.h
    src/gpu_monitor.cpp
    src/cpu_topology.h
    src/cpu_topology.cpp
    src/resources.rc
)

# QML files
set(QML_FILES
    qml/main.qml
    qml/DashboardPage.qml
    qml/ProcessPage.qml
    qml/GpuTelemetryPage.qml
    qml/CpuTopologyPage.qml
    qml/CommandPalette.qml
    qml/FileUnlockerDialog.qml
    qml/SpecsPage.qml
    qml/SensorsPage.qml
    qml/GraphsPage.qml
    qml/AlertsPage.qml
)

qt_add_executable(RathonWare
    ${SOURCES}
)

target_include_directories(RathonWare PRIVATE src)

# Set QML module name
qt_add_qml_module(RathonWare
    URI RathonWare
    VERSION 1.0
    QML_FILES ${QML_FILES}
    RESOURCES
        assets/logo.png
)

# Requirement 3: Silent Subprocess & Native Windows Subsystem execution
set_target_properties(RathonWare PROPERTIES WIN32_EXECUTABLE TRUE)

# Windows specific system library linkings.
# Note: nvml is intentionally NOT linked statically here. It is loaded dynamically at runtime
# via LoadLibraryW("nvml.dll") so RathonWare can launch cleanly on AMD Radeon and Intel Arc machines.
if(WIN32)
    target_link_libraries(RathonWare PRIVATE pdh psapi iphlpapi rstrtmgr ole32 oleaut32 wbemuuid ws2_32 dxgi)
endif()

target_link_libraries(RathonWare PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
)

# Compiler flags
if(MSVC)
    target_compile_options(RathonWare PRIVATE /W4)
else()
    target_compile_options(RathonWare PRIVATE -Wall -Wextra -Wno-cast-function-type)
endif()

# -------------------------------------------------------------
# Requirement 7: Automated Unit Testing Suite
# -------------------------------------------------------------
enable_testing()

add_executable(RathonWareTests
    tests/test_domain.cpp
    src/domain/process_tree.cpp
)

target_include_directories(RathonWareTests PRIVATE src)

if(MSVC)
    target_compile_options(RathonWareTests PRIVATE /W4)
else()
    target_compile_options(RathonWareTests PRIVATE -Wall -Wextra -Wno-cast-function-type)
endif()

if(MINGW)
    target_link_options(RathonWareTests PRIVATE -static -static-libgcc -static-libstdc++)
endif()

add_test(NAME DomainAndResilienceTests COMMAND RathonWareTests)
