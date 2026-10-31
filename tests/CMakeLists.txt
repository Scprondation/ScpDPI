find_package(GTest CONFIG REQUIRED)
add_executable(scpdpi_tests
  core/core_tests.cpp
  core/config_tests.cpp
  core/ipc_tests.cpp
  integration/platform_tests.cpp
)
target_link_libraries(scpdpi_tests PRIVATE scpdpi_core GTest::gtest_main)
if(MSVC)
  target_compile_options(scpdpi_tests PRIVATE /W4 /permissive-)
else()
  target_compile_options(scpdpi_tests PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow)
endif()
add_test(NAME scpdpi_tests COMMAND scpdpi_tests)
