add_executable(scpdpi_service main.cpp console_ui.cpp ipc_server.cpp autostart.cpp startup_task.cpp service_client.cpp dpi_backend.cpp)
target_link_libraries(scpdpi_service PRIVATE scpdpi_core)
if(WIN32)
  target_link_libraries(scpdpi_service PRIVATE advapi32 ole32 oleaut32 taskschd shell32)
  add_executable(scpdpi_autostart WIN32 autostart_main.cpp)
  target_link_libraries(scpdpi_autostart PRIVATE advapi32)
  if(MSVC)
    target_compile_options(scpdpi_autostart PRIVATE /W4 /permissive-)
  else()
    target_compile_options(scpdpi_autostart PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow)
  endif()

  set(SCPDPI_BYPASS_VERSION "0.2.3rc3-2")
  set(SCPDPI_BYPASS_ARCHIVE "${CMAKE_BINARY_DIR}/_deps/goodbyedpi-${SCPDPI_BYPASS_VERSION}.zip")
  set(SCPDPI_BYPASS_URL "https://github.com/ValdikSS/GoodbyeDPI/releases/download/0.2.3rc3/goodbyedpi-${SCPDPI_BYPASS_VERSION}.zip")
  file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/_deps")
  file(DOWNLOAD "${SCPDPI_BYPASS_URL}" "${SCPDPI_BYPASS_ARCHIVE}"
    EXPECTED_HASH SHA256=37f96b32d050dadcc930a639eba68e1ccd57ed5c04a5f77dfca908f01905a4c5
    TLS_VERIFY ON STATUS SCPDPI_BYPASS_DOWNLOAD_STATUS)
  list(GET SCPDPI_BYPASS_DOWNLOAD_STATUS 0 SCPDPI_BYPASS_DOWNLOAD_CODE)
  if(NOT SCPDPI_BYPASS_DOWNLOAD_CODE EQUAL 0)
    list(GET SCPDPI_BYPASS_DOWNLOAD_STATUS 1 SCPDPI_BYPASS_DOWNLOAD_MESSAGE)
    message(FATAL_ERROR "Could not download the pinned Windows DPI backend: ${SCPDPI_BYPASS_DOWNLOAD_MESSAGE}")
  endif()

  set(SCPDPI_BYPASS_EXTRACT_DIR "${CMAKE_BINARY_DIR}/_deps/goodbyedpi")
  file(MAKE_DIRECTORY "${SCPDPI_BYPASS_EXTRACT_DIR}")
  execute_process(COMMAND "${CMAKE_COMMAND}" -E tar xf "${SCPDPI_BYPASS_ARCHIVE}"
    WORKING_DIRECTORY "${SCPDPI_BYPASS_EXTRACT_DIR}" RESULT_VARIABLE SCPDPI_BYPASS_EXTRACT_RESULT)
  if(NOT SCPDPI_BYPASS_EXTRACT_RESULT EQUAL 0)
    message(FATAL_ERROR "Could not extract the pinned Windows DPI backend archive")
  endif()

  set(SCPDPI_BYPASS_PACKAGE "${SCPDPI_BYPASS_EXTRACT_DIR}/goodbyedpi-${SCPDPI_BYPASS_VERSION}")
  set(SCPDPI_BYPASS_RUNTIME "$<TARGET_FILE_DIR:scpdpi_service>/scpdpi-dpi")
  add_custom_command(TARGET scpdpi_service POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${SCPDPI_BYPASS_RUNTIME}"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${SCPDPI_BYPASS_PACKAGE}/x86_64/goodbyedpi.exe" "${SCPDPI_BYPASS_RUNTIME}/goodbyedpi.exe"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${SCPDPI_BYPASS_PACKAGE}/x86_64/WinDivert.dll" "${SCPDPI_BYPASS_RUNTIME}/WinDivert.dll"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${SCPDPI_BYPASS_PACKAGE}/x86_64/WinDivert64.sys" "${SCPDPI_BYPASS_RUNTIME}/WinDivert64.sys"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${SCPDPI_BYPASS_PACKAGE}/russia-blacklist.txt" "${SCPDPI_BYPASS_RUNTIME}/russia-blacklist.txt"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${SCPDPI_BYPASS_PACKAGE}/russia-youtube.txt" "${SCPDPI_BYPASS_RUNTIME}/russia-youtube.txt"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/dpi/scpdpi-discord.txt" "${SCPDPI_BYPASS_RUNTIME}/scpdpi-discord.txt"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory "${SCPDPI_BYPASS_PACKAGE}/licenses" "${SCPDPI_BYPASS_RUNTIME}/licenses"
    COMMENT "Staging the pinned Windows DPI backend")
endif()
if(MSVC)
  target_compile_options(scpdpi_service PRIVATE /W4 /permissive-)
else()
  target_compile_options(scpdpi_service PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow)
endif()
