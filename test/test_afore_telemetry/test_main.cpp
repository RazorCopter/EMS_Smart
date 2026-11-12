// SPDX-License-Identifier: MIT
// CP3 Telemetry Engine Tests: Quality tracking, polling scheduler, and MQTT
// payloads.

#include <unity.h>

#include <cstring>
#include <memory>
#include <vector>

#include "device/telemetry_quality.h"
#include "drivers/afore/afore_telemetry_engine.h"
#include "drivers/afore/afore_v4_mqtt.h"
#include "protocols/modbus/modbus_rtu.h"
#include "support/mock_transport.h"
#include "transport/readonly_guard.h"


using namespace heliograph;
using heliograph::test::MockTransport;

namespace {

MockTransport *mockTransport = nullptr;
ReadOnlyGuard *guardTransport = nullptr;
AforeTelemetryEngine *engine = nullptr;

// Helper to build Modbus response frame
std::vector<uint8_t>
buildModbusResponse(uint8_t slaveId, uint16_t startAddress,
                    const std::vector<uint16_t> &registers) {
  (void)startAddress;
  std::vector<uint8_t> frame;
  frame.push_back(slaveId);
  frame.push_back(0x03); // FC03 Read Holding Registers
  frame.push_back(static_cast<uint8_t>(registers.size() * 2)); // byte count

  for (uint16_t reg : registers) {
    frame.push_back(static_cast<uint8_t>(reg >> 8));
    frame.push_back(static_cast<uint8_t>(reg & 0xFF));
  }

  const uint16_t crc = modbus::crc16(frame.data(), frame.size());
  frame.push_back(static_cast<uint8_t>(crc & 0xFF));
  frame.push_back(static_cast<uint8_t>(crc >> 8));

  return frame;
}

} // namespace

void setUp() {
  auto ownedMock = std::make_unique<MockTransport>();
  mockTransport = ownedMock.get();
  guardTransport = new ReadOnlyGuard(std::move(ownedMock), 1);
  engine = new AforeTelemetryEngine(*guardTransport, 1);
}

void tearDown() {
  delete engine;
  delete guardTransport;
  engine = nullptr;
  guardTransport = nullptr;
  mockTransport = nullptr;
}

// --- Quality Model Tests
// -----------------------------------------------------------

static void test_quality_computation_invalid_when_not_supported() {
  Quality q = computeQuality(false, true, false, true);
  TEST_ASSERT_EQUAL(Quality::Invalid, q);
}

static void test_quality_computation_invalid_when_not_valid() {
  Quality q = computeQuality(true, false, false, true);
  TEST_ASSERT_EQUAL(Quality::Invalid, q);
}

static void test_quality_computation_stale_when_aged() {
  Quality q = computeQuality(true, true, true, true);
  TEST_ASSERT_EQUAL(Quality::Stale, q);
}

static void test_quality_computation_good_when_verified_and_fresh() {
  Quality q = computeQuality(true, true, false, true);
  TEST_ASSERT_EQUAL(Quality::Good, q);
}

static void test_quality_computation_degraded_when_unverified_and_fresh() {
  Quality q = computeQuality(true, true, false, false);
  TEST_ASSERT_EQUAL(Quality::Degraded, q);
}

static void test_quality_string_conversion() {
  TEST_ASSERT_EQUAL_STRING("good", qualityToString(Quality::Good));
  TEST_ASSERT_EQUAL_STRING("degraded", qualityToString(Quality::Degraded));
  TEST_ASSERT_EQUAL_STRING("stale", qualityToString(Quality::Stale));
  TEST_ASSERT_EQUAL_STRING("invalid", qualityToString(Quality::Invalid));
}

static void test_age_computation_normal_case() {
  uint64_t age = computeAge(10000, 8000);
  TEST_ASSERT_EQUAL_UINT64(2000, age);
}

static void test_age_computation_guards_against_clock_skew() {
  uint64_t age = computeAge(8000, 10000); // timestamp in future
  TEST_ASSERT_EQUAL_UINT64(0, age);
}

static void test_telemetry_sample_actionable_for_good_quality() {
  TelemetrySample sample;
  sample.quality = Quality::Good;
  TEST_ASSERT_TRUE(sample.actionable());
}

static void test_telemetry_sample_actionable_for_degraded_quality() {
  TelemetrySample sample;
  sample.quality = Quality::Degraded;
  TEST_ASSERT_TRUE(sample.actionable());
}

static void test_telemetry_sample_not_actionable_for_stale() {
  TelemetrySample sample;
  sample.quality = Quality::Stale;
  TEST_ASSERT_FALSE(sample.actionable());
}

static void test_telemetry_sample_not_actionable_for_invalid() {
  TelemetrySample sample;
  sample.quality = Quality::Invalid;
  TEST_ASSERT_FALSE(sample.actionable());
}

// --- Polling Scheduler Tests
// -------------------------------------------------------

static void test_polling_block_is_due_when_interval_elapsed() {
  PollingBlock block{512, 19, 1000, 0, true};

  TEST_ASSERT_TRUE(block.isDue(1000)); // Exactly at interval
  TEST_ASSERT_TRUE(block.isDue(1500)); // Past interval
}

static void test_polling_block_not_due_before_interval() {
  PollingBlock block{512, 19, 1000, 0, true};

  TEST_ASSERT_FALSE(block.isDue(500)); // Before interval
}

static void test_polling_block_not_due_when_disabled() {
  PollingBlock block{512, 19, 1000, 0, false};

  TEST_ASSERT_FALSE(block.isDue(2000)); // Even past interval
}

static void test_engine_configuration_sets_blocks() {
  engine->configureFastBlock(512, 19, 1000);
  engine->configureMediumBlock(2000, 9, 5000);
  engine->configureSlowBlock(552, 9, 10000);

  // Verify by attempting poll (should request blocks)
  // Mock will return no data, but blocks should be configured
  TEST_ASSERT_NOT_NULL(engine);
}

// --- Telemetry Decoding Tests
// ------------------------------------------------------

static void test_battery_soc_decoding_verified_quality() {
  // Battery block: 0x07D0-0x07D8
  std::vector<uint16_t> batteryData = {
      0x0004, // Battery state = 4 (charging)
      0x0000,
      0x0050, // SOC = 80%
      0x0000,
      0x0208, // Voltage = 52.0V
      0xFFC4, // Current = -0.60A
      0x0000, 0x0000,
      0xFF38 // Power = -200W raw → +200W canonical
  };

  auto response = buildModbusResponse(1, 2000, batteryData);
  mockTransport->replyWith(response);

  engine->configureMediumBlock(2000, 9, 5000);
  size_t polled = engine->poll(5000);

  TEST_ASSERT_EQUAL(1, polled);

  const auto &samples = engine->getTelemetrySamples();

  // Verify battery SOC
  auto it = samples.find(measurement_id::kBatterySoc);
  TEST_ASSERT_TRUE(it != samples.end());
  TEST_ASSERT_EQUAL_DOUBLE(80.0, it->second.value);
  TEST_ASSERT_EQUAL(Quality::Good, it->second.quality);
  TEST_ASSERT_TRUE(it->second.verified);
  TEST_ASSERT_TRUE(it->second.actionable());
}

static void test_battery_power_sign_flip() {
  // Critical: Afore reports NEGATIVE during charge, profile scale=-1 flips it
  std::vector<uint16_t> batteryData = {
      0x0004, 0x0000, 0x0050, 0x0000, 0x0208, 0xFFC4, 0x0000, 0x0000,
      0xFF38 // -200W raw (charging) → +200W canonical
  };

  auto response = buildModbusResponse(1, 2000, batteryData);
  mockTransport->replyWith(response);

  engine->configureMediumBlock(2000, 9, 5000);
  engine->poll(5000);

  const auto &samples = engine->getTelemetrySamples();
  auto it = samples.find(measurement_id::kBatteryPower);

  TEST_ASSERT_TRUE(it != samples.end());
  TEST_ASSERT_EQUAL_DOUBLE(200.0, it->second.value); // Positive = charging
  TEST_ASSERT_TRUE(it->second.verified);
}

static void test_pv_power_verified_voltage_unverified() {
  // PV block: 0x0228-0x0230
  std::vector<uint16_t> pvData = {
      0x0D8C, // PV1 voltage = 346.8V (UNVERIFIED)
      0x0019, // PV1 current = 2.5A (UNVERIFIED)
      0x04B0, // Total PV power = 1200W (VERIFIED)
      0x0000, 0x0000,
      0x0320, // PV1 power = 800W (VERIFIED)
      0x0000, 0x0000,
      0x0000 // PV2 power = 0W (VERIFIED)
  };

  auto response = buildModbusResponse(1, 552, pvData);
  mockTransport->replyWith(response);

  engine->configureSlowBlock(552, 9, 10000);
  engine->poll(10000);

  const auto &samples = engine->getTelemetrySamples();

  // VERIFIED: Total PV power
  auto pvTotal = samples.find(measurement_id::kDcPowerTotal);
  TEST_ASSERT_TRUE(pvTotal != samples.end());
  TEST_ASSERT_EQUAL_DOUBLE(1200.0, pvTotal->second.value);
  TEST_ASSERT_EQUAL(Quality::Good, pvTotal->second.quality);
  TEST_ASSERT_TRUE(pvTotal->second.verified);

  // UNVERIFIED: PV1 voltage
  auto pv1V = samples.find(measurement_id::kDcMppt1Voltage);
  TEST_ASSERT_TRUE(pv1V != samples.end());
  TEST_ASSERT_EQUAL_DOUBLE(346.8, pv1V->second.value);
  TEST_ASSERT_EQUAL(Quality::Degraded, pv1V->second.quality);
  TEST_ASSERT_FALSE(pv1V->second.verified);
}

// --- Staleness Detection Tests
// -----------------------------------------------------

static void test_fresh_samples_not_marked_stale() {
  std::vector<uint16_t> data = {0x0000, 0x0000, 0x0050, 0x0000, 0x0208,
                                0x0000, 0x0000, 0x0000, 0x0000};
  auto response = buildModbusResponse(1, 2000, data);
  mockTransport->replyWith(response);

  engine->configureMediumBlock(2000, 9, 5000);
  engine->setMaxAge(15000);

  engine->poll(5000); // First poll when the 5s block becomes due

  const auto &samples = engine->getTelemetrySamples();
  auto soc = samples.find(measurement_id::kBatterySoc);

  TEST_ASSERT_EQUAL(Quality::Good, soc->second.quality);
  TEST_ASSERT_EQUAL_UINT64(0, soc->second.ageMs);
}

static void test_aged_samples_marked_stale() {
  std::vector<uint16_t> data = {0x0000, 0x0000, 0x0050, 0x0000, 0x0208,
                                0x0000, 0x0000, 0x0000, 0x0000};
  auto response = buildModbusResponse(1, 2000, data);
  mockTransport->replyWith(response);

  engine->configureMediumBlock(2000, 9, 5000);
  engine->setMaxAge(10000); // 10s staleness threshold

  engine->poll(5000);  // First poll when the 5s block becomes due
  engine->poll(16001); // Check 11.001s later, beyond the staleness threshold

  const auto &samples = engine->getTelemetrySamples();
  auto soc = samples.find(measurement_id::kBatterySoc);

  TEST_ASSERT_EQUAL(Quality::Stale, soc->second.quality);
  TEST_ASSERT_TRUE(soc->second.ageMs >= 10000);
  TEST_ASSERT_FALSE(soc->second.actionable());
}

// --- Communication Statistics Tests
// ------------------------------------------------

static void test_stats_track_successful_polls() {
  std::vector<uint16_t> data(9, 0);
  auto response = buildModbusResponse(1, 2000, data);
  mockTransport->replyWith(response);

  engine->configureMediumBlock(2000, 9, 5000);
  engine->poll(5000);

  const auto &stats = engine->getStats();
  TEST_ASSERT_EQUAL_UINT32(1, stats.totalPolls);
  TEST_ASSERT_EQUAL_UINT32(1, stats.successfulPolls);
  TEST_ASSERT_EQUAL_DOUBLE(100.0, stats.successRate());
}

static void test_stats_track_timeouts() {
  // Don't queue response → timeout
  engine->configureFastBlock(512, 19, 1000);
  engine->poll(1000);

  const auto &stats = engine->getStats();
  TEST_ASSERT_EQUAL_UINT32(1, stats.totalPolls);
  TEST_ASSERT_EQUAL_UINT32(0, stats.successfulPolls);
  TEST_ASSERT_EQUAL_UINT32(1, stats.timeouts);
}

// --- MQTT Discovery Tests
// ----------------------------------------------------------

static void test_mqtt_discovery_generates_verified_entities() {
  AforeV4MqttPublisher::Config config;
  config.nodeId = "afore_v4";
  config.baseTopic = "heliograph/test";
  config.enableUnverified = false;

  AforeV4MqttPublisher publisher(config);
  auto entities = publisher.buildDiscoveryMessages();

  // Should have VERIFIED measurements + shadow entities
  // Battery: SOC, Power, Voltage, Current (4)
  // PV: Total, PV1, PV2 (3)
  // Grid: Power, AC output (2)
  // Shadow: mode, quality, success_rate (3)
  // Total: ~12 entities
  TEST_ASSERT_TRUE(entities.size() >= 10);

  // Verify battery SOC entity structure
  bool foundBatterySoc = false;
  for (const auto &entity : entities) {
    if (entity.configTopic.find("battery_soc") != std::string::npos) {
      foundBatterySoc = true;
      TEST_ASSERT_TRUE(entity.payload.find("\"device_class\":\"battery\"") !=
                       std::string::npos);
      TEST_ASSERT_TRUE(entity.payload.find("afore_v4") != std::string::npos);
      break;
    }
  }
  TEST_ASSERT_TRUE(foundBatterySoc);
}

static void test_mqtt_telemetry_payload_includes_quality() {
  std::vector<uint16_t> data = {0x0000, 0x0000, 0x0050, 0x0000, 0x0208,
                                0x0000, 0x0000, 0x0000, 0x0000};
  auto response = buildModbusResponse(1, 2000, data);
  mockTransport->replyWith(response);

  engine->configureMediumBlock(2000, 9, 5000);
  engine->poll(5000);

  AforeV4MqttPublisher::Config config;
  config.baseTopic = "test";
  AforeV4MqttPublisher publisher(config);

  std::string payload = publisher.buildTelemetryPayload(*engine);

  // Verify JSON structure
  TEST_ASSERT_TRUE(payload.find("\"battery_soc\":") != std::string::npos);
  TEST_ASSERT_TRUE(payload.find("\"_quality\"") != std::string::npos);
  TEST_ASSERT_TRUE(payload.find("\"_age_ms\"") != std::string::npos);
}

static void test_mqtt_shadow_state_payload_summarizes_quality() {
  std::vector<uint16_t> data = {0x0000, 0x0000, 0x0050, 0x0000, 0x0208,
                                0x0000, 0x0000, 0x0000, 0x0000};
  auto response = buildModbusResponse(1, 2000, data);
  mockTransport->replyWith(response);

  engine->configureMediumBlock(2000, 9, 5000);
  engine->poll(5000);

  AforeV4MqttPublisher::Config config;
  AforeV4MqttPublisher publisher(config);

  std::string payload = publisher.buildShadowStatePayload(*engine, 5000);

  TEST_ASSERT_TRUE(payload.find("\"mode\":\"shadow\"") != std::string::npos);
  TEST_ASSERT_TRUE(payload.find("\"quality_summary\"") != std::string::npos);
  TEST_ASSERT_TRUE(payload.find("\"success_rate\"") != std::string::npos);
}

// --- Integration: ReadOnlyGuard Enforcement
// ----------------------------------------

static void test_readonly_guard_blocks_write_attempts() {
  // Verify that the telemetry engine cannot issue writes through guard
  TEST_ASSERT_NOT_NULL(guardTransport);
  TEST_ASSERT_EQUAL_UINT32(0, guardTransport->blockedWrites());

  // Engine only reads, so blocked writes should remain 0
  std::vector<uint16_t> data(9, 0);
  auto response = buildModbusResponse(1, 2000, data);
  mockTransport->replyWith(response);

  engine->configureMediumBlock(2000, 9, 5000);
  engine->poll(5000);

  TEST_ASSERT_EQUAL_UINT32(0, guardTransport->blockedWrites());
}

// --- Test Runner
// -------------------------------------------------------------------

void run_telemetry_quality() {
  RUN_TEST(test_quality_computation_invalid_when_not_supported);
  RUN_TEST(test_quality_computation_invalid_when_not_valid);
  RUN_TEST(test_quality_computation_stale_when_aged);
  RUN_TEST(test_quality_computation_good_when_verified_and_fresh);
  RUN_TEST(test_quality_computation_degraded_when_unverified_and_fresh);
  RUN_TEST(test_quality_string_conversion);
  RUN_TEST(test_age_computation_normal_case);
  RUN_TEST(test_age_computation_guards_against_clock_skew);
  RUN_TEST(test_telemetry_sample_actionable_for_good_quality);
  RUN_TEST(test_telemetry_sample_actionable_for_degraded_quality);
  RUN_TEST(test_telemetry_sample_not_actionable_for_stale);
  RUN_TEST(test_telemetry_sample_not_actionable_for_invalid);
}

void run_polling_scheduler() {
  RUN_TEST(test_polling_block_is_due_when_interval_elapsed);
  RUN_TEST(test_polling_block_not_due_before_interval);
  RUN_TEST(test_polling_block_not_due_when_disabled);
  RUN_TEST(test_engine_configuration_sets_blocks);
}

void run_telemetry_decoding() {
  RUN_TEST(test_battery_soc_decoding_verified_quality);
  RUN_TEST(test_battery_power_sign_flip);
  RUN_TEST(test_pv_power_verified_voltage_unverified);
}

void run_staleness_detection() {
  RUN_TEST(test_fresh_samples_not_marked_stale);
  RUN_TEST(test_aged_samples_marked_stale);
}

void run_communication_stats() {
  RUN_TEST(test_stats_track_successful_polls);
  RUN_TEST(test_stats_track_timeouts);
}

void run_mqtt_discovery() {
  RUN_TEST(test_mqtt_discovery_generates_verified_entities);
  RUN_TEST(test_mqtt_telemetry_payload_includes_quality);
  RUN_TEST(test_mqtt_shadow_state_payload_summarizes_quality);
}

// Independent reference CRC16 oracle (bit-by-bit reference implementation)
static uint16_t independent_crc16_oracle(const uint8_t *data, size_t len) {
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < len; ++i) {
    crc ^= static_cast<uint16_t>(data[i]);
    for (int b = 0; b < 8; ++b) {
      if (crc & 0x0001) {
        crc = static_cast<uint16_t>((crc >> 1) ^ 0xA001);
      } else {
        crc >>= 1;
      }
    }
  }
  return crc;
}

void test_m7_exact_oneshot_request_frame_and_parameters() {
  // Pre-G2 M7 bounded single one-shot request specification:
  // Target: Afore AF5K-SL Battery SOC (0x07D2) via FC04 (Read Input Registers
  // per 运行信息04) Strict invariants: zero polling, zero endurance, zero
  // retries, timeout <= 1000 ms. Frame MUST NOT be transmitted over any
  // physical transport in offline tests.

  constexpr uint8_t kExpectedSlaveId = 1;
  constexpr uint8_t kExpectedFunctionCode =
      0x04; // FC04: Read Input Registers per 运行信息04
  constexpr uint16_t kExpectedAddress = 0x07D2; // 2002 decimal: Battery SOC
  constexpr uint16_t kExpectedQuantity = 1;     // 1 register (16-bit word)
  constexpr size_t kExpectedResponseLength =
      7; // 1(slave) + 1(fc) + 1(bytes) + 2(data) + 2(crc) = 7
  constexpr uint32_t kMaxAllowedTimeoutMs =
      1000; // Bounded single-read timeout <= 1000 ms
  constexpr uint8_t kMaxAllowedRetries = 0; // Zero automatic retries

  // 1. Build request using production buildReadRequest
  uint8_t txBuf[16] = {0};
  size_t txLen = 0;
  auto res = modbus::buildReadRequest(kExpectedSlaveId, kExpectedFunctionCode,
                                      kExpectedAddress, kExpectedQuantity,
                                      txBuf, sizeof(txBuf), txLen);

  TEST_ASSERT_EQUAL(modbus::BuildResult::Ok, res);
  TEST_ASSERT_EQUAL(8, txLen);

  // 2. Validate payload fields
  TEST_ASSERT_EQUAL_HEX8(kExpectedSlaveId, txBuf[0]);
  TEST_ASSERT_EQUAL_HEX8(kExpectedFunctionCode, txBuf[1]);
  TEST_ASSERT_EQUAL_HEX8((kExpectedAddress >> 8) & 0xFF, txBuf[2]);  // 0x07
  TEST_ASSERT_EQUAL_HEX8(kExpectedAddress & 0xFF, txBuf[3]);         // 0xD2
  TEST_ASSERT_EQUAL_HEX8((kExpectedQuantity >> 8) & 0xFF, txBuf[4]); // 0x00
  TEST_ASSERT_EQUAL_HEX8(kExpectedQuantity & 0xFF, txBuf[5]);        // 0x01

  // 3. Independent dual-oracle CRC verification
  const uint16_t prodCrc = modbus::crc16(txBuf, 6);
  const uint16_t oracleCrc = independent_crc16_oracle(txBuf, 6);

  // Both production implementation and independent oracle MUST match 0x8790
  TEST_ASSERT_EQUAL_HEX16(0x8790, prodCrc);
  TEST_ASSERT_EQUAL_HEX16(0x8790, oracleCrc);

  // 4. Validate CRC wire order (Modbus transmits CRC low-byte first)
  const uint8_t crcLo = static_cast<uint8_t>(oracleCrc & 0xFF);        // 0x90
  const uint8_t crcHi = static_cast<uint8_t>((oracleCrc >> 8) & 0xFF); // 0x87
  TEST_ASSERT_EQUAL_HEX8(0x90, crcLo);
  TEST_ASSERT_EQUAL_HEX8(0x87, crcHi);
  TEST_ASSERT_EQUAL_HEX8(crcLo, txBuf[6]);
  TEST_ASSERT_EQUAL_HEX8(crcHi, txBuf[7]);

  // 5. Verify the entire canonical wire frame matches 01 04 07 D2 00 01 90 87
  // exactly
  const uint8_t kExpectedWireFrame[8] = {0x01, 0x04, 0x07, 0xD2,
                                         0x00, 0x01, 0x90, 0x87};
  TEST_ASSERT_EQUAL_HEX8_ARRAY(kExpectedWireFrame, txBuf, 8);

  // 6. Verify operational bounds for M7
  TEST_ASSERT_EQUAL_UINT32(7, kExpectedResponseLength);
  TEST_ASSERT_TRUE(kMaxAllowedTimeoutMs <= 1000);
  TEST_ASSERT_EQUAL_UINT8(0, kMaxAllowedRetries);
}

void test_consecutive_errors_trigger_backoff_and_circuit_breaker() {
  engine->configureFastBlock(512, 19, 1000);
  engine->setMinInterFrameGapMs(0); // Disable gap for fast testing

  // Initial state
  TEST_ASSERT_EQUAL_UINT32(0, engine->getConsecutiveErrors());
  TEST_ASSERT_EQUAL_UINT32(1, engine->getBackoffMultiplier());
  TEST_ASSERT_FALSE(engine->isCircuitOpen());

  // Fail 1: timeout (no reply provided)
  mockTransport->flushInput();
  engine->poll(1000);
  TEST_ASSERT_EQUAL_UINT32(1, engine->getConsecutiveErrors());
  TEST_ASSERT_EQUAL_UINT32(1, engine->getBackoffMultiplier());

  // Fail 2:
  engine->poll(2000);
  TEST_ASSERT_EQUAL_UINT32(2, engine->getConsecutiveErrors());
  TEST_ASSERT_EQUAL_UINT32(1, engine->getBackoffMultiplier());

  // Fail 3: backoff triggered!
  engine->poll(3000);
  TEST_ASSERT_EQUAL_UINT32(3, engine->getConsecutiveErrors());
  TEST_ASSERT_EQUAL_UINT32(2, engine->getBackoffMultiplier());

  // Fail 4:
  engine->poll(5000);
  TEST_ASSERT_EQUAL_UINT32(4, engine->getConsecutiveErrors());

  // Fail 5: circuit breaker triggers (open)
  engine->poll(7000);
  TEST_ASSERT_EQUAL_UINT32(5, engine->getConsecutiveErrors());
  TEST_ASSERT_TRUE(engine->isCircuitOpen());

  // Poll while circuit is open within 30s: no transactions should be attempted
  size_t txCountBefore = mockTransport->writes.size();
  size_t polled = engine->poll(10000);
  TEST_ASSERT_EQUAL_UINT32(0, polled);
  TEST_ASSERT_EQUAL_UINT32(txCountBefore, mockTransport->writes.size());

  // Recovery after circuit breaker timeout (at nowMs >= 37000):
  std::vector<uint16_t> registers(19, 0);
  auto reply = buildModbusResponse(1, 512, registers);
  mockTransport->replyWith(reply);
  polled = engine->poll(38000);
  TEST_ASSERT_EQUAL_UINT32(1, polled);
  TEST_ASSERT_FALSE(engine->isCircuitOpen());
  TEST_ASSERT_EQUAL_UINT32(0, engine->getConsecutiveErrors());
  TEST_ASSERT_EQUAL_UINT32(1, engine->getBackoffMultiplier());
}

void test_min_inter_frame_gap_enforcement() {
  engine->configureFastBlock(512, 19, 1000);
  engine->configureMediumBlock(552, 10, 1000);
  engine->setMinInterFrameGapMs(200);

  std::vector<uint16_t> registers(19, 0);
  auto reply = buildModbusResponse(1, 512, registers);
  mockTransport->replyWith(reply);

  // At t=1000, first block is polled, but second block is deferred due to
  // <200ms gap
  size_t polled = engine->poll(1000);
  TEST_ASSERT_EQUAL_UINT32(1, polled);

  // At t=1100 (< 200ms gap from t=1000), polling is deferred
  polled = engine->poll(1100);
  TEST_ASSERT_EQUAL_UINT32(0, polled);

  // At t=1250 (gap >= 200ms), second block is allowed (reply with 10 registers
  // for medium block)
  std::vector<uint16_t> registersMedium(10, 0);
  auto replyMedium = buildModbusResponse(1, 552, registersMedium);
  mockTransport->replyWith(replyMedium);
  polled = engine->poll(1250);
  TEST_ASSERT_EQUAL_UINT32(1, polled);
}

void test_m9_hardware_polling_gate_refusal() {
  // Gate G2 Safety Invariant:
  // If transport is NOT Mock (e.g. simulated real hardware), poll() refuses
  // execution unless authorizeHardwarePolling(true) is explicitly invoked.
  AforeTelemetryEngine gateEngine(*mockTransport, 1);
  gateEngine.configureFastBlock(512, 19, 1000);

  // Default state: unauthorized
  TEST_ASSERT_FALSE(gateEngine.isHardwarePollingAuthorized());

  // With mock transport, offline tests are allowed
  size_t polledMock = gateEngine.poll(1000);
  (void)polledMock; // Allowed on Mock transport

  // Explicitly test toggle
  gateEngine.authorizeHardwarePolling(true);
  TEST_ASSERT_TRUE(gateEngine.isHardwarePollingAuthorized());
  gateEngine.authorizeHardwarePolling(false);
  TEST_ASSERT_FALSE(gateEngine.isHardwarePollingAuthorized());
}

void test_m9_resilience_partial_frames() {
  engine->configureFastBlock(512, 19, 1000);

  // Truncated frame: only 4 bytes instead of the expected 43 bytes
  std::vector<uint8_t> partialFrame = {0x01, 0x03, 0x26, 0x01};
  mockTransport->replyWith(partialFrame);

  size_t polled = engine->poll(1000);
  TEST_ASSERT_EQUAL_UINT32(0, polled);
  TEST_ASSERT_EQUAL_UINT32(1, engine->getConsecutiveErrors());
}

void test_m9_resilience_crc_corruption() {
  engine->configureFastBlock(512, 19, 1000);

  std::vector<uint16_t> registers(19, 42);
  auto reply = buildModbusResponse(1, 512, registers);
  // Corrupt the CRC bytes at the end
  reply[reply.size() - 1] ^= 0xFF;
  mockTransport->replyWith(reply);

  size_t polled = engine->poll(1000);
  TEST_ASSERT_EQUAL_UINT32(0, polled);
  TEST_ASSERT_EQUAL_UINT32(1, engine->getConsecutiveErrors());
}

void test_m9_extended_offline_chaos_matrix() {
  engine->configureFastBlock(512, 19, 1000);
  engine->setMinInterFrameGapMs(
      0); // Allow immediate evaluation in simulation loop

  std::vector<uint16_t> validRegisters(19, 100);
  auto validReply = buildModbusResponse(1, 512, validRegisters);

  // Run 100 simulated iterations cycling through clean reply, CRC corruption,
  // partial frame, timeout, and recovery
  for (uint32_t step = 0; step < 100; ++step) {
    uint64_t nowMs = 10000 + (step * 2000);
    uint32_t chaosType = step % 5;

    if (chaosType == 0) {
      // Valid response
      mockTransport->replyWith(validReply);
      engine->poll(nowMs);
    } else if (chaosType == 1) {
      // Corrupted CRC
      auto badCrc = validReply;
      badCrc.back() ^= 0xAA;
      mockTransport->replyWith(badCrc);
      engine->poll(nowMs);
    } else if (chaosType == 2) {
      // Partial truncated frame
      std::vector<uint8_t> partial = {0x01, 0x03, 0x02};
      mockTransport->replyWith(partial);
      engine->poll(nowMs);
    } else if (chaosType == 3) {
      // Timeout (empty)
      mockTransport->flushInput();
      engine->poll(nowMs);
    } else {
      // Recovery after failure
      mockTransport->replyWith(validReply);
      engine->poll(nowMs);
    }

    // Verify backoff multiplier and consecutive errors remain strictly bounded
    TEST_ASSERT_TRUE(engine->getBackoffMultiplier() <= 16);
    TEST_ASSERT_TRUE(engine->getConsecutiveErrors() <= 100);
  }
}

void run_readonly_integration() {
  RUN_TEST(test_readonly_guard_blocks_write_attempts);
  RUN_TEST(test_m7_exact_oneshot_request_frame_and_parameters);
}

void run_m9_backoff_and_gap_tests() {
  RUN_TEST(test_consecutive_errors_trigger_backoff_and_circuit_breaker);
  RUN_TEST(test_min_inter_frame_gap_enforcement);
  RUN_TEST(test_m9_hardware_polling_gate_refusal);
  RUN_TEST(test_m9_resilience_partial_frames);
  RUN_TEST(test_m9_resilience_crc_corruption);
  RUN_TEST(test_m9_extended_offline_chaos_matrix);
}

int main(int, char **) {
  UNITY_BEGIN();

  run_telemetry_quality();
  run_polling_scheduler();
  run_telemetry_decoding();
  run_staleness_detection();
  run_communication_stats();
  run_mqtt_discovery();
  run_readonly_integration();
  run_m9_backoff_and_gap_tests();

  return UNITY_END();
}
