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

# M2: Conditional compilation for Afore Shadow read-only build
# When AFORE_SHADOW_BUILD=1, exclude all write-capable sources to enforce
# structural read-only guarantee at compile time.

# Common host-compilable sources (aligned with platformio.ini env:native build_src_filter)
set(COMMON_SOURCES
    src/device/capability.cpp
    src/device/command.cpp
    src/device/device_context.cpp
    src/device/device_state.cpp
    src/device/measurement.cpp
    src/device/telemetry_quality.cpp
    src/transport/transport.cpp
    src/transport/posix_serial_transport.cpp
    src/transport/readonly_guard.cpp
    src/state/state_store.cpp
    src/config/configuration.cpp
    src/config/host_config_backend.cpp
    src/protocols/modbus/modbus_rtu.cpp
    src/protocols/modbus/modbus_client.cpp
    src/app/afore_telemetry_engine.cpp
    src/app/afore_v4_mqtt.cpp
    src/ems/shadow_ems_types.cpp
    src/ems/shadow_ems_controller.cpp
)

# M2: Sources excluded from AFORE_SHADOW_BUILD (write-capable components)
# These files enable write operations and are physically excluded when building
# the Afore Shadow read-only daemon to prevent any write capability at link time.
if(NOT AFORE_SHADOW_BUILD)
    list(APPEND COMMON_SOURCES
        src/commands/command_dispatcher.cpp
        src/commands/command_queue.cpp
        src/relays/drm.cpp
        src/relays/relay_controller.cpp
        src/status/boot_button.cpp
        src/status/status_led.cpp
    )
endif()

# Host daemon executable (Phase 0 Shadow)
add_executable(heliograph-daemon
    src/host/main.cpp
    ${COMMON_SOURCES}
)

# M2: Afore Shadow build option - pass AFORE_SHADOW_BUILD to code if enabled
# Usage: cmake -DAFORE_SHADOW_BUILD=ON ..
# Default: OFF (standard build with all capabilities)
option(AFORE_SHADOW_BUILD "Build Afore Shadow read-only daemon (M2 hardening)" OFF)

target_compile_definitions(heliograph-daemon PRIVATE
    ENABLE_DRIVER_MODBUS_PROFILE=1
    ENABLE_DRIVER_SUNSPEC=1
    ENABLE_DRIVER_SOLARMAX=1
    ENABLE_DRIVER_MOCK=1
)

if(AFORE_SHADOW_BUILD)
    target_compile_definitions(heliograph-daemon PRIVATE AFORE_SHADOW_BUILD=1)
    message(STATUS "M2: Building Afore Shadow read-only daemon (write sources excluded)")
endif()

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

# Afore AF5K-SL profile tests (Phase 0 CP2)
add_executable(test-afore
    test/test_afore/test_main.cpp
    ${COMMON_SOURCES}
)

target_compile_definitions(test-afore PRIVATE
    ENABLE_DRIVER_MODBUS_PROFILE=1
    UNITY_INCLUDE_DOUBLE=1
    UNITY_DOUBLE_PRECISION=1e-12
)

target_link_libraries(test-afore pthread)

add_test(NAME afore-profile-tests COMMAND test-afore)

# Afore CP3 Telemetry Engine tests
add_executable(test-afore-telemetry
    test/test_afore_telemetry/test_main.cpp
    ${COMMON_SOURCES}
)

target_compile_definitions(test-afore-telemetry PRIVATE
    ENABLE_DRIVER_MODBUS_PROFILE=1
    UNITY_INCLUDE_DOUBLE=1
    UNITY_DOUBLE_PRECISION=1e-12
)

target_link_libraries(test-afore-telemetry pthread)

add_test(NAME afore-telemetry-tests COMMAND test-afore-telemetry)

# Shadow EMS CP4 tests
add_executable(test-shadow-ems
    test/test_shadow_ems/test_main.cpp
    ${COMMON_SOURCES}
)

target_compile_definitions(test-shadow-ems PRIVATE
    ENABLE_DRIVER_MODBUS_PROFILE=1
    UNITY_INCLUDE_DOUBLE=1
    UNITY_DOUBLE_PRECISION=1e-12
)

target_link_libraries(test-shadow-ems pthread)

add_test(NAME shadow-ems-tests COMMAND test-shadow-ems)

# Install targets
install(TARGETS heliograph-daemon DESTINATION bin)
