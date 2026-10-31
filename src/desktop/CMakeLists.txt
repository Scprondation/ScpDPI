find_package(Qt6 QUIET COMPONENTS Core Gui Qml Quick QuickControls2 Network)
if(Qt6_FOUND)
  qt_standard_project_setup(REQUIRES 6.5)
  qt_add_executable(scpdpi_desktop WIN32
    main.cpp
    dashboard_controller.cpp
    dashboard_controller.hpp
  )
  set_source_files_properties(qml/theme/Palette.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
  qt_add_qml_module(scpdpi_desktop
    URI ScpDPI
    VERSION 1.0
    QML_FILES
      qml/Main.qml
      qml/components/Sidebar.qml
      qml/components/Header.qml
      qml/components/StatusIndicator.qml
      qml/components/StatusCard.qml
      qml/components/MetricCard.qml
      qml/components/InterfaceCard.qml
      qml/components/RouteTable.qml
      qml/components/ProfileSelector.qml
      qml/components/LogViewer.qml
      qml/pages/DashboardPage.qml
      qml/pages/NetworkPage.qml
      qml/pages/VpnPage.qml
      qml/pages/ProfilesPage.qml
      qml/pages/DiagnosticsPage.qml
      qml/pages/LogsPage.qml
      qml/pages/SettingsPage.qml
      qml/pages/AboutPage.qml
      qml/theme/Palette.qml
  )
  target_link_libraries(scpdpi_desktop PRIVATE scpdpi_core Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickControls2 Qt6::Network)
  if(MSVC)
    target_compile_options(scpdpi_desktop PRIVATE /W4 /permissive-)
  else()
    target_compile_options(scpdpi_desktop PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow)
  endif()
else()
  message(STATUS "Qt 6 was not found; scpdpi_desktop source is present but its executable is unavailable")
  add_custom_target(scpdpi_desktop COMMENT "Qt 6 is required to build the ScpDPI desktop application")
endif()
