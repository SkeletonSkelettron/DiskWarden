cmake_minimum_required(VERSION 3.21)

project(DiskWarden VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(DISKWARDEN_BUILD_TESTS "Build the unit tests" ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets Qml Quick QuickControls2 DBus Sql)

qt_standard_project_setup(REQUIRES 6.5)

# Logic that doesn't depend on D-Bus or QML, shared with the tests.
add_library(diskwarden_core STATIC
    src/core/diskstats.cpp
    src/core/diskstats.h
    src/core/formatting.cpp
    src/core/formatting.h
    src/core/healthcalculator.cpp
    src/core/healthcalculator.h
    src/core/smartattribute.cpp
    src/core/smartattribute.h
    src/core/sysfs.cpp
    src/core/sysfs.h
)
target_include_directories(diskwarden_core PUBLIC src/core)
target_link_libraries(diskwarden_core PUBLIC Qt6::Core)

qt_add_executable(diskwarden
    src/main.cpp
)

set(QML_FILES
    qml/AboutDialog.qml
    qml/AlertsPage.qml
    qml/ChartPanel.qml
    qml/ConfigurationDialog.qml
    qml/DiskCard.qml
    qml/GradientBar.qml
    qml/HelpButton.qml
    qml/InformationPage.qml
    qml/LogPage.qml
    qml/Main.qml
    qml/OverviewPage.qml
    qml/PerformancePage.qml
    qml/SelfTestDialog.qml
    qml/SmartPage.qml
    qml/StatusIcon.qml
    qml/SurfaceTestWindow.qml
    qml/TabButtonHds.qml
    qml/TemperaturePage.qml
    qml/TextPanel.qml
    qml/ThemedWindow.qml
    qml/Theme.qml
    qml/ToolIconButton.qml
    qml/VolumeCard.qml
)
# Keep the QML files at the root of the module, next to the icons.
foreach(file ${QML_FILES})
    get_filename_component(name ${file} NAME)
    set_source_files_properties(${file} PROPERTIES QT_RESOURCE_ALIAS ${name})
endforeach()
set_source_files_properties(qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(diskwarden
    URI DiskWarden
    VERSION 1.0
    SOURCES
        src/app/appsettings.cpp
        src/app/appsettings.h
        src/app/chartitem.cpp
        src/app/chartitem.h
        src/app/disk.cpp
        src/app/disk.h
        src/app/diskmanager.cpp
        src/app/diskmanager.h
        src/app/historydb.cpp
        src/app/historydb.h
        src/app/infomodel.cpp
        src/app/infomodel.h
        src/app/logmodel.cpp
        src/app/logmodel.h
        src/app/notifier.cpp
        src/app/notifier.h
        src/app/smartattributemodel.cpp
        src/app/smartattributemodel.h
        src/app/surfacemapitem.cpp
        src/app/surfacemapitem.h
        src/app/surfacetest.cpp
        src/app/surfacetest.h
        src/app/traycontroller.cpp
        src/app/traycontroller.h
        src/app/udisks.cpp
        src/app/udisks.h
    QML_FILES ${QML_FILES}
    RESOURCES
        icons/about.svg
        icons/alerts.svg
        icons/check.svg
        icons/configure.svg
        icons/diskwarden.svg
        icons/error.svg
        icons/hdd.svg
        icons/help.svg
        icons/information.svg
        icons/log.svg
        icons/nvme.svg
        icons/overview.svg
        icons/performance.svg
        icons/refresh.svg
        icons/selftest-extended.svg
        icons/selftest-short.svg
        icons/smart.svg
        icons/ssd.svg
        icons/surface.svg
        icons/temperature.svg
        icons/unknown.svg
        icons/warning.svg
)

target_compile_definitions(diskwarden PRIVATE DISKWARDEN_VERSION="${PROJECT_VERSION}")
target_include_directories(diskwarden PRIVATE src/app src/core)
target_link_libraries(diskwarden PRIVATE
    diskwarden_core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::DBus
    Qt6::Sql
)

include(GNUInstallDirs)
install(TARGETS diskwarden RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES data/diskwarden.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES icons/diskwarden.svg DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)

if(DISKWARDEN_BUILD_TESTS)
    find_package(Qt6 REQUIRED COMPONENTS Test)
    enable_testing()
    foreach(test healthcalculator smartattribute diskstats formatting)
        qt_add_executable(tst_${test} tests/tst_${test}.cpp)
        target_link_libraries(tst_${test} PRIVATE diskwarden_core Qt6::Test)
        add_test(NAME ${test} COMMAND tst_${test})
    endforeach()
endif()
