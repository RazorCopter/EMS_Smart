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

  bool result =
      backend.loadFromFile("/tmp/nonexistent_config_12345.json", config);
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
  const char *path = "/tmp/heliograph_test_config.json";
  std::ofstream out(path);
  out << R"({
  "logging": { "level": "debug" },
  "polling": { "interval_seconds": 5 },
  "driver": {
    "force_id": "modbus_profile",
    "profile": "afore_af5k_sl",
    "unit_id": 7
  }
})";
  out.close();

  HostConfigBackend backend;
  Configuration config;

  bool result = backend.loadFromFile(path, config);
  if (!result) {
    printf("FAIL: loadFromFile failed: %s\n", backend.lastError().c_str());
    exit(1);
  }

  if (config.logLevel != LogLevel::Debug) {
    printf("FAIL: log level not parsed correctly\n");
    exit(1);
  }
  if (config.polling.intervalSeconds != 5) {
    printf("FAIL: poll interval not parsed correctly\n");
    exit(1);
  }
  if (config.driver.id != "modbus_profile" ||
      config.driver.options["profile"] != "afore_af5k_sl" ||
      config.driver.options["unit_id"] != "7") {
    printf("FAIL: driver selection not parsed correctly\n");
    exit(1);
  }

  std::remove(path);
  printf("OK\n");
}

static void test_load_from_environment() {
  setenv("HELIOGRAPH_LOG_LEVEL", "trace", 1);
  setenv("HELIOGRAPH_POLL_INTERVAL", "15", 1);
  setenv("HELIOGRAPH_DRIVER", "modbus_profile", 1);
  setenv("HELIOGRAPH_PROFILE", "afore_af5k_sl", 1);
  setenv("HELIOGRAPH_SLAVE_ID", "9", 1);

  HostConfigBackend backend;
  Configuration config;

  backend.loadFromEnvironment(config);

  if (config.logLevel != LogLevel::Trace) {
    printf("FAIL: log level not loaded from env\n");
    exit(1);
  }
  if (config.polling.intervalSeconds != 15) {
    printf("FAIL: poll interval not loaded from env\n");
    exit(1);
  }
  if (config.driver.id != "modbus_profile" ||
      config.driver.options["profile"] != "afore_af5k_sl" ||
      config.driver.options["unit_id"] != "9") {
    printf("FAIL: driver selection not loaded from env\n");
    exit(1);
  }

  unsetenv("HELIOGRAPH_LOG_LEVEL");
  unsetenv("HELIOGRAPH_POLL_INTERVAL");
  unsetenv("HELIOGRAPH_DRIVER");
  unsetenv("HELIOGRAPH_PROFILE");
  unsetenv("HELIOGRAPH_SLAVE_ID");

  printf("OK\n");
}

static void require_selection(const Configuration &config, bool expected,
                              uint8_t expectedSlave = 1) {
  HostDriverSelection selection;
  ConfigError error;
  const bool result = resolveHostDriverSelection(config, selection, error);
  if (result != expected) {
    printf("FAIL: selection result=%d, expected=%d (%s: %s)\n", result,
           expected, error.field.c_str(), error.message.c_str());
    exit(1);
  }
  if (result && (selection.driverId != "modbus_profile" ||
                 selection.profileId != "afore_af5k_sl" ||
                 selection.slaveId != expectedSlave)) {
    printf("FAIL: unexpected resolved selection\n");
    exit(1);
  }
}

static void test_production_driver_selection() {
  Configuration config;
  require_selection(config, true, 1);

  HostDriverSelection first;
  HostDriverSelection second;
  ConfigError error;
  if (!resolveHostDriverSelection(config, first, error) ||
      !resolveHostDriverSelection(config, second, error) ||
      first.profileId != second.profileId || first.slaveId != second.slaveId) {
    printf("FAIL: selection is not deterministic\n");
    exit(1);
  }

  config.driver.options["unit_id"] = "247";
  require_selection(config, true, 247);
  config.driver.options["unit_id"] = "0";
  require_selection(config, false);
  config.driver.options["unit_id"] = "248";
  require_selection(config, false);

  config = Configuration{};
  config.driver.options["profile"] = "missing_profile";
  require_selection(config, false);

  config = Configuration{};
  config.driver.id = "mock";
  require_selection(config, false);

  printf("OK\n");
}

int main() {
  printf("==> test_load_from_nonexistent_file_fails: ");
  test_load_from_nonexistent_file_fails();

  printf("==> test_load_from_valid_json: ");
  test_load_from_valid_json();

  printf("==> test_load_from_environment: ");
  test_load_from_environment();

  printf("==> test_production_driver_selection: ");
  test_production_driver_selection();

  printf("\nAll HostConfigBackend tests passed.\n");
  return 0;
}

#else

int main() {
  printf("HostConfigBackend tests skipped (ESP32 build)\n");
  return 0;
}

#endif // !defined(ESP32)
