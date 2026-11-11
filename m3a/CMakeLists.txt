# M3A Safety Core Test - Isolated Build
# Target: afore-m3a-readonly-tests
# Scope: ReadOnlyGuard validation ONLY

cmake_minimum_required(VERSION 3.16)
project(afore-m3a-safety LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Explicit source paths - NO globbing
set(M3A_PRODUCTION_SOURCES
    ../src/transport/readonly_guard.cpp
    ../src/protocols/modbus/modbus_rtu.cpp
    ../src/transport/transport.cpp
)

set(M3A_TEST_SOURCES
    test_readonly_guard_m3a.cpp
)

# Main test executable
add_executable(afore-m3a-readonly-tests 
    ${M3A_PRODUCTION_SOURCES}
    ${M3A_TEST_SOURCES}
)

# Target-local compile definitions
target_compile_definitions(afore-m3a-readonly-tests PRIVATE
    AFORE_SHADOW_BUILD=1
)

# Target-local compile options
target_compile_options(afore-m3a-readonly-tests PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Werror
)

# Target-local include directories
target_include_directories(afore-m3a-readonly-tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

# Register test with CTest
enable_testing()

add_test(
    NAME afore-m3a-readonly-tests
    COMMAND afore-m3a-readonly-tests
)

# Negative compile test subdirectory
add_subdirectory(negative_compile)

# Post-build message
add_custom_command(TARGET afore-m3a-readonly-tests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E echo "M3A safety core built successfully"
)
