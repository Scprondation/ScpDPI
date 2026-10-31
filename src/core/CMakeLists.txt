add_library(scpdpi_core
  src/common/ip_address.cpp
  src/common/logger.cpp
  src/config/config.cpp
  src/vpn/detector.cpp
  src/classifier/traffic_classifier.cpp
  src/policy/policy_engine.cpp
  src/diagnostics/diagnostics.cpp
  src/telemetry/telemetry.cpp
  src/engine/core_engine.cpp
  src/platform/platform_factory.cpp
  src/service/ipc_protocol.cpp
)
if(WIN32)
  target_sources(scpdpi_core PRIVATE ../platform/windows/interface_provider.cpp ../platform/windows/route_provider.cpp)
  target_link_libraries(scpdpi_core PRIVATE iphlpapi ws2_32)
  target_compile_definitions(scpdpi_core PRIVATE _WIN32_WINNT=0x0A00 WINVER=0x0A00)
elseif(UNIX)
  target_sources(scpdpi_core PRIVATE ../platform/linux/interface_provider.cpp ../platform/linux/route_provider.cpp)
endif()

target_include_directories(scpdpi_core PUBLIC include)
target_link_libraries(scpdpi_core PUBLIC nlohmann_json::nlohmann_json spdlog::spdlog)
target_compile_definitions(scpdpi_core PUBLIC SCPDPI_VERSION="${PROJECT_VERSION}")
if(MSVC)
  target_compile_options(scpdpi_core PRIVATE /W4 /permissive- /Zc:__cplusplus)
else()
  target_compile_options(scpdpi_core PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow)
endif()
