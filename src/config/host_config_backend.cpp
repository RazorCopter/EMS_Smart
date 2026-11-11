// SPDX-License-Identifier: MIT

#include "host_config_backend.h"

#if !defined(ESP32)

#include <cstdlib>
#include <fstream>
#include <sstream>

// ArduinoJson is already used by env:native and is pure C++, no Arduino
// dependency
#include <ArduinoJson.h>

#include "diagnostics/logger.h"
#include "drivers/driver_descriptor.h"
#include "drivers/modbus_profile/modbus_profile_driver.h"
#include "drivers/modbus_profile/profile_tables.h"

namespace heliograph {

bool resolveHostDriverSelection(const Configuration &config,
                                HostDriverSelection &selection,
                                ConfigError &error) {
  constexpr const char *kProductionDriverId = "modbus_profile";
  constexpr const char *kDefaultProfileId = "afore_af5k_sl";

  const std::string driverId =
      config.driver.id.empty() ? kProductionDriverId : config.driver.id;
  if (driverId != kProductionDriverId) {
    error = {"driver.force_id",
             "production daemon supports only modbus_profile"};
    return false;
  }

  DriverOptions options = config.driver.options;
  if (options["profile"].empty()) {
    options["profile"] = kDefaultProfileId;
  }
  if (options["unit_id"].empty()) {
    options["unit_id"] = "1";
  }

  const DriverDescriptor &descriptor = profile::descriptor();
  DriverOptionError optionError;
  if (!validateDriverOptions(descriptor, options, optionError)) {
    error = {"driver.options." + optionError.key, optionError.message};
    return false;
  }

  const std::string profileId = descriptor.optionOr(options, "profile");
  if (profile::findProfile(profileId.c_str()) == nullptr) {
    error = {"driver.options.profile", "profile is not compiled in"};
    return false;
  }

  long slaveId = 0;
  if (!descriptor.numericOption(options, "unit_id", slaveId)) {
    error = {"driver.options.unit_id", "must be between 1 and 247"};
    return false;
  }

  selection.driverId = driverId;
  selection.profileId = profileId;
  selection.slaveId = static_cast<uint8_t>(slaveId);
  return true;
}

bool HostConfigBackend::loadFromFile(const std::string &path,
                                     Configuration &config) {
  lastError_.clear();

  std::ifstream file(path);
  if (!file.is_open()) {
    std::ostringstream oss;
    oss << "failed to open config file: " << path;
    lastError_ = oss.str();
    return false;
  }

  // Read entire file into string
  std::ostringstream buffer;
  buffer << file.rdbuf();
  std::string content = buffer.str();

  // Parse JSON
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, content);
  if (error) {
    std::ostringstream oss;
    oss << "JSON parse error: " << error.c_str();
    lastError_ = oss.str();
    return false;
  }

  // Extract fields (using existing Configuration field names where possible)
  // For Phase 0 Shadow, we primarily need: serial port, baud rate, log level,
  // device discovery

  // Serial/transport config
  if (doc["serial_port"].is<const char *>()) {
    // Store in a way that can be retrieved later; this isn't part of the core
    // Configuration struct, so we'll need a separate holder or extend
    // Configuration. For now, log it and assume it's passed separately to the
    // transport.
    std::string port = doc["serial_port"].as<std::string>();
    log::info("host_config: serial_port=%s", port.c_str());
  }

  // Logging
  if (doc["logging"].is<JsonObject>()) {
    JsonObject logging = doc["logging"];
    if (logging["level"].is<const char *>()) {
      LogLevel level;
      if (parseLogLevel(logging["level"].as<std::string>(), level)) {
        config.logLevel = level;
      }
    }
  }

  // Polling interval
  if (doc["polling"].is<JsonObject>()) {
    JsonObject polling = doc["polling"];
    if (polling["interval_seconds"].is<unsigned int>()) {
      config.polling.intervalSeconds =
          polling["interval_seconds"].as<uint32_t>();
    }
  }

  // Driver selection. Options use the generic modbus_profile contract; vendor
  // knowledge remains in the generated profile table.
  if (doc["driver"].is<JsonObject>()) {
    JsonObject driver = doc["driver"];
    if (driver["force_id"].is<const char *>()) {
      config.driver.id = driver["force_id"].as<std::string>();
    }
    if (driver["profile"].is<const char *>()) {
      config.driver.options["profile"] = driver["profile"].as<std::string>();
    }
    if (driver["unit_id"].is<unsigned int>()) {
      config.driver.options["unit_id"] =
          std::to_string(driver["unit_id"].as<unsigned int>());
    }
  }

  // MQTT (optional for Phase 0, but supported)
  if (doc["mqtt"].is<JsonObject>()) {
    JsonObject mqtt = doc["mqtt"];
    config.mqtt.enabled = mqtt["enabled"] | false;
    if (config.mqtt.enabled) {
      config.mqtt.host = mqtt["host"].as<std::string>();
      config.mqtt.port = mqtt["port"] | 1883;
      config.mqtt.username = mqtt["username"].as<std::string>();
      config.mqtt.password = mqtt["password"].as<std::string>();
      config.mqtt.baseTopic = mqtt["base_topic"] | "heliograph";
    }
  }

  log::info("host_config: loaded from %s", path.c_str());
  return true;
}

void HostConfigBackend::loadFromEnvironment(Configuration &config) {
  // HELIOGRAPH_LOG_LEVEL
  if (const char *level = std::getenv("HELIOGRAPH_LOG_LEVEL")) {
    LogLevel parsed;
    if (parseLogLevel(level, parsed)) {
      config.logLevel = parsed;
      log::info("host_config: log level from env: %s", level);
    }
  }

  // HELIOGRAPH_SERIAL_PORT (informational; actual usage is in the daemon main)
  if (const char *port = std::getenv("HELIOGRAPH_SERIAL_PORT")) {
    log::info("host_config: serial port from env: %s", port);
  }

  // HELIOGRAPH_POLL_INTERVAL
  if (const char *interval = std::getenv("HELIOGRAPH_POLL_INTERVAL")) {
    config.polling.intervalSeconds = static_cast<uint32_t>(std::atoi(interval));
  }

  // HELIOGRAPH_DRIVER
  if (const char *driver = std::getenv("HELIOGRAPH_DRIVER")) {
    config.driver.id = driver;
  }
  if (const char *profileId = std::getenv("HELIOGRAPH_PROFILE")) {
    config.driver.options["profile"] = profileId;
  }
  if (const char *unitId = std::getenv("HELIOGRAPH_SLAVE_ID")) {
    config.driver.options["unit_id"] = unitId;
  }
}

} // namespace heliograph

#endif // !defined(ESP32)
