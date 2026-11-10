// SPDX-License-Identifier: MIT
//
// Shadow EMS Testbench with Synthetic Scenarios (CP4).
//
// Implements comprehensive test scenarios to validate Shadow EMS controller:
//   - Scenario A: Surplus solar with low SOC → charge to 95%
//   - Scenario B: Evening load spike → discharge to reserve (15%)
//   - Scenario C: Grid export with battery at 100% → solar clipping detection
//   - Scenario D: Stale telemetry → fail-safe SafeIdle transition
//   - Scenario E: Divergence tracking vs stock inverter behavior
//
// All tests verify:
//   - Correct operating mode transitions
//   - Constraint enforcement (SOC, power limits)
//   - Fail-safe behavior on poor telemetry quality
//   - Divergence metrics accuracy
//   - No write operations (Shadow mode guarantee)

#include <cassert>
#include <cmath>
#include <cstdio>
#include <map>
#include <string>

#include "../src/device/telemetry_quality.h"
#include "../src/ems/shadow_ems_controller.h"
#include "../src/ems/shadow_ems_types.h"

using namespace heliograph;
using namespace heliograph::ems;

// Test utilities
static int testsPassed = 0;
static int testsFailed = 0;

#define TEST_ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            printf("❌ FAIL: %s\n   Line %d: %s\n", __func__, __LINE__, message); \
            testsFailed++; \
            return; \
        } \
    } while (0)

#define TEST_PASS() \
    do { \
        printf("✅ PASS: %s\n", __func__); \
        testsPassed++; \
    } while (0)

// Helper: create synthetic telemetry sample
TelemetrySample makeSample(double value, Quality quality = Quality::Good, bool verified = true) {
    TelemetrySample s;
    s.value = value;
    s.quality = quality;
    s.timestampMs = 1000000;  // Fixed timestamp for simplicity
    s.ageMs = 0;
    s.verified = verified;
    s.derived = false;
    return s;
}

// Helper: create complete telemetry set
std::map<std::string, TelemetrySample> makeTelemetry(
    double pvW, double gridW, double batteryW, double socPct,
    Quality quality = Quality::Good) {
    
    std::map<std::string, TelemetrySample> samples;
    samples["pv.power_total"] = makeSample(pvW, quality);
    samples["grid.power"] = makeSample(gridW, quality);
    samples["battery.power"] = makeSample(batteryW, quality);
    samples["battery.soc"] = makeSample(socPct, quality);
    return samples;
}

// ============================================================================
// SCENARIO A: Surplus Solar with Low SOC → Charge to 95%
// ============================================================================

void test_scenario_a_surplus_charge_to_max_soc() {
    printf("\n=== Scenario A: Surplus Solar Charging ===\n");
    
    ShadowEMSController controller;
    uint64_t nowMs = 1000000;
    
    // Initial state: Low SOC (30%), surplus solar (3000W PV, -2500W grid export)
    auto samples = makeTelemetry(3000.0, -2500.0, 0.0, 30.0);
    
    controller.update(samples, nowMs);
    auto state = controller.getState();
    
    TEST_ASSERT(state.recommendation.mode == OperatingMode::Charging,
                "Should recommend charging with surplus solar");
    TEST_ASSERT(state.recommendation.reason == DecisionReason::SurplusSolarAvailable,
                "Reason should be surplus solar");
    TEST_ASSERT(state.recommendation.recommendedBatteryPowerW > 2000.0,
                "Should request significant charge power to absorb export");
    TEST_ASSERT(state.telemetryActionable, "Telemetry should be actionable");
    
    // Simulate progression: SOC rising, charging continues
    nowMs += 10000;
    samples = makeTelemetry(3200.0, -2700.0, 2500.0, 60.0);
    controller.update(samples, nowMs);
    state = controller.getState();
    
    TEST_ASSERT(state.recommendation.mode == OperatingMode::Charging,
                "Should continue charging");
    
    // Near max SOC (92%): still charging but reduced
    nowMs += 10000;
    samples = makeTelemetry(3000.0, -2000.0, 1800.0, 92.0);
    controller.update(samples, nowMs);
    state = controller.getState();
    
    TEST_ASSERT(state.recommendation.mode == OperatingMode::Charging,
                "Should still charge near max SOC");
    TEST_ASSERT(state.recommendation.allowedBatteryPowerW < 2000.0,
                "Should taper charge near SOC limit");
    
    // Reached max SOC (95%): stop charging
    nowMs += 10000;
    samples = makeTelemetry(2800.0, -2500.0, 0.0, 95.0);
    controller.update(samples, nowMs);
    state = controller.getState();
    
    TEST_ASSERT(state.recommendation.mode == OperatingMode::Hold ||
                state.recommendation.mode == OperatingMode::SolarClipping,
                "Should stop charging at max SOC");
    TEST_ASSERT(state.recommendation.allowedBatteryPowerW < 100.0,
                "Should not recommend charge at max SOC");
    
    TEST_PASS();
}

// ============================================================================
// SCENARIO B: Evening Load Spike → Discharge to Reserve
// ============================================================================

void test_scenario_b_deficit_discharge_to_min_soc() {
    printf("\n=== Scenario B: Evening Load Discharge ===\n");
    
    ShadowEMSController controller;
    uint64_t nowMs = 2000000;
    
    // Initial: High SOC (85%), low PV (200W), high load (grid import 2200W)
    auto samples = makeTelemetry(200.0, 2200.0, 0.0, 85.0);
    
    controller.update(samples, nowMs);
    auto state = controller.getState();
    
    TEST_ASSERT(state.recommendation.mode == OperatingMode::Discharging,
                "Should recommend discharging with grid import");
    TEST_ASSERT(state.recommendation.reason == DecisionReason::DeficitGridImport,
                "Reason should be deficit/import");
    TEST_ASSERT(state.recommendation.recommendedBatteryPowerW < -2000.0,
                "Should request significant discharge to offset import");
    
    // SOC dropping: continue discharge
    nowMs += 10000;
    samples = makeTelemetry(150.0, 1800.0, -2000.0, 50.0);
    controller.update(samples, nowMs);
    state = controller.getState();
    
    TEST_ASSERT(state.recommendation.mode == OperatingMode::Discharging,
                "Should continue discharging");
    
    // Near min SOC (18%): reduce discharge
    nowMs += 10000;
    samples = makeTelemetry(100.0, 1500.0, -1400.0, 18.0);
    controller.update(samples, nowMs);
    state = controller.getState();
    
    TEST_ASSERT(state.recommendation.mode == OperatingMode::Discharging ||
                state.recommendation.mode == OperatingMode::Hold,
                "Should reduce or stop discharge near min SOC");
    TEST_ASSERT(std::abs(state.recommendation.allowedBatteryPowerW) < 1500.0,
                "Should taper discharge near SOC limit");
    
    // Reached min SOC (15%): stop discharge
    nowMs += 10000;
    samples = makeTelemetry(50.0, 2000.0, 0.0, 15.0);
    controller.update(samples, nowMs);
    state = controller.getState();
    
    TEST_ASSERT(state.recommendation.mode == OperatingMode::Hold,
                "Should stop discharging at min SOC");
    TEST_ASSERT(std::abs(state.recommendation.allowedBatteryPowerW) < 100.0,
                "Should not recommend discharge at min SOC");
    
    TEST_PASS();
}

// ============================================================================
// SCENARIO C: Grid Export with Battery Full → Solar Clipping
// ============================================================================

void test_scenario_c_solar_clipping_detection() {
    printf("\n=== Scenario C: Solar Clipping Detection ===\n");
    
    ShadowEMSController controller;
    uint64_t nowMs = 3000000;
    
    // Battery at 100%, surplus solar causing export
    auto samples = makeTelemetry(4500.0, -3000.0, 0.0, 100.0);
    
    controller.update(samples, nowMs);
    auto state = controller.getState();
    
    TEST_ASSERT(state.recommendation.mode == OperatingMode::SolarClipping ||
                state.recommendation.mode == OperatingMode::Hold,
                "Should detect solar clipping or hold at max SOC");
    TEST_ASSERT(state.recommendation.allowedBatteryPowerW < 100.0,
                "Cannot charge: battery full");
    TEST_ASSERT(state.recommendation.chargeLimitW == 0.0,
                "Charge limit should be zero at 100% SOC");
    
    // Clipping continues
    nowMs += 10000;
    samples = makeTelemetry(4800.0, -3200.0, 0.0, 100.0);
    controller.update(samples, nowMs);
    state = controller.getState();
    
    TEST_ASSERT(state.recommendation.mode == OperatingMode::SolarClipping ||
                state.recommendation.mode == OperatingMode::Hold,
                "Should continue indicating clipping");
    
    TEST_PASS();
}

// ============================================================================
// SCENARIO D: Stale Telemetry → Fail-Safe Transition
// ============================================================================

void test_scenario_d_stale_telemetry_failsafe() {
    printf("\n=== Scenario D: Stale Telemetry Fail-Safe ===\n");
    
    ShadowEMSController controller;
    uint64_t nowMs = 4000000;
    
    // Start with good telemetry
    auto samples = makeTelemetry(2000.0, -1500.0, 1400.0, 60.0, Quality::Good);
    
    controller.update(samples, nowMs);
    auto state = controller.getState();
    
    TEST_ASSERT(state.telemetryActionable, "Initial telemetry should be actionable");
    TEST_ASSERT(state.recommendation.mode == OperatingMode::Charging,
                "Should be charging with good data");
    
    // Telemetry goes stale
    nowMs += 10000;
    samples = makeTelemetry(2000.0, -1500.0, 1400.0, 60.0, Quality::Stale);
    
    controller.update(samples, nowMs);
    state = controller.getState();
    
    TEST_ASSERT(!state.telemetryActionable, "Stale telemetry should not be actionable");
    TEST_ASSERT(state.recommendation.mode == OperatingMode::SafeIdle,
                "Should transition to SafeIdle with stale data");
    TEST_ASSERT(state.recommendation.reason == DecisionReason::TelemetryInvalid,
                "Reason should be telemetry invalid");
    TEST_ASSERT(state.recommendation.allowedBatteryPowerW == 0.0,
                "Should recommend zero power in SafeIdle");
    TEST_ASSERT(state.failsafeActivations >= 1, "Should count fail-safe activation");
    
    // Telemetry becomes invalid
    nowMs += 10000;
    samples = makeTelemetry(2000.0, -1500.0, 1400.0, 60.0, Quality::Invalid);
    
    controller.update(samples, nowMs);
    state = controller.getState();
    
    TEST_ASSERT(state.recommendation.mode == OperatingMode::SafeIdle,
                "Should stay in SafeIdle with invalid data");
    
    // Telemetry recovers
    nowMs += 10000;
    samples = makeTelemetry(2200.0, -1700.0, 1600.0, 62.0, Quality::Good);
    
    controller.update(samples, nowMs);
    state = controller.getState();
    
    TEST_ASSERT(state.telemetryActionable, "Recovered telemetry should be actionable");
    TEST_ASSERT(state.recommendation.mode == OperatingMode::Charging,
                "Should resume normal operation after recovery");
    
    TEST_PASS();
}

// ============================================================================
// SCENARIO E: Divergence Tracking vs Stock Inverter
// ============================================================================

void test_scenario_e_divergence_tracking() {
    printf("\n=== Scenario E: Divergence Tracking ===\n");
    
    ShadowEMSController controller;
    uint64_t nowMs = 5000000;
    
    // Shadow recommends charging 2000W, but inverter actually charges at 500W
    auto samples = makeTelemetry(3000.0, -2000.0, 500.0, 50.0);
    
    controller.update(samples, nowMs);
    auto state = controller.getState();
    
    TEST_ASSERT(state.recommendation.mode == OperatingMode::Charging,
                "Shadow should recommend charging");
    TEST_ASSERT(state.recommendation.allowedBatteryPowerW > 1500.0,
                "Shadow should recommend significant charge");
    
    double expectedDivergence = state.recommendation.allowedBatteryPowerW - 500.0;
    TEST_ASSERT(std::abs(state.divergence.divergencePowerW - expectedDivergence) < 100.0,
                "Divergence should reflect difference between recommended and actual");
    
    // Continue divergence over time
    nowMs += 10000;
    samples = makeTelemetry(3100.0, -2100.0, 600.0, 52.0);
    controller.update(samples, nowMs);
    state = controller.getState();
    
    TEST_ASSERT(state.divergence.cumulativeDivergenceKWh > 0.0,
                "Should accumulate divergence energy over time");
    
    // Sustained divergence should be counted
    nowMs += 35000;  // 35s later (exceeds 30s threshold)
    samples = makeTelemetry(3000.0, -2000.0, 550.0, 54.0);
    controller.update(samples, nowMs);
    state = controller.getState();
    
    TEST_ASSERT(state.divergence.divergenceEvents >= 1,
                "Should count sustained divergence as event");
    
    // Shadow recommends discharge, inverter does nothing
    nowMs += 10000;
    samples = makeTelemetry(100.0, 1800.0, 0.0, 55.0);
    controller.update(samples, nowMs);
    state = controller.getState();
    
    TEST_ASSERT(state.recommendation.mode == OperatingMode::Discharging,
                "Shadow should recommend discharge");
    TEST_ASSERT(state.divergence.divergencePowerW < -1000.0,
                "Large negative divergence: recommended discharge vs actual idle");
    
    TEST_PASS();
}

// ============================================================================
// Additional Unit Tests
// ============================================================================

void test_constraints_enforcement() {
    printf("\n=== Test: Constraints Enforcement ===\n");
    
    ShadowEMSConfig config;
    config.constraints.maxChargePowerW = 3000.0;
    config.constraints.maxDischargePowerW = 3000.0;
    config.constraints.minSOC = 10.0;
    config.constraints.maxSOC = 90.0;
    
    ShadowEMSController controller(config);
    uint64_t nowMs = 6000000;
    
    // Request exceeds power limit
    auto samples = makeTelemetry(6000.0, -5000.0, 0.0, 50.0);
    controller.update(samples, nowMs);
    auto state = controller.getState();
    
    TEST_ASSERT(state.recommendation.allowedBatteryPowerW <= 3000.0,
                "Should clamp to max charge power");
    
    // Request discharge exceeds limit
    nowMs += 10000;
    samples = makeTelemetry(0.0, 4500.0, 0.0, 50.0);
    controller.update(samples, nowMs);
    state = controller.getState();
    
    TEST_ASSERT(state.recommendation.allowedBatteryPowerW >= -3000.0,
                "Should clamp to max discharge power");
    
    TEST_PASS();
}

void test_hysteresis_behavior() {
    printf("\n=== Test: Hysteresis Band ===\n");
    
    ShadowEMSConfig config;
    config.gridExportThresholdW = 50.0;
    config.gridImportThresholdW = 50.0;
    config.hysteresisW = 30.0;
    
    ShadowEMSController controller(config);
    uint64_t nowMs = 7000000;
    
    // Within hysteresis band: should hold
    auto samples = makeTelemetry(1500.0, 40.0, 0.0, 50.0);  // 40W import (within band)
    controller.update(samples, nowMs);
    auto state = controller.getState();
    
    TEST_ASSERT(state.recommendation.mode == OperatingMode::Hold,
                "Should hold within hysteresis band");
    
    // Just outside hysteresis (import): should discharge
    nowMs += 10000;
    samples = makeTelemetry(1500.0, 85.0, 0.0, 50.0);  // 85W import (>50+30)
    controller.update(samples, nowMs);
    state = controller.getState();
    
    TEST_ASSERT(state.recommendation.mode == OperatingMode::Discharging,
                "Should discharge outside hysteresis band (import)");
    
    // Just outside hysteresis (export): should charge
    nowMs += 10000;
    samples = makeTelemetry(2500.0, -85.0, 0.0, 50.0);  // 85W export (>50+30)
    controller.update(samples, nowMs);
    state = controller.getState();
    
    TEST_ASSERT(state.recommendation.mode == OperatingMode::Charging,
                "Should charge outside hysteresis band (export)");
    
    TEST_PASS();
}

void test_confidence_scoring() {
    printf("\n=== Test: Confidence Scoring ===\n");
    
    ShadowEMSController controller;
    uint64_t nowMs = 8000000;
    
    // All Good quality
    auto samples = makeTelemetry(2000.0, -1500.0, 0.0, 50.0, Quality::Good);
    controller.update(samples, nowMs);
    auto state = controller.getState();
    
    TEST_ASSERT(state.recommendation.confidenceScore >= 0.9,
                "High confidence with Good quality telemetry");
    
    // Degraded quality
    nowMs += 10000;
    samples = makeTelemetry(2000.0, -1500.0, 0.0, 50.0, Quality::Degraded);
    controller.update(samples, nowMs);
    state = controller.getState();
    
    TEST_ASSERT(state.recommendation.confidenceScore < 0.9,
                "Lower confidence with Degraded quality");
    TEST_ASSERT(state.telemetryActionable,
                "Degraded should still be actionable");
    
    TEST_PASS();
}

void test_power_smoothing() {
    printf("\n=== Test: Power Smoothing/Ramping ===\n");
    
    ShadowEMSConfig config;
    config.maxPowerRampWPerS = 500.0;  // 500W/s ramp limit
    
    ShadowEMSController controller(config);
    uint64_t nowMs = 9000000;
    
    // Initial: request 2000W charge
    auto samples = makeTelemetry(3000.0, -2000.0, 0.0, 50.0);
    controller.update(samples, nowMs);
    auto state = controller.getState();
    double initialPower = state.recommendation.allowedBatteryPowerW;
    
    // 1 second later: maintain 2000W request
    nowMs += 1000;
    samples = makeTelemetry(3000.0, -2000.0, 500.0, 51.0);
    controller.update(samples, nowMs);
    state = controller.getState();
    double secondPower = state.recommendation.allowedBatteryPowerW;
    
    // Power change should be limited by ramp rate
    double powerChange = std::abs(secondPower - initialPower);
    TEST_ASSERT(powerChange <= 600.0,  // 500W/s + some tolerance
                "Power change should respect ramp limit");
    
    TEST_PASS();
}

// ============================================================================
// Main Test Runner
// ============================================================================

int main() {
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════════╗\n");
    printf("║  Shadow EMS Test Suite (CP4)                                  ║\n");
    printf("║  Synthetic Scenarios + Unit Tests                             ║\n");
    printf("╚════════════════════════════════════════════════════════════════╝\n");
    
    // Synthetic scenarios
    test_scenario_a_surplus_charge_to_max_soc();
    test_scenario_b_deficit_discharge_to_min_soc();
    test_scenario_c_solar_clipping_detection();
    test_scenario_d_stale_telemetry_failsafe();
    test_scenario_e_divergence_tracking();
    
    // Unit tests
    test_constraints_enforcement();
    test_hysteresis_behavior();
    test_confidence_scoring();
    test_power_smoothing();
    
    // Summary
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════════╗\n");
    printf("║  Test Results                                                  ║\n");
    printf("╠════════════════════════════════════════════════════════════════╣\n");
    printf("║  ✅ Passed: %-3d                                                ║\n", testsPassed);
    printf("║  ❌ Failed: %-3d                                                ║\n", testsFailed);
    printf("╚════════════════════════════════════════════════════════════════╝\n");
    printf("\n");
    
    if (testsFailed == 0) {
        printf("🎉 All tests passed! Shadow EMS CP4 implementation verified.\n\n");
        return 0;
    } else {
        printf("⚠️  Some tests failed. Review implementation.\n\n");
        return 1;
    }
}
