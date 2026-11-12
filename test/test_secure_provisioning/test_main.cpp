// SPDX-License-Identifier: MIT
// Unit tests for Unified Secure Provisioning Module

#include "network/secure_provisioning.h"
#include <unity.h>


using namespace heliograph::provisioning;

void setUp() {}
void tearDown() {}

void test_validation_rules() {
  ProvisioningRequest req;
  std::string err;

  // Empty SSID
  req.wifiSsid = "";
  TEST_ASSERT_FALSE(SecureProvisioningManager::validateRequest(req, err));
  TEST_ASSERT_NOT_EQUAL(0, err.length());

  // Valid open network
  req.wifiSsid = "GuestWiFi";
  req.wifiPassword = "";
  TEST_ASSERT_TRUE(SecureProvisioningManager::validateRequest(req, err));

  // Invalid password (< 8 chars)
  req.wifiPassword = "short";
  TEST_ASSERT_FALSE(SecureProvisioningManager::validateRequest(req, err));

  // Valid password (>= 8 chars)
  req.wifiPassword = "ValidPassword123";
  TEST_ASSERT_TRUE(SecureProvisioningManager::validateRequest(req, err));

  // Short admin password (< 8 chars)
  req.adminPassword = "admin";
  TEST_ASSERT_FALSE(SecureProvisioningManager::validateRequest(req, err));

  // Valid admin password
  req.adminPassword = "AdminPassword456";
  TEST_ASSERT_TRUE(SecureProvisioningManager::validateRequest(req, err));

  // MQTT configuration validation
  req.configureMqtt = true;
  req.mqttHost = "";
  TEST_ASSERT_FALSE(SecureProvisioningManager::validateRequest(req, err));

  req.mqttHost = "192.168.1.100";
  req.mqttPort = 0;
  TEST_ASSERT_FALSE(SecureProvisioningManager::validateRequest(req, err));

  req.mqttPort = 1883;
  req.mqttBaseTopic = "";
  TEST_ASSERT_FALSE(SecureProvisioningManager::validateRequest(req, err));

  req.mqttBaseTopic = "solar/afore";
  TEST_ASSERT_TRUE(SecureProvisioningManager::validateRequest(req, err));
}

void test_first_boot_provisioning() {
  SecureProvisioningManager mgr;
  heliograph::Configuration config;
  TEST_ASSERT_FALSE(config.provisioned());

  ProvisioningRequest req;
  req.wifiSsid = "HomeNetwork";
  req.wifiPassword = "SecretWifiPassword";
  req.adminPassword = "SuperAdminPassword";
  req.configureMqtt = true;
  req.mqttHost = "broker.local";
  req.mqttPort = 1883;
  req.mqttUsername = "homeassistant";
  req.mqttPassword = "MqttSecretPassword";
  req.mqttBaseTopic = "ems/afore";

  auto res = mgr.provision(req, config, "", 1000);
  TEST_ASSERT_EQUAL(ProvisioningResultStatus::Success, res.status);
  TEST_ASSERT_TRUE(res.requiresReboot);

  TEST_ASSERT_EQUAL_STRING("HomeNetwork", config.wifi.ssid.c_str());
  TEST_ASSERT_EQUAL_STRING("SecretWifiPassword", config.wifi.password.c_str());
  TEST_ASSERT_EQUAL_STRING("SuperAdminPassword",
                           config.security.adminPassword.c_str());
  TEST_ASSERT_TRUE(config.mqtt.enabled);
  TEST_ASSERT_EQUAL_STRING("broker.local", config.mqtt.host.c_str());
  TEST_ASSERT_EQUAL_UINT16(1883, config.mqtt.port);
  TEST_ASSERT_EQUAL_STRING("ems/afore", config.mqtt.baseTopic.c_str());
  TEST_ASSERT_TRUE(config.provisioned());
}

void test_reprovisioning_auth_gate() {
  SecureProvisioningManager mgr;
  heliograph::Configuration config;
  config.wifi.ssid = "InitialNet";
  config.security.adminPassword = "ExistingPassword123";
  TEST_ASSERT_TRUE(config.provisioned());

  ProvisioningRequest req;
  req.wifiSsid = "NewNet";
  req.wifiPassword = "NewPassword123";

  // Wrong password
  auto res = mgr.provision(req, config, "WrongPassword", 1000);
  TEST_ASSERT_EQUAL(ProvisioningResultStatus::Unauthorized, res.status);
  TEST_ASSERT_EQUAL_STRING("InitialNet", config.wifi.ssid.c_str());

  // Correct password
  res = mgr.provision(req, config, "ExistingPassword123", 2000);
  TEST_ASSERT_EQUAL(ProvisioningResultStatus::Success, res.status);
  TEST_ASSERT_EQUAL_STRING("NewNet", config.wifi.ssid.c_str());
}

void test_rate_limiting_lockout() {
  SecureProvisioningManager mgr;
  heliograph::Configuration config;
  config.wifi.ssid = "TargetNet";
  config.security.adminPassword = "SecretAdmin123";

  ProvisioningRequest req;
  req.wifiSsid = "ExploitNet";

  // 5 consecutive failures
  uint32_t t = 10000;
  for (int i = 0; i < 5; ++i) {
    auto res = mgr.provision(req, config, "BadPass", t);
    TEST_ASSERT_EQUAL(ProvisioningResultStatus::Unauthorized, res.status);
    t += 100;
  }

  // 6th attempt is rate-limited even if password is correct
  auto res = mgr.provision(req, config, "SecretAdmin123", t);
  TEST_ASSERT_EQUAL(ProvisioningResultStatus::RateLimited, res.status);

  // After lockout expiration (30 seconds), valid attempt succeeds
  t += SecureProvisioningManager::kLockoutPeriodMs + 100;
  res = mgr.provision(req, config, "SecretAdmin123", t);
  TEST_ASSERT_EQUAL(ProvisioningResultStatus::Success, res.status);
  TEST_ASSERT_EQUAL_STRING("ExploitNet", config.wifi.ssid.c_str());
}

void test_safe_audit_summary_redaction() {
  ProvisioningRequest req;
  req.wifiSsid = "MySSID";
  req.wifiPassword = "PlainPassword123";
  req.adminPassword = "AdminPassword789";
  req.configureMqtt = true;
  req.mqttHost = "10.0.0.1";
  req.mqttPassword = "MqttSecretPassword";

  std::string summary = SecureProvisioningManager::safeAuditSummary(req);
  TEST_ASSERT_NOT_EQUAL(std::string::npos, summary.find("MySSID"));
  TEST_ASSERT_NOT_EQUAL(std::string::npos, summary.find("[REDACTED]"));
  TEST_ASSERT_EQUAL(std::string::npos, summary.find("PlainPassword123"));
  TEST_ASSERT_EQUAL(std::string::npos, summary.find("AdminPassword789"));
  TEST_ASSERT_EQUAL(std::string::npos, summary.find("MqttSecretPassword"));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_validation_rules);
  RUN_TEST(test_first_boot_provisioning);
  RUN_TEST(test_reprovisioning_auth_gate);
  RUN_TEST(test_rate_limiting_lockout);
  RUN_TEST(test_safe_audit_summary_redaction);
  return UNITY_END();
}
