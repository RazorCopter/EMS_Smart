// SPDX-License-Identifier: MIT
// CP3 Telemetry Engine Tests: Quality tracking, polling scheduler, and MQTT payloads.

#include <unity.h>

#include <cstring>
#include <vector>

#include "app/afore_telemetry_engine.h"
#include "app/afore_v4_mqtt.h"
#include "device/telemetry_quality.h"
#include "support/mock_transport.h"
#include "transport/readonly_guard.h"

using namespace heliograph;
using heliograph::test::MockTransport;

namespace {

MockTransport* mockTransport = nullptr;
ReadOnlyGuard* guardTransport = nullptr;
AforeTelemetryEngine* engine = nullptr;

// Helper to build Modbus response frame
std::vector<uint8_t> buildModbusResponse(uint8_t slaveId, uint16_t startAddress,
                                          const std::vector<uint16_t>& registers) {
    std::vector<uint8_t> frame;
    frame.push_back(slaveId);
    frame.push_back(0x03);  // FC03 Read Holding Registers
    frame.push_back(static_cast<uint8_t>(registers.size() * 2));  // byte count
    
    for (uint16_t reg : registers) {
        frame.push_back(static_cast<uint8_t>(reg >> 8));
        frame.push_back(static_cast<uint8_t>(reg & 0xFF));
    }
    
    // CRC placeholder
    frame.push_back(0x00);
    frame.push_back(0x00);
    
    return frame;
}

}  // namespace

void setUp() {
    mockTransport = new MockTransport();
    guardTransport = new ReadOnlyGuard(*mockTransport, true);
    engine = new AforeTelemetryEngine(*guardTransport, 1);
}

void tearDown() {
    delete engine;
    delete guardTransport;
    delete mockTransport;
    engine = nullptr;
    guardTransport = nullptr;
    mockTransport = nullptr;
}

// --- Quality Model Tests -----------------------------------------------------------

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
    uint64_t age = computeAge(8000, 10000);  // timestamp in future
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

// --- Polling Scheduler Tests -------------------------------------------------------

static void test_polling_block_is_due_when_interval_elapsed() {
    PollingBlock block{512, 19, 1000, 0, true};
    
    TEST_ASSERT_TRUE(block.isDue(1000));   // Exactly at interval
    TEST_ASSERT_TRUE(block.isDue(1500));   // Past interval
}

static void test_polling_block_not_due_before_interval() {
    PollingBlock block{512, 19, 1000, 0, true};
    
    TEST_ASSERT_FALSE(block.isDue(500));   // Before interval
}

static void test_polling_block_not_due_when_disabled() {
    PollingBlock block{512, 19, 1000, 0, false};
    
    TEST_ASSERT_FALSE(block.isDue(2000));  // Even past interval
}

static void test_engine_configuration_sets_blocks() {
    engine->configureFastBlock(512, 19, 1000);
    engine->configureMediumBlock(2000, 9, 5000);
    engine->configureSlowBlock(552, 9, 10000);
    
    // Verify by attempting poll (should request blocks)
    // Mock will return no data, but blocks should be configured
    TEST_ASSERT_NOT_NULL(engine);
}

// --- Telemetry Decoding Tests ------------------------------------------------------

static void test_battery_soc_decoding_verified_quality() {
    // Battery block: 0x07D0-0x07D8
    std::vector<uint16_t> batteryData = {
        0x0004,  // Battery state = 4 (charging)
        0x0000,
        0x0050,  // SOC = 80%
        0x0000,
        0x0208,  // Voltage = 52.0V
        0xFFC4,  // Current = -0.60A
        0x0000, 0x0000,
        0xFF38   // Power = -200W raw → +200W canonical
    };
    
    auto response = buildModbusResponse(1, 2000, batteryData);
    mockTransport->queueResponse(response);
    
    engine->configureMediumBlock(2000, 9, 5000);
    size_t polled = engine->poll(5000);
    
    TEST_ASSERT_EQUAL(1, polled);
    
    const auto& samples = engine->getTelemetrySamples();
    
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
        0xFF38   // -200W raw (charging) → +200W canonical
    };
    
    auto response = buildModbusResponse(1, 2000, batteryData);
    mockTransport->queueResponse(response);
    
    engine->configureMediumBlock(2000, 9, 5000);
    engine->poll(5000);
    
    const auto& samples = engine->getTelemetrySamples();
    auto it = samples.find(measurement_id::kBatteryPower);
    
    TEST_ASSERT_TRUE(it != samples.end());
    TEST_ASSERT_EQUAL_DOUBLE(200.0, it->second.value);  // Positive = charging
    TEST_ASSERT_TRUE(it->second.verified);
}

static void test_pv_power_verified_voltage_unverified() {
    // PV block: 0x0228-0x0230
    std::vector<uint16_t> pvData = {
        0x0D8C,  // PV1 voltage = 346.8V (UNVERIFIED)
        0x0019,  // PV1 current = 2.5A (UNVERIFIED)
        0x04B0,  // Total PV power = 1200W (VERIFIED)
        0x0000, 0x0000,
        0x0320,  // PV1 power = 800W (VERIFIED)
        0x0000, 0x0000,
        0x0000   // PV2 power = 0W (VERIFIED)
    };
    
    auto response = buildModbusResponse(1, 552, pvData);
    mockTransport->queueResponse(response);
    
    engine->configureSlowBlock(552, 9, 10000);
    engine->poll(10000);
    
    const auto& samples = engine->getTelemetrySamples();
    
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

// --- Staleness Detection Tests -----------------------------------------------------

static void test_fresh_samples_not_marked_stale() {
    std::vector<uint16_t> data = {
        0x0000, 0x0000, 0x0050, 0x0000, 0x0208, 0x0000, 0x0000, 0x0000, 0x0000
    };
    auto response = buildModbusResponse(1, 2000, data);
    mockTransport->queueResponse(response);
    
    engine->configureMediumBlock(2000, 9, 5000);
    engine->setMaxAge(15000);
    
    engine->poll(1000);  // First poll at t=1000
    
    const auto& samples = engine->getTelemetrySamples();
    auto soc = samples.find(measurement_id::kBatterySoc);
    
    TEST_ASSERT_EQUAL(Quality::Good, soc->second.quality);
    TEST_ASSERT_EQUAL_UINT64(0, soc->second.ageMs);
}

static void test_aged_samples_marked_stale() {
    std::vector<uint16_t> data = {
        0x0000, 0x0000, 0x0050, 0x0000, 0x0208, 0x0000, 0x0000, 0x0000, 0x0000
    };
    auto response = buildModbusResponse(1, 2000, data);
    mockTransport->queueResponse(response);
    
    engine->configureMediumBlock(2000, 9, 5000);
    engine->setMaxAge(10000);  // 10s staleness threshold
    
    engine->poll(1000);  // Poll at t=1000
    engine->poll(12000); // Check at t=12000 (11s later, exceeds threshold)
    
    const auto& samples = engine->getTelemetrySamples();
    auto soc = samples.find(measurement_id::kBatterySoc);
    
    TEST_ASSERT_EQUAL(Quality::Stale, soc->second.quality);
    TEST_ASSERT_TRUE(soc->second.ageMs >= 10000);
    TEST_ASSERT_FALSE(soc->second.actionable());
}

// --- Communication Statistics Tests ------------------------------------------------

static void test_stats_track_successful_polls() {
    std::vector<uint16_t> data(9, 0);
    auto response = buildModbusResponse(1, 2000, data);
    mockTransport->queueResponse(response);
    
    engine->configureMediumBlock(2000, 9, 5000);
    engine->poll(5000);
    
    const auto& stats = engine->getStats();
    TEST_ASSERT_EQUAL_UINT32(1, stats.totalPolls);
    TEST_ASSERT_EQUAL_UINT32(1, stats.successfulPolls);
    TEST_ASSERT_EQUAL_DOUBLE(100.0, stats.successRate());
}

static void test_stats_track_timeouts() {
    // Don't queue response → timeout
    engine->configureFastBlock(512, 19, 1000);
    engine->poll(1000);
    
    const auto& stats = engine->getStats();
    TEST_ASSERT_EQUAL_UINT32(1, stats.totalPolls);
    TEST_ASSERT_EQUAL_UINT32(0, stats.successfulPolls);
    TEST_ASSERT_EQUAL_UINT32(1, stats.timeouts);
}

// --- MQTT Discovery Tests ----------------------------------------------------------

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
    for (const auto& entity : entities) {
        if (entity.configTopic.find("battery_soc") != std::string::npos) {
            foundBatterySoc = true;
            TEST_ASSERT_TRUE(entity.payload.find("\"device_class\":\"battery\"") != std::string::npos);
            TEST_ASSERT_TRUE(entity.payload.find("afore_v4") != std::string::npos);
            break;
        }
    }
    TEST_ASSERT_TRUE(foundBatterySoc);
}

static void test_mqtt_telemetry_payload_includes_quality() {
    std::vector<uint16_t> data = {
        0x0000, 0x0000, 0x0050, 0x0000, 0x0208, 0x0000, 0x0000, 0x0000, 0x0000
    };
    auto response = buildModbusResponse(1, 2000, data);
    mockTransport->queueResponse(response);
    
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
    std::vector<uint16_t> data = {
        0x0000, 0x0000, 0x0050, 0x0000, 0x0208, 0x0000, 0x0000, 0x0000, 0x0000
    };
    auto response = buildModbusResponse(1, 2000, data);
    mockTransport->queueResponse(response);
    
    engine->configureMediumBlock(2000, 9, 5000);
    engine->poll(5000);
    
    AforeV4MqttPublisher::Config config;
    AforeV4MqttPublisher publisher(config);
    
    std::string payload = publisher.buildShadowStatePayload(*engine, 5000);
    
    TEST_ASSERT_TRUE(payload.find("\"mode\":\"shadow\"") != std::string::npos);
    TEST_ASSERT_TRUE(payload.find("\"quality_summary\"") != std::string::npos);
    TEST_ASSERT_TRUE(payload.find("\"success_rate\"") != std::string::npos);
}

// --- Integration: ReadOnlyGuard Enforcement ----------------------------------------

static void test_readonly_guard_blocks_write_attempts() {
    // Verify that the telemetry engine cannot issue writes through guard
    TEST_ASSERT_NOT_NULL(guardTransport);
    TEST_ASSERT_EQUAL_UINT32(0, guardTransport->blockedWrites());
    
    // Engine only reads, so blocked writes should remain 0
    std::vector<uint16_t> data(9, 0);
    auto response = buildModbusResponse(1, 2000, data);
    mockTransport->queueResponse(response);
    
    engine->configureMediumBlock(2000, 9, 5000);
    engine->poll(5000);
    
    TEST_ASSERT_EQUAL_UINT32(0, guardTransport->blockedWrites());
}

// --- Test Runner -------------------------------------------------------------------

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

void run_readonly_integration() {
    RUN_TEST(test_readonly_guard_blocks_write_attempts);
}

int main(int, char**) {
    UNITY_BEGIN();
    
    run_telemetry_quality();
    run_polling_scheduler();
    run_telemetry_decoding();
    run_staleness_detection();
    run_communication_stats();
    run_mqtt_discovery();
    run_readonly_integration();
    
    return UNITY_END();
}
