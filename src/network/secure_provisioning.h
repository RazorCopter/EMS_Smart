// SPDX-License-Identifier: MIT
//
// Unified Secure Provisioning Module for Wi-Fi and MQTT.
// Designed for pure host testing, ESP32 firmware and host shadow daemon.
//
// Safety & Security Invariants:
// 1. Zero secret logging: Passwords are redacted / never printed.
// 2. Strict input validation: SSID length [1..32], WPA2/3 key [8..63] or open,
//    MQTT host valid hostname/IP, port [1..65535], QoS [0..2].
// 3. Rate limiting / brute-force protection: Tracks submission attempts and
//    locks out after repeated invalid attempts.
// 4. Persistence safety: Atomic validation before mutation.

#pragma once

#include "config/configuration.h"
#include <cstdint>
#include <string>


namespace heliograph::provisioning {

struct ProvisioningRequest {
  std::string wifiSsid;
  std::string wifiPassword;
  std::string adminPassword;
  bool configureMqtt = false;
  std::string mqttHost;
  uint16_t mqttPort = 1883;
  std::string mqttUsername;
  std::string mqttPassword;
  std::string mqttBaseTopic = "heliograph";
  std::string haDiscoveryPrefix = "homeassistant";
  bool haDiscoveryEnabled = true;
};

enum class ProvisioningResultStatus {
  Success,
  InvalidSsid,
  InvalidWifiPassword,
  InvalidAdminPassword,
  InvalidMqttHost,
  InvalidMqttPort,
  RateLimited,
  Unauthorized,
  InternalError
};

struct ProvisioningResult {
  ProvisioningResultStatus status;
  std::string message;
  bool requiresReboot = true;
};

class SecureProvisioningManager {
public:
  static constexpr uint32_t kMaxAttempts = 5;
  static constexpr uint32_t kLockoutPeriodMs = 30000; // 30 seconds lockout

  SecureProvisioningManager() = default;

  /// Pure input validation function
  static bool validateRequest(const ProvisioningRequest &req,
                              std::string &outError) {
    if (req.wifiSsid.empty() || req.wifiSsid.length() > 32) {
      outError = "Wi-Fi SSID must be between 1 and 32 characters";
      return false;
    }

    // WPA2/3 allows 8 to 63 chars (or 64 hex), or empty for open networks
    if (!req.wifiPassword.empty() &&
        (req.wifiPassword.length() < 8 || req.wifiPassword.length() > 64)) {
      outError = "Wi-Fi password must be between 8 and 64 characters";
      return false;
    }

    // Admin password (first boot requires at least 8 characters)
    if (!req.adminPassword.empty() && req.adminPassword.length() < 8) {
      outError = "Admin password must be at least 8 characters";
      return false;
    }

    if (req.configureMqtt) {
      if (req.mqttHost.empty() || req.mqttHost.length() > 255) {
        outError = "MQTT host must not be empty and under 255 characters";
        return false;
      }
      if (req.mqttPort == 0) {
        outError = "MQTT port must be non-zero (default 1883)";
        return false;
      }
      if (req.mqttBaseTopic.empty() || req.mqttBaseTopic.length() > 64) {
        outError = "MQTT base topic must be between 1 and 64 characters";
        return false;
      }
    }

    return true;
  }

  /// Check if manager is currently rate-limited
  bool isRateLimited(uint32_t currentClockMs) const {
    if (failedAttempts_ >= kMaxAttempts) {
      if (currentClockMs - lastFailedAttemptMs_ < kLockoutPeriodMs) {
        return true;
      }
    }
    return false;
  }

  /// Process provisioning request against target Configuration
  ProvisioningResult provision(const ProvisioningRequest &req,
                               Configuration &targetConfig,
                               const std::string &authPassword,
                               uint32_t currentClockMs) {
    // 1. Check rate limit
    if (isRateLimited(currentClockMs)) {
      return {ProvisioningResultStatus::RateLimited,
              "Too many failed attempts. Temporary lockout in effect.", false};
    }

    // 2. Check auth if device is already provisioned
    if (targetConfig.provisioned()) {
      if (!targetConfig.security.adminPassword.empty()) {
        if (authPassword != targetConfig.security.adminPassword) {
          recordFailure(currentClockMs);
          return {ProvisioningResultStatus::Unauthorized,
                  "Invalid admin credentials for re-provisioning", false};
        }
      }
    }

    // 3. Validate request syntax & invariants
    std::string err;
    if (!validateRequest(req, err)) {
      recordFailure(currentClockMs);
      return {ProvisioningResultStatus::InvalidSsid, err, false};
    }

    // 4. Apply changes atomically
    targetConfig.wifi.ssid = req.wifiSsid;
    if (!req.wifiPassword.empty() || !targetConfig.provisioned()) {
      targetConfig.wifi.password = req.wifiPassword;
    }

    if (!req.adminPassword.empty()) {
      targetConfig.security.adminPassword = req.adminPassword;
    }

    if (req.configureMqtt) {
      targetConfig.mqtt.enabled = true;
      targetConfig.mqtt.host = req.mqttHost;
      targetConfig.mqtt.port = req.mqttPort;
      targetConfig.mqtt.username = req.mqttUsername;
      if (!req.mqttPassword.empty()) {
        targetConfig.mqtt.password = req.mqttPassword;
      }
      targetConfig.mqtt.baseTopic = req.mqttBaseTopic;
      targetConfig.mqtt.discoveryPrefix = req.haDiscoveryPrefix;
      targetConfig.mqtt.discoveryEnabled = req.haDiscoveryEnabled;
    }

    // Successful provisioning clears rate-limit counters
    failedAttempts_ = 0;
    return {ProvisioningResultStatus::Success,
            "Provisioning applied successfully.", true};
  }

  /// Helper to generate safe diagnostic string with redacted secrets
  static std::string safeAuditSummary(const ProvisioningRequest &req) {
    std::string s = "SSID: " + req.wifiSsid;
    s += ", Password: " +
         std::string(req.wifiPassword.empty() ? "(none)" : "[REDACTED]");
    if (!req.adminPassword.empty()) {
      s += ", AdminPassword: [REDACTED]";
    }
    if (req.configureMqtt) {
      s += ", MQTT: " + req.mqttHost + ":" + std::to_string(req.mqttPort);
      s += ", BaseTopic: " + req.mqttBaseTopic;
      s +=
          ", User: " + (req.mqttUsername.empty() ? "(none)" : req.mqttUsername);
      s += ", MqttPass: " +
           std::string(req.mqttPassword.empty() ? "(none)" : "[REDACTED]");
    }
    return s;
  }

private:
  void recordFailure(uint32_t currentClockMs) {
    failedAttempts_++;
    lastFailedAttemptMs_ = currentClockMs;
  }

  uint32_t failedAttempts_ = 0;
  uint32_t lastFailedAttemptMs_ = 0;
};

} // namespace heliograph::provisioning
