# SPDX-License-Identifier: MIT
#
# Heliograph host build dependencies.
#
# Reproducible dependency management for standalone CMake builds (Linux/macOS host).
# PlatformIO manages dependencies via platformio.ini for ESP32 firmware builds.
#
# All versions are pinned to match platformio.ini lib_deps to ensure behavior
# parity between ESP32 firmware and native host shadow daemon.

include(FetchContent)

# =============================================================================
# ArduinoJson 7.4.3 (JSON parsing and serialization)
# =============================================================================
# Source: platformio.ini line 126, 332
#   bblanchon/ArduinoJson@7.4.3
#
# ArduinoJson is header-only C++14/17 library with zero dependencies.
# Used for:
#   - Configuration loading (src/config/host_config_backend.cpp)
#   - Telemetry payload generation (src/telemetry/*)
#   - Home Assistant discovery messages
#
# Upstream: https://github.com/bblanchon/ArduinoJson
# Release: https://github.com/bblanchon/ArduinoJson/releases/tag/v7.4.3
# License: MIT (see LICENSE-THIRD-PARTY.md)

FetchContent_Declare(
    ArduinoJson
    GIT_REPOSITORY https://github.com/bblanchon/ArduinoJson.git
    GIT_TAG        77771d3c07668e01d8f52acb03910c1110bb373f  # v7.4.3 commit (immutable)
    GIT_SHALLOW    TRUE    # Single commit, no history
)

FetchContent_MakeAvailable(ArduinoJson)

# ArduinoJson CMakeLists.txt exports ArduinoJson target with INTERFACE includes
# Usage in targets:
#   target_link_libraries(my_target PRIVATE ArduinoJson)

message(STATUS "ArduinoJson 7.4.3 fetched (header-only, ${arduinojson_SOURCE_DIR})")

# =============================================================================
# Unity Test Framework (optional, for native unit tests)
# =============================================================================
# Source: platformio.ini line 48
#   test_framework = unity
#
# PlatformIO bundles Unity automatically. For CMake native tests, Unity is
# fetched as FetchContent if tests are enabled.
#
# Upstream: https://github.com/ThrowTheSwitch/Unity
# License: MIT (see LICENSE-THIRD-PARTY.md)
#
# Unity version: PlatformIO ships Unity 2.6.0 (as of pio core 6.1.x).
# We match this version for test behavior parity.

option(BUILD_TESTING "Build unit tests with Unity framework" OFF)

if(BUILD_TESTING)
    FetchContent_Declare(
        Unity
        GIT_REPOSITORY https://github.com/ThrowTheSwitch/Unity.git
        GIT_TAG        v2.6.0  # Matches PlatformIO bundled version
        GIT_SHALLOW    TRUE
    )

    FetchContent_MakeAvailable(Unity)

    # Unity exports 'unity' target
    # Compile with UNITY_INCLUDE_DOUBLE for measurement model (see platformio.ini)
    target_compile_definitions(unity PUBLIC UNITY_INCLUDE_DOUBLE)

    message(STATUS "Unity 2.6.0 fetched (test framework, ${unity_SOURCE_DIR})")
endif()
