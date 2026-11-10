cmake_minimum_required(VERSION 3.16)
project(heliograph-host VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler warnings
add_compile_options(
    -Wall
    -Wextra
    -Werror
    $<$<CONFIG:Debug>:-g>
    $<$<CONFIG:Release>:-O2>
)

# Find ArduinoJson (header-only library, should be available via system package or FetchContent)
# For now, assume it's available via PlatformIO's libdeps or system install
# TODO: Add proper find_package or FetchContent for ArduinoJson if building standalone

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/test
)

# Common host-compilable sources (aligned with platformio.ini env:native build_src_filter)
set(COMMON_SOURCES
    src/device/capability.cpp
    src/device/command.cpp
    src/device/device_context.cpp
    src/device/device_state.cpp
    src/device/measurement.cpp
    src/transport/transport.cpp
    src/transport/posix_serial_transport.cpp
    src/transport/readonly_guard.cpp
    src/state/state_store.cpp
    src/commands/command_dispatcher.cpp
    src/commands/command_queue.cpp
    src/relays/drm.cpp
    src/relays/relay_controller.cpp
    src/status/boot_button.cpp
    src/status/status_led.cpp
    src/config/configuration.cpp
    src/config/host_config_backend.cpp
    src/protocols/modbus/modbus_rtu.cpp
    # Add more as needed from the native build filter
)

# Host daemon executable (Phase 0 Shadow)
add_executable(heliograph-daemon
    src/host/main.cpp
    ${COMMON_SOURCES}
)

target_compile_definitions(heliograph-daemon PRIVATE
    ENABLE_DRIVER_MODBUS_PROFILE=1
    ENABLE_DRIVER_SUNSPEC=1
    ENABLE_DRIVER_SOLARMAX=1
    ENABLE_DRIVER_MOCK=1
)

# Link pthread for std::mutex and std::timed_mutex
target_link_libraries(heliograph-daemon pthread)

# Unit tests executable
enable_testing()

add_executable(heliograph-tests
    test/host/test_posix_serial.cpp
    test/host/test_readonly_guard.cpp
    test/host/test_host_config.cpp
    ${COMMON_SOURCES}
)

target_compile_definitions(heliograph-tests PRIVATE
    ENABLE_DRIVER_MODBUS_PROFILE=1
    ENABLE_DRIVER_MOCK=1
    UNITY_INCLUDE_DOUBLE=1
    UNITY_DOUBLE_PRECISION=1e-12
)

target_link_libraries(heliograph-tests pthread)

add_test(NAME heliograph-unit-tests COMMAND heliograph-tests)

# Install targets
install(TARGETS heliograph-daemon DESTINATION bin)
