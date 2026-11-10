// SPDX-License-Identifier: MIT

#include "host_config_backend.h"

#if !defined(ESP32)

#include <cstdlib>
#include <fstream>
#include <sstream>

// ArduinoJson is already used by env:native and is pure C++, no Arduino dependency
#include <ArduinoJson.h>

#include "diagnostics/logger.h"

namespace heliograph {

bool HostConfigBackend::loadFromFile(const std::string& path, Configuration& config) {
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
    // For Phase 0 Shadow, we primarily need: serial port, baud rate, log level, device discovery

    // Serial/transport config
    if (doc.containsKey("serial_port")) {
        // Store in a way that can be retrieved later; this isn't part of the core Configuration
        // struct, so we'll need a separate holder or extend Configuration.
        // For now, log it and assume it's passed separately to the transport.
        std::string port = doc["serial_port"].as<std::string>();
        log::info("host_config: serial_port=%s", port.c_str());
    }

    // Logging
    if (doc.containsKey("logging")) {
        JsonObject logging = doc["logging"];
        if (logging.containsKey("level")) {
            LogLevel level;
            if (parseLogLevel(logging["level"].as<std::string>(), level)) {
                config.logging.level = level;
            }
        }
    }

    // Polling interval
    if (doc.containsKey("polling")) {
        JsonObject polling = doc["polling"];
        if (polling.containsKey("interval_seconds")) {
            config.polling.intervalSeconds = polling["interval_seconds"].as<uint32_t>();
        }
    }

    // Driver selection
    if (doc.containsKey("driver")) {
        JsonObject driver = doc["driver"];
        if (driver.containsKey("force_id")) {
            config.driver.forceId = driver["force_id"].as<std::string>();
        }
    }

    // MQTT (optional for Phase 0, but supported)
    if (doc.containsKey("mqtt")) {
        JsonObject mqtt = doc["mqtt"];
        config.mqtt.enabled = mqtt["enabled"] | false;
        if (config.mqtt.enabled) {
            config.mqtt.host       = mqtt["host"].as<std::string>();
            config.mqtt.port       = mqtt["port"] | 1883;
            config.mqtt.username   = mqtt["username"].as<std::string>();
            config.mqtt.password   = mqtt["password"].as<std::string>();
            config.mqtt.baseTopic  = mqtt["base_topic"] | "heliograph";
        }
    }

    log::info("host_config: loaded from %s", path.c_str());
    return true;
}

void HostConfigBackend::loadFromEnvironment(Configuration& config) {
    // HELIOGRAPH_LOG_LEVEL
    if (const char* level = std::getenv("HELIOGRAPH_LOG_LEVEL")) {
        LogLevel parsed;
        if (parseLogLevel(level, parsed)) {
            config.logging.level = parsed;
            log::info("host_config: log level from env: %s", level);
        }
    }

    // HELIOGRAPH_SERIAL_PORT (informational; actual usage is in the daemon main)
    if (const char* port = std::getenv("HELIOGRAPH_SERIAL_PORT")) {
        log::info("host_config: serial port from env: %s", port);
    }

    // HELIOGRAPH_POLL_INTERVAL
    if (const char* interval = std::getenv("HELIOGRAPH_POLL_INTERVAL")) {
        config.polling.intervalSeconds = static_cast<uint32_t>(std::atoi(interval));
    }

    // HELIOGRAPH_DRIVER
    if (const char* driver = std::getenv("HELIOGRAPH_DRIVER")) {
        config.driver.forceId = driver;
    }
}

}  // namespace heliograph

#endif  // !defined(ESP32)
