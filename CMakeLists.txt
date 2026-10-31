cmake_minimum_required(VERSION 3.24)
project(ScpDPI VERSION 0.1.0 LANGUAGES CXX)

option(SCPDPI_BUILD_TESTS "Build ScpDPI tests" ON)
option(SCPDPI_BUILD_DESKTOP "Build the optional Qt desktop application when Qt is available" OFF)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(nlohmann_json CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)

add_subdirectory(src/core)
add_subdirectory(src/service)
find_package(Doxygen QUIET)
if(DOXYGEN_FOUND)
  add_custom_target(scpdpi_docs
    COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_SOURCE_DIR}/Doxyfile
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Generating ScpDPI API documentation")
endif()
if(SCPDPI_BUILD_DESKTOP)
  add_subdirectory(src/desktop)
else()
  add_custom_target(scpdpi_desktop COMMENT "Desktop build disabled by SCPDPI_BUILD_DESKTOP")
endif()
if(SCPDPI_BUILD_TESTS)
  include(CTest)
  enable_testing()
  add_subdirectory(tests)
endif()
