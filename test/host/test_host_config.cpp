// SPDX-License-Identifier: MIT
//
// Unit tests for host configuration backend.

#include "config/host_config_backend.h"

#if !defined(ESP32)

#include <cstdio>
#include <cstdlib>
#include <fstream>

using namespace heliograph;

static void test_load_from_nonexistent_file_fails() {
    HostConfigBackend backend;
    Configuration config;

    bool result = backend.loadFromFile("/tmp/nonexistent_config_12345.json", config);
    if (result) {
        printf("FAIL: loadFromFile succeeded on nonexistent file\n");
        exit(1);
    }
    if (backend.lastError().empty()) {
        printf("FAIL: lastError() is empty after failure\n");
        exit(1);
    }

    printf("OK\n");
}

static void test_load_from_valid_json() {
    // Create a temporary config file
    const char* path = "/tmp/heliograph_test_config.json";
    std::ofstream out(path);
    out << R"({
  "logging": { "level": "debug" },
  "polling": { "interval_seconds": 5 },
  "driver": { "force_id": "mock" }
})";
    out.close();

    HostConfigBackend backend;
    Configuration config;

    bool result = backend.loadFromFile(path, config);
    if (!result) {
        printf("FAIL: loadFromFile failed: %s\n", backend.lastError().c_str());
        exit(1);
    }

    if (config.logging.level != LogLevel::Debug) {
        printf("FAIL: log level not parsed correctly\n");
        exit(1);
    }
    if (config.polling.intervalSeconds != 5) {
        printf("FAIL: poll interval not parsed correctly\n");
        exit(1);
    }
    if (config.driver.forceId != "mock") {
        printf("FAIL: driver force_id not parsed correctly\n");
        exit(1);
    }

    std::remove(path);
    printf("OK\n");
}

static void test_load_from_environment() {
    setenv("HELIOGRAPH_LOG_LEVEL", "trace", 1);
    setenv("HELIOGRAPH_POLL_INTERVAL", "15", 1);
    setenv("HELIOGRAPH_DRIVER", "sunspec", 1);

    HostConfigBackend backend;
    Configuration config;

    backend.loadFromEnvironment(config);

    if (config.logging.level != LogLevel::Trace) {
        printf("FAIL: log level not loaded from env\n");
        exit(1);
    }
    if (config.polling.intervalSeconds != 15) {
        printf("FAIL: poll interval not loaded from env\n");
        exit(1);
    }
    if (config.driver.forceId != "sunspec") {
        printf("FAIL: driver not loaded from env\n");
        exit(1);
    }

    unsetenv("HELIOGRAPH_LOG_LEVEL");
    unsetenv("HELIOGRAPH_POLL_INTERVAL");
    unsetenv("HELIOGRAPH_DRIVER");

    printf("OK\n");
}

int main() {
    printf("==> test_load_from_nonexistent_file_fails: ");
    test_load_from_nonexistent_file_fails();

    printf("==> test_load_from_valid_json: ");
    test_load_from_valid_json();

    printf("==> test_load_from_environment: ");
    test_load_from_environment();

    printf("\nAll HostConfigBackend tests passed.\n");
    return 0;
}

#else

int main() {
    printf("HostConfigBackend tests skipped (ESP32 build)\n");
    return 0;
}

#endif  // !defined(ESP32)
