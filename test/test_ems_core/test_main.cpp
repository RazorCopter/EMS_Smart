// SPDX-License-Identifier: MIT
//
// Unit tests for EMS Core Domain Contracts & Constraint Pipeline (Milestone
// M3).

#include "ems/constraint_pipeline.h"
#include "ems/ems_core_contracts.h"
#include "ems/ems_rest_api.h"
#include "ems/strategy_engine.h"
#include <unity.h>

#include <cstdio>
#include <cstring>

using namespace heliograph::ems;

void setUp(void) {}
void tearDown(void) {}

void test_strategy_mode_string_conversion() {
  TEST_ASSERT_EQUAL_STRING("SelfUse",
                           strategyModeToString(StrategyMode::SelfUse));
  TEST_ASSERT_EQUAL_STRING("ChargeFirst",
                           strategyModeToString(StrategyMode::ChargeFirst));
  TEST_ASSERT_EQUAL_STRING("SolarOptimized",
                           strategyModeToString(StrategyMode::SolarOptimized));
  TEST_ASSERT_EQUAL_STRING("TouTariff",
                           strategyModeToString(StrategyMode::TouTariff));
  TEST_ASSERT_EQUAL_STRING(
      "ScheduledMaintenance",
      strategyModeToString(StrategyMode::ScheduledMaintenance));
  TEST_ASSERT_EQUAL_STRING("SafeIdle",
                           strategyModeToString(StrategyMode::SafeIdle));

  TEST_ASSERT_EQUAL(StrategyMode::SelfUse, strategyModeFromString("SelfUse"));
  TEST_ASSERT_EQUAL(StrategyMode::ChargeFirst,
                    strategyModeFromString("ChargeFirst"));
  TEST_ASSERT_EQUAL(StrategyMode::SolarOptimized,
                    strategyModeFromString("SolarOptimized"));
  TEST_ASSERT_EQUAL(StrategyMode::TouTariff,
                    strategyModeFromString("TouTariff"));
  TEST_ASSERT_EQUAL(StrategyMode::ScheduledMaintenance,
                    strategyModeFromString("ScheduledMaintenance"));
  TEST_ASSERT_EQUAL(StrategyMode::SafeIdle, strategyModeFromString("SafeIdle"));
  TEST_ASSERT_EQUAL(StrategyMode::SelfUse,
                    strategyModeFromString("NonExistentFallback"));
}

void test_authority_source_string_conversion() {
  TEST_ASSERT_EQUAL_STRING(
      "LocalAutonomous",
      authoritySourceToString(AuthoritySource::LocalAutonomous));
  TEST_ASSERT_EQUAL_STRING(
      "ScheduledMaintenance",
      authoritySourceToString(AuthoritySource::ScheduledMaintenance));
  TEST_ASSERT_EQUAL_STRING(
      "MqttRemoteOverride",
      authoritySourceToString(AuthoritySource::MqttRemoteOverride));
  TEST_ASSERT_EQUAL_STRING(
      "RestApiOverride",
      authoritySourceToString(AuthoritySource::RestApiOverride));
  TEST_ASSERT_EQUAL_STRING(
      "SafetyEmergency",
      authoritySourceToString(AuthoritySource::SafetyEmergency));
}

void test_decision_record_json_serialization() {
  EmsDecisionRecord record;
  record.sequenceId = 42;
  record.timestampMs = 1700000000000ULL;
  record.strategy = StrategyMode::SelfUse;
  record.authority = AuthoritySource::LocalAutonomous;
  record.proposedPowerW = 1500;
  record.authorizedPowerW = 1200;
  record.actuatedPowerW = 0;
  record.appliedConstraintsMask =
      ConstraintRampRateSlew | ConstraintBmsChargeLimit;
  std::snprintf(record.justification, sizeof(record.justification),
                "Ramped power to 1200 W");

  char buffer[256];
  size_t len = record.toJson(buffer, sizeof(buffer));
  TEST_ASSERT_TRUE(len > 0);
  TEST_ASSERT_TRUE(std::strstr(buffer, "\"seq\":42") != nullptr);
  TEST_ASSERT_TRUE(std::strstr(buffer, "\"strategy\":\"SelfUse\"") != nullptr);
  TEST_ASSERT_TRUE(std::strstr(buffer, "\"authority\":\"LocalAutonomous\"") !=
                   nullptr);
  TEST_ASSERT_TRUE(std::strstr(buffer, "\"proposed_w\":1500") != nullptr);
  TEST_ASSERT_TRUE(std::strstr(buffer, "\"authorized_w\":1200") != nullptr);
  TEST_ASSERT_TRUE(std::strstr(buffer, "\"actuated_w\":0") != nullptr);
  TEST_ASSERT_TRUE(std::strstr(buffer, "Ramped power to 1200 W") != nullptr);
}

void test_failsafe_when_telemetry_invalid() {
  ConstraintPipeline pipeline;
  TelemetryInput telem;
  telem.timestampMs = 1000;
  telem.gridMeterValid = false; // Meter failed
  telem.batteryBmsValid = true;
  telem.batterySocPct = 50.0f;

  auto dec = pipeline.applyConstraints(2000, telem, 0, StrategyMode::SelfUse,
                                       AuthoritySource::LocalAutonomous, 1);
  TEST_ASSERT_EQUAL_INT32(0, dec.authorizedPowerW);
  TEST_ASSERT_EQUAL(StrategyMode::SafeIdle, dec.strategy);
  TEST_ASSERT_TRUE(std::strstr(dec.justification, "Telemetry invalid") !=
                   nullptr);
}

void test_constraint_soc_reserve_floor_blocks_discharge() {
  TechnicalConstraints c;
  c.socReserveFloorPct = 20.0f;
  ConstraintPipeline pipeline(c);

  TelemetryInput telem;
  telem.timestampMs = 1000;
  telem.gridMeterValid = true;
  telem.batteryBmsValid = true;
  telem.batterySocPct = 19.5f; // Below reserve floor

  auto dec = pipeline.applyConstraints(-1500, telem, 0, StrategyMode::SelfUse,
                                       AuthoritySource::LocalAutonomous, 2);
  TEST_ASSERT_EQUAL_INT32(0, dec.authorizedPowerW);
  TEST_ASSERT_TRUE(dec.hasConstraint(ConstraintSocReserveFloor));
}

void test_constraint_soc_ceiling_blocks_charge() {
  ConstraintPipeline pipeline;
  TelemetryInput telem;
  telem.timestampMs = 1000;
  telem.gridMeterValid = true;
  telem.batteryBmsValid = true;
  telem.batterySocPct = 100.0f; // Fully charged

  auto dec = pipeline.applyConstraints(2000, telem, 0, StrategyMode::SelfUse,
                                       AuthoritySource::LocalAutonomous, 3);
  TEST_ASSERT_EQUAL_INT32(0, dec.authorizedPowerW);
  TEST_ASSERT_TRUE(dec.hasConstraint(ConstraintSocMaxCeiling));
}

void test_constraint_bms_charge_and_discharge_limits() {
  TechnicalConstraints c;
  c.maxChargePowerW = 2500;
  c.maxDischargePowerW = 2000;
  c.allowGridCharging = true;
  ConstraintPipeline pipeline(c);

  TelemetryInput telem;
  telem.timestampMs = 1000;
  telem.gridMeterValid = true;
  telem.batteryBmsValid = true;
  telem.batterySocPct = 50.0f;

  // Exceed charge limit
  auto decCharge =
      pipeline.applyConstraints(3500, telem, 2500, StrategyMode::SelfUse,
                                AuthoritySource::LocalAutonomous, 4);
  TEST_ASSERT_EQUAL_INT32(2500, decCharge.authorizedPowerW);
  TEST_ASSERT_TRUE(decCharge.hasConstraint(ConstraintBmsChargeLimit));

  // Exceed discharge limit
  auto decDischarge =
      pipeline.applyConstraints(-3000, telem, -2000, StrategyMode::SelfUse,
                                AuthoritySource::LocalAutonomous, 5);
  TEST_ASSERT_EQUAL_INT32(-2000, decDischarge.authorizedPowerW);
  TEST_ASSERT_TRUE(decDischarge.hasConstraint(ConstraintBmsDischargeLimit));
}

void test_constraint_grid_import_disallowed() {
  TechnicalConstraints c;
  c.allowGridCharging = false;
  ConstraintPipeline pipeline(c);

  TelemetryInput telem;
  telem.timestampMs = 1000;
  telem.gridMeterValid = true;
  telem.batteryBmsValid = true;
  telem.batterySocPct = 50.0f;
  telem.pvPowerW = 1000;
  telem.loadPowerW = 400; // Available solar surplus = 600 W

  // Requesting 1500 W charge without grid charging enabled
  auto dec = pipeline.applyConstraints(1500, telem, 600, StrategyMode::SelfUse,
                                       AuthoritySource::LocalAutonomous, 6);
  TEST_ASSERT_EQUAL_INT32(600, dec.authorizedPowerW);
  TEST_ASSERT_TRUE(dec.hasConstraint(ConstraintGridImportDisallowed));
}

void test_constraint_ramp_rate_and_deadband() {
  TechnicalConstraints c;
  c.maxRampRateWPerS = 300;
  c.deadbandW = 25;
  c.allowGridCharging = true;
  ConstraintPipeline pipeline(c);

  TelemetryInput telem;
  telem.timestampMs = 1000;
  telem.gridMeterValid = true;
  telem.batteryBmsValid = true;
  telem.batterySocPct = 50.0f;

  // Ramp limiting: from 0 W to 1000 W should be capped at 300 W
  auto decRamp =
      pipeline.applyConstraints(1000, telem, 0, StrategyMode::SelfUse,
                                AuthoritySource::LocalAutonomous, 7);
  TEST_ASSERT_EQUAL_INT32(300, decRamp.authorizedPowerW);
  TEST_ASSERT_TRUE(decRamp.hasConstraint(ConstraintRampRateSlew));

  // Deadband: previous power 500 W, requested 515 W (< 25 W delta)
  auto decDeadband =
      pipeline.applyConstraints(515, telem, 500, StrategyMode::SelfUse,
                                AuthoritySource::LocalAutonomous, 8);
  TEST_ASSERT_EQUAL_INT32(500, decDeadband.authorizedPowerW);
  TEST_ASSERT_TRUE(decDeadband.hasConstraint(ConstraintDeadbandJitter));
}

void test_m4_strategy_self_use_surplus_and_deficit() {
  StrategyConfig sc;
  sc.mode = StrategyMode::SelfUse;
  sc.socReserveFloorPct = 20.0f;

  TechnicalConstraints tc;
  tc.maxChargePowerW = 3000;
  tc.maxDischargePowerW = 3000;
  tc.maxRampRateWPerS = 5000;
  tc.deadbandW = 0;

  StrategyEngine engine(sc, tc);

  // Scenario 1: Solar surplus, grid export of -1500 W. Battery at 0 W.
  TelemetryInput telemSurplus;
  telemSurplus.timestampMs = 1000;
  telemSurplus.gridMeterValid = true;
  telemSurplus.batteryBmsValid = true;
  telemSurplus.pvPowerW = 2000;
  telemSurplus.loadPowerW = 500;
  telemSurplus.gridPowerW = -1500;
  telemSurplus.batterySocPct = 50.0f;

  auto dec1 = engine.evaluate(telemSurplus);
  TEST_ASSERT_EQUAL_INT32(1500, dec1.authorizedPowerW);
  TEST_ASSERT_EQUAL_INT32(1500, engine.currentPower());

  // Scenario 2: Evening load deficit, grid import of +800 W. Battery set at 0
  // W.
  engine.setCurrentPower(0);
  TelemetryInput telemDeficit;
  telemDeficit.timestampMs = 2000;
  telemDeficit.gridMeterValid = true;
  telemDeficit.batteryBmsValid = true;
  telemDeficit.gridPowerW = 800;
  telemDeficit.batterySocPct = 50.0f;

  auto dec2 = engine.evaluate(telemDeficit);
  TEST_ASSERT_EQUAL_INT32(-800, dec2.authorizedPowerW);
}

void test_m4_strategy_self_use_soc_floor_protection() {
  StrategyConfig sc;
  sc.mode = StrategyMode::SelfUse;
  sc.socReserveFloorPct = 20.0f;

  TechnicalConstraints tc;
  tc.maxRampRateWPerS = 5000;
  tc.deadbandW = 0;
  tc.socReserveFloorPct = 20.0f;

  StrategyEngine engine(sc, tc);
  engine.setCurrentPower(0);

  // Demand discharge, but battery SOC is 18% <= 20%
  TelemetryInput telem;
  telem.timestampMs = 1000;
  telem.gridMeterValid = true;
  telem.batteryBmsValid = true;
  telem.gridPowerW = 1000;
  telem.batterySocPct = 18.0f;

  auto dec = engine.evaluate(telem);
  TEST_ASSERT_EQUAL_INT32(0, dec.authorizedPowerW);
  TEST_ASSERT_TRUE(dec.hasConstraint(ConstraintSocReserveFloor));
}

void test_m4_strategy_charge_first_behavior() {
  StrategyConfig sc;
  sc.mode = StrategyMode::ChargeFirst;
  sc.socChargeFirstTargetPct = 90.0f;
  sc.chargeFirstGridAssistW = 1000;

  TechnicalConstraints tc;
  tc.maxChargePowerW = 3000;
  tc.allowGridCharging = true;
  tc.maxRampRateWPerS = 5000;
  tc.deadbandW = 0;

  StrategyEngine engine(sc, tc);
  engine.setCurrentPower(0);

  TelemetryInput telem;
  telem.timestampMs = 1000;
  telem.gridMeterValid = true;
  telem.batteryBmsValid = true;
  telem.pvPowerW = 1500;
  telem.loadPowerW = 500;
  telem.batterySocPct = 60.0f;

  auto dec = engine.evaluate(telem);
  TEST_ASSERT_EQUAL_INT32(2000, dec.authorizedPowerW);
}

void test_m5_strategy_scheduled_maintenance_charging_and_completion() {
  StrategyConfig sc;
  sc.mode = StrategyMode::ScheduledMaintenance;
  sc.maintenanceTargetSocPct = 100.0f;
  sc.maintenancePowerW = 1800;
  sc.maintenanceTimeoutSec = 3600;

  TechnicalConstraints tc;
  tc.maxChargePowerW = 3000;
  tc.allowGridCharging = true;
  tc.maxRampRateWPerS = 5000;
  tc.deadbandW = 0;

  StrategyEngine engine(sc, tc);
  engine.setCurrentPower(0);

  // Cycle 1: SOC 80% -> charges at maintenance power 1800 W
  TelemetryInput telem1;
  telem1.timestampMs = 1000;
  telem1.gridMeterValid = true;
  telem1.batteryBmsValid = true;
  telem1.batterySocPct = 80.0f;

  auto dec1 = engine.evaluate(telem1);
  TEST_ASSERT_EQUAL_INT32(1800, dec1.authorizedPowerW);

  // Cycle 2: SOC reaches 100% -> maintenance marks complete, power returns to 0
  // W
  TelemetryInput telem2;
  telem2.timestampMs = 2000;
  telem2.gridMeterValid = true;
  telem2.batteryBmsValid = true;
  telem2.batterySocPct = 100.0f;

  auto dec2 = engine.evaluate(telem2);
  TEST_ASSERT_EQUAL_INT32(0, dec2.authorizedPowerW);
}

void test_m6_strategy_solar_optimized_curtailment_prevention() {
  StrategyConfig sc;
  sc.mode = StrategyMode::SolarOptimized;
  sc.allowSolarForecastCurtailmentPrevention = true;
  sc.solarForecastExcessKwh = 12.0f; // High solar excess forecast

  TechnicalConstraints tc;
  tc.maxChargePowerW = 3000;
  tc.allowGridCharging = true;
  tc.maxRampRateWPerS = 5000;
  tc.deadbandW = 0;

  StrategyEngine engine(sc, tc);
  engine.setCurrentPower(0);

  TelemetryInput telem;
  telem.timestampMs = 1000;
  telem.gridMeterValid = true;
  telem.batteryBmsValid = true;
  telem.pvPowerW = 2500;
  telem.loadPowerW = 500;
  telem.gridPowerW = -2000;    // Surplus 2000 W
  telem.batterySocPct = 60.0f; // Above 50% threshold

  auto dec = engine.evaluate(telem);
  // Intent is curtailed to 50% (1000 W) to avoid filling battery before solar
  // peak
  TEST_ASSERT_EQUAL_INT32(1000, dec.authorizedPowerW);
}

void test_m7_strategy_tou_tariff_optimization() {
  StrategyConfig sc;
  sc.mode = StrategyMode::TouTariff;
  sc.touLowTariffWindowActive = true;
  sc.touScheduledChargePowerW = 2500;
  sc.touTargetSocPct = 80.0f;

  TechnicalConstraints tc;
  tc.maxChargePowerW = 3000;
  tc.allowGridCharging = true; // Off-peak grid charging permitted
  tc.maxRampRateWPerS = 5000;
  tc.deadbandW = 0;

  StrategyEngine engine(sc, tc);
  engine.setCurrentPower(0);

  TelemetryInput telem;
  telem.timestampMs = 1000;
  telem.gridMeterValid = true;
  telem.batteryBmsValid = true;
  telem.gridPowerW = 200;
  telem.batterySocPct = 40.0f; // Below 80% target

  auto dec = engine.evaluate(telem);
  TEST_ASSERT_EQUAL_INT32(2500, dec.authorizedPowerW);
}

void test_m8_ems_rest_api_state_formatting_and_update() {
  StrategyConfig sc;
  sc.mode = StrategyMode::SelfUse;
  sc.socReserveFloorPct = 20.0f;

  TechnicalConstraints tc;
  tc.maxChargePowerW = 3000;
  tc.maxDischargePowerW = 3000;
  tc.allowGridCharging = false;

  StrategyEngine engine(sc, tc);
  engine.setCurrentPower(1200);

  TelemetryInput telem;
  telem.timestampMs = 1000;
  telem.gridMeterValid = true;
  telem.batteryBmsValid = true;
  telem.batterySocPct = 55.0f;
  telem.gridPowerW = -500;
  telem.pvPowerW = 2000;
  telem.loadPowerW = 1500;

  std::string json = formatEmsStateJson(engine, telem);
  TEST_ASSERT_TRUE(json.find("\"mode\":\"SelfUse\"") != std::string::npos);
  TEST_ASSERT_TRUE(json.find("\"battery_soc_pct\":55.0") != std::string::npos);
  TEST_ASSERT_TRUE(json.find("\"soc_reserve_floor_pct\":20.0") !=
                   std::string::npos);

  // Test updating EMS config via JSON
  std::string updateJson = "{\"mode\":\"ChargeFirst\",\"soc_reserve_floor_"
                           "pct\":25.0,\"allow_grid_charging\":true}";
  std::string err;
  bool ok = updateEmsFromJson(engine, updateJson, err);
  TEST_ASSERT_TRUE(ok);
  TEST_ASSERT_EQUAL(StrategyMode::ChargeFirst, engine.config().mode);
  TEST_ASSERT_EQUAL_FLOAT(25.0f, engine.config().socReserveFloorPct);
  TEST_ASSERT_TRUE(engine.constraints().allowGridCharging);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_strategy_mode_string_conversion);
  RUN_TEST(test_authority_source_string_conversion);
  RUN_TEST(test_decision_record_json_serialization);
  RUN_TEST(test_failsafe_when_telemetry_invalid);
  RUN_TEST(test_constraint_soc_reserve_floor_blocks_discharge);
  RUN_TEST(test_constraint_soc_ceiling_blocks_charge);
  RUN_TEST(test_constraint_bms_charge_and_discharge_limits);
  RUN_TEST(test_constraint_grid_import_disallowed);
  RUN_TEST(test_constraint_ramp_rate_and_deadband);
  RUN_TEST(test_m4_strategy_self_use_surplus_and_deficit);
  RUN_TEST(test_m4_strategy_self_use_soc_floor_protection);
  RUN_TEST(test_m4_strategy_charge_first_behavior);
  RUN_TEST(test_m5_strategy_scheduled_maintenance_charging_and_completion);
  RUN_TEST(test_m6_strategy_solar_optimized_curtailment_prevention);
  RUN_TEST(test_m7_strategy_tou_tariff_optimization);
  RUN_TEST(test_m8_ems_rest_api_state_formatting_and_update);
  return UNITY_END();
}
