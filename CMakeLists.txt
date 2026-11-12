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

# Reproducible dependency management (ArduinoJson, Unity)
# See cmake/dependencies.cmake for pinned versions matching platformio.ini
include(cmake/dependencies.cmake)

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/test
)

# Device profiles are generated as part of every CMake build, just as they are
# in PlatformIO. A clean checkout therefore never depends on a manual generator
# invocation or on a stale generated table.
find_package(Python3 3.11 REQUIRED COMPONENTS Interpreter)
file(GLOB_RECURSE DEVICE_PROFILE_INPUTS CONFIGURE_DEPENDS
    ${CMAKE_SOURCE_DIR}/profiles/*.toml
)
set(GENERATED_PROFILES_CPP
    ${CMAKE_SOURCE_DIR}/src/drivers/modbus_profile/profiles_generated.cpp
)
add_custom_command(
    OUTPUT ${GENERATED_PROFILES_CPP}
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/gen_profiles.py
    DEPENDS
        ${CMAKE_SOURCE_DIR}/tools/gen_profiles.py
        ${CMAKE_SOURCE_DIR}/src/device/measurement.h
        ${CMAKE_SOURCE_DIR}/src/device/command.h
        ${DEVICE_PROFILE_INPUTS}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Validating and generating Modbus device profiles"
    VERBATIM
)
add_custom_target(generate-profiles DEPENDS ${GENERATED_PROFILES_CPP})

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
    src/config/configuration_store.cpp
    src/config/host_config_backend.cpp
    src/protocols/modbus/modbus_rtu.cpp
    src/protocols/modbus/modbus_client.cpp
    src/protocols/pmu/pmu_protocol.cpp
    src/protocols/pmu/pmu_transaction.cpp
    src/drivers/driver_registry.cpp
    src/drivers/discovery_engine.cpp
    src/drivers/modbus_profile/descriptor.cpp
    src/drivers/modbus_profile/modbus_profile_driver.cpp
    src/drivers/modbus_profile/profile_tables.cpp
    ${GENERATED_PROFILES_CPP}
    src/drivers/modbus_profile/register_dump.cpp
    src/diagnostics/breadcrumbs.cpp
    src/diagnostics/bus_tap.cpp
    src/diagnostics/coredump.cpp
    src/diagnostics/diagnostics.cpp
    src/diagnostics/frame_capture.cpp
    src/diagnostics/logger.cpp
    src/diagnostics/log_timestamp.cpp
    src/diagnostics/log_buffer.cpp
    src/network/ipv4.cpp
    src/relays/drm.cpp
    src/drivers/afore/afore_telemetry_engine.cpp
    src/drivers/afore/afore_v4_mqtt.cpp
    src/ems/shadow_ems_types.cpp
    src/ems/shadow_ems_controller.cpp
    src/ems/power_balance_evaluator.cpp
)

# Static check: reject dummy stub implementations
foreach(SOURCE_FILE ${COMMON_SOURCES})
    if(SOURCE_FILE MATCHES "net_stubs\\.cpp")
        message(FATAL_ERROR "COMMON_SOURCES contains net_stubs.cpp - use real implementations from src/network/ipv4.cpp and src/relays/drm.cpp instead")
    endif()
endforeach()

# M2: Afore Shadow build option - pass AFORE_SHADOW_BUILD to code if enabled
# Usage: cmake -DAFORE_SHADOW_BUILD=ON ..
# Default: OFF (standard build with all capabilities)
option(AFORE_SHADOW_BUILD "Build Afore Shadow read-only daemon (M2 hardening)" OFF)

# M2: Sources excluded from AFORE_SHADOW_BUILD (write-capable components)
# These files enable write operations and are physically excluded when building
# the Afore Shadow read-only daemon to prevent any write capability at link time.
if(NOT AFORE_SHADOW_BUILD)
    list(APPEND COMMON_SOURCES
        src/commands/command_dispatcher.cpp
        src/commands/command_queue.cpp
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

add_dependencies(heliograph-daemon generate-profiles)
target_compile_definitions(heliograph-daemon PRIVATE
    ENABLE_DRIVER_MODBUS_PROFILE=1
)

if(AFORE_SHADOW_BUILD)
    target_compile_definitions(heliograph-daemon PRIVATE AFORE_SHADOW_BUILD=1)
    message(STATUS "M2: Building Afore Shadow read-only daemon (write sources excluded)")
endif()

# Link pthread for std::mutex and std::timed_mutex, and ArduinoJson for JSON parsing
target_link_libraries(heliograph-daemon PRIVATE pthread ArduinoJson)

if(BUILD_TESTING)
    enable_testing()

    # Each host source owns main(), so each is a separate executable.
    add_executable(heliograph-tests
        test/host/test_readonly_guard.cpp
        src/drivers/mock/mock_driver.cpp
        ${COMMON_SOURCES}
    )
    target_compile_definitions(heliograph-tests PRIVATE
        ENABLE_DRIVER_MODBUS_PROFILE=1
        ENABLE_DRIVER_MOCK=1
    )
    add_dependencies(heliograph-tests generate-profiles)
    target_include_directories(heliograph-tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(heliograph-tests PRIVATE pthread ArduinoJson)
    add_test(NAME readonly-guard-tests COMMAND heliograph-tests)

    add_executable(test-posix-serial
        test/host/test_posix_serial.cpp
        ${COMMON_SOURCES}
    )
    add_dependencies(test-posix-serial generate-profiles)
    target_compile_definitions(test-posix-serial PRIVATE ENABLE_DRIVER_MODBUS_PROFILE=1)
    target_link_libraries(test-posix-serial PRIVATE pthread ArduinoJson)
    add_test(NAME posix-serial-tests COMMAND test-posix-serial)

    add_executable(test-host-config
        test/host/test_host_config.cpp
        ${COMMON_SOURCES}
    )
    add_dependencies(test-host-config generate-profiles)
    target_compile_definitions(test-host-config PRIVATE ENABLE_DRIVER_MODBUS_PROFILE=1)
    target_link_libraries(test-host-config PRIVATE pthread ArduinoJson)
    add_test(NAME host-config-tests COMMAND test-host-config)

    # Afore CP3 Telemetry Engine tests
    add_executable(test-afore-telemetry
        test/test_afore_telemetry/test_main.cpp
        ${COMMON_SOURCES}
    )
    add_dependencies(test-afore-telemetry generate-profiles)
    target_compile_definitions(test-afore-telemetry PRIVATE
        ENABLE_DRIVER_MODBUS_PROFILE=1
        UNITY_INCLUDE_DOUBLE=1
        UNITY_DOUBLE_PRECISION=1e-12
    )
    target_include_directories(test-afore-telemetry PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(test-afore-telemetry PRIVATE pthread ArduinoJson unity)
    add_test(NAME afore-telemetry-tests COMMAND test-afore-telemetry)

    # Milestone M11: 7-Day Endurance Simulation test
    add_executable(test-endurance-7d
        test/test_endurance_7d/test_main.cpp
        src/ems/power_balance_evaluator.cpp
        ${COMMON_SOURCES}
    )
    add_dependencies(test-endurance-7d generate-profiles)
    target_compile_definitions(test-endurance-7d PRIVATE
        ENABLE_DRIVER_MODBUS_PROFILE=1
        UNITY_INCLUDE_DOUBLE=1
        UNITY_DOUBLE_PRECISION=1e-12
    )
    target_include_directories(test-endurance-7d PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(test-endurance-7d PRIVATE pthread ArduinoJson unity)
    add_test(NAME endurance-7d-tests COMMAND test-endurance-7d)

    # Milestone M12: Waveshare ESP32-S3-RS485-CAN HAL test
    add_executable(test-waveshare-hal
        test/test_waveshare_hal/test_main.cpp
        ${COMMON_SOURCES}
    )
    add_dependencies(test-waveshare-hal generate-profiles)
    target_compile_definitions(test-waveshare-hal PRIVATE
        HELIOGRAPH_BOARD_RS485_CAN=1
        UNITY_INCLUDE_DOUBLE=1
        UNITY_DOUBLE_PRECISION=1e-12
    )
    target_include_directories(test-waveshare-hal PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(test-waveshare-hal PRIVATE pthread ArduinoJson unity)
    add_test(NAME waveshare-hal-tests COMMAND test-waveshare-hal)

    # M4 pure target-aware EMS core: intentionally isolated from COMMON_SOURCES so
    # this target cannot acquire transport, network, serial, protocol, or driver I/O.
    add_library(target-aware-ems-core STATIC
        src/ems/target_aware_ems.cpp
    )
    target_include_directories(target-aware-ems-core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

    add_executable(test-target-aware-ems
        test/test_target_aware_ems/test_main.cpp
    )
    target_link_libraries(test-target-aware-ems PRIVATE target-aware-ems-core)
    add_test(NAME target-aware-ems-tests COMMAND test-target-aware-ems)

    # M5 pure historical Home Assistant replay engine: offline replay, no network or transport I/O
    add_library(ha-replay-core STATIC
        src/ems/ha_replay_engine.cpp
    )
    target_include_directories(ha-replay-core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(ha-replay-core PUBLIC target-aware-ems-core ArduinoJson)

    add_executable(test-ha-replay
        test/test_ha_replay/test_main.cpp
    )
    target_link_libraries(test-ha-replay PRIVATE ha-replay-core)
    add_test(NAME test-ha-replay COMMAND test-ha-replay WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

    # Shadow EMS CP4 tests
    add_executable(test-shadow-ems
        test/test_shadow_ems/test_main.cpp
        ${COMMON_SOURCES}
    )
    add_dependencies(test-shadow-ems generate-profiles)
    target_compile_definitions(test-shadow-ems PRIVATE
        ENABLE_DRIVER_MODBUS_PROFILE=1
        UNITY_INCLUDE_DOUBLE=1
        UNITY_DOUBLE_PRECISION=1e-12
    )
    target_include_directories(test-shadow-ems PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(test-shadow-ems PRIVATE pthread ArduinoJson unity)
    add_test(NAME shadow-ems-tests COMMAND test-shadow-ems)

    # Power Balance & Sign Convention M8 tests
    add_executable(test-power-balance
        test/test_power_balance/test_main.cpp
        src/ems/power_balance_evaluator.cpp
    )
    target_include_directories(test-power-balance PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    add_test(NAME power-balance-tests COMMAND test-power-balance)

    # Secure Provisioning tests
    add_executable(test-secure-provisioning
        test/test_secure_provisioning/test_main.cpp
    )
    target_include_directories(test-secure-provisioning PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(test-secure-provisioning PRIVATE unity pthread)
    add_test(NAME secure-provisioning-tests COMMAND test-secure-provisioning)
endif()

# Install targets
install(TARGETS heliograph-daemon DESTINATION bin)
