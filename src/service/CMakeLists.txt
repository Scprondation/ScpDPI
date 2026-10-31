add_executable(scpdpi_service main.cpp console_ui.cpp ipc_server.cpp autostart.cpp startup_task.cpp service_client.cpp dpi_backend.cpp dpi/packet_engine.cpp dpi/tls_client_hello.cpp dpi/quic_initial.cpp dpi/host_lists.cpp)
target_link_libraries(scpdpi_service PRIVATE scpdpi_core)
if(WIN32)
  target_link_libraries(scpdpi_service PRIVATE advapi32 ole32 oleaut32 taskschd shell32 iphlpapi bcrypt)
  add_executable(scpdpi_autostart WIN32 autostart_main.cpp)
  target_link_libraries(scpdpi_autostart PRIVATE advapi32)
  if(MSVC)
    target_compile_options(scpdpi_autostart PRIVATE /W4 /permissive-)
  else()
    target_compile_options(scpdpi_autostart PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow)
  endif()

  set(SCPDPI_WINDIVERT_VERSION "2.2.2-A")
  set(SCPDPI_WINDIVERT_ARCHIVE "${CMAKE_BINARY_DIR}/_deps/WinDivert-${SCPDPI_WINDIVERT_VERSION}.zip")
  set(SCPDPI_WINDIVERT_URL "https://github.com/basil00/WinDivert/releases/download/v2.2.2/WinDivert-${SCPDPI_WINDIVERT_VERSION}.zip")
  set(SCPDPI_RULIST_COMMIT "c5470dd0ce0c706c945a938f7694a2634e9d7ea9")
  set(SCPDPI_RULIST_FILE "${CMAKE_BINARY_DIR}/_deps/russia-registry-domains.txt")
  file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/_deps")
  file(DOWNLOAD "${SCPDPI_WINDIVERT_URL}" "${SCPDPI_WINDIVERT_ARCHIVE}"
    EXPECTED_HASH SHA256=63cb41763bb4b20f600b6de04e991a9c2be73279e317d4d82f237b150c5f3f15
    TLS_VERIFY ON STATUS SCPDPI_WINDIVERT_DOWNLOAD_STATUS)
  list(GET SCPDPI_WINDIVERT_DOWNLOAD_STATUS 0 SCPDPI_WINDIVERT_DOWNLOAD_CODE)
  if(NOT SCPDPI_WINDIVERT_DOWNLOAD_CODE EQUAL 0)
    list(GET SCPDPI_WINDIVERT_DOWNLOAD_STATUS 1 SCPDPI_WINDIVERT_DOWNLOAD_MESSAGE)
    message(FATAL_ERROR "Could not download the pinned WinDivert packet driver: ${SCPDPI_WINDIVERT_DOWNLOAD_MESSAGE}")
  endif()

  file(DOWNLOAD "https://raw.githubusercontent.com/bol-van/rulist/${SCPDPI_RULIST_COMMIT}/reestr_hostname_resolvable.txt" "${SCPDPI_RULIST_FILE}"
    EXPECTED_HASH SHA256=2c3e871bc8547e923fff557c3b157f492f74f5dedf9566f810e50d0552e36aba
    TLS_VERIFY ON STATUS SCPDPI_RULIST_DOWNLOAD_STATUS)
  list(GET SCPDPI_RULIST_DOWNLOAD_STATUS 0 SCPDPI_RULIST_DOWNLOAD_CODE)
  if(NOT SCPDPI_RULIST_DOWNLOAD_CODE EQUAL 0)
    list(GET SCPDPI_RULIST_DOWNLOAD_STATUS 1 SCPDPI_RULIST_DOWNLOAD_MESSAGE)
    message(FATAL_ERROR "Could not download the pinned Russian registry domain list: ${SCPDPI_RULIST_DOWNLOAD_MESSAGE}")
  endif()

  set(SCPDPI_WINDIVERT_EXTRACT_DIR "${CMAKE_BINARY_DIR}/_deps/windivert")
  file(MAKE_DIRECTORY "${SCPDPI_WINDIVERT_EXTRACT_DIR}")
  execute_process(COMMAND "${CMAKE_COMMAND}" -E tar xf "${SCPDPI_WINDIVERT_ARCHIVE}"
    WORKING_DIRECTORY "${SCPDPI_WINDIVERT_EXTRACT_DIR}" RESULT_VARIABLE SCPDPI_WINDIVERT_EXTRACT_RESULT)
  if(NOT SCPDPI_WINDIVERT_EXTRACT_RESULT EQUAL 0)
    message(FATAL_ERROR "Could not extract the pinned WinDivert package")
  endif()

  set(SCPDPI_WINDIVERT_PACKAGE "${SCPDPI_WINDIVERT_EXTRACT_DIR}/WinDivert-${SCPDPI_WINDIVERT_VERSION}")
  target_include_directories(scpdpi_service PRIVATE "${SCPDPI_WINDIVERT_PACKAGE}/include")
  target_link_libraries(scpdpi_service PRIVATE "${SCPDPI_WINDIVERT_PACKAGE}/x64/WinDivert.lib")
  set(SCPDPI_DPI_RUNTIME "$<TARGET_FILE_DIR:scpdpi_service>/scpdpi-dpi")
  add_custom_command(TARGET scpdpi_service POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E remove_directory "${SCPDPI_DPI_RUNTIME}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${SCPDPI_DPI_RUNTIME}"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${SCPDPI_WINDIVERT_PACKAGE}/x64/WinDivert.dll" "$<TARGET_FILE_DIR:scpdpi_service>/WinDivert.dll"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${SCPDPI_WINDIVERT_PACKAGE}/x64/WinDivert64.sys" "$<TARGET_FILE_DIR:scpdpi_service>/WinDivert64.sys"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${SCPDPI_WINDIVERT_PACKAGE}/LICENSE" "$<TARGET_FILE_DIR:scpdpi_service>/WinDivert-LICENSE.txt"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${SCPDPI_RULIST_FILE}" "${SCPDPI_DPI_RUNTIME}/russia-registry-domains.txt"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/dpi/russia-youtube.txt" "${SCPDPI_DPI_RUNTIME}/russia-youtube.txt"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/dpi/russia-discord.txt" "${SCPDPI_DPI_RUNTIME}/russia-discord.txt"
    COMMENT "Staging ScpDPI packet engine and Russian host lists")
endif()
if(MSVC)
  target_compile_options(scpdpi_service PRIVATE /W4 /permissive-)
else()
  target_compile_options(scpdpi_service PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow)
endif()
