// SPDX-License-Identifier: MIT

#include "ems/target_aware_ems.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

using namespace heliograph::ems::target;

namespace {

int failures = 0;

void check(bool condition, const std::string &scenario,
           const std::string &detail) {
  if (!condition) {
    ++failures;
    std::cerr << "FAIL [" << scenario << "]: " << detail << '\n';
  }
}

Datum<double> valid(double value) {
  return Datum<double>{{value}, DataQuality::Valid, 0};
}

EMSInput baseline() {
  EMSInput input;
  input.clock = {9 * 60, 1, 5, 15};
  input.socPct = valid(50.0);
  input.batteryPowerW = valid(800.0);
  input.pvPowerW = valid(2000.0);
  input.forecast.available = true;
  input.forecast.ageSeconds = 60;
  for (uint16_t minute = 9 * 60; minute < 15 * 60 + 30; minute += 30) {
    input.forecast.today.push_back({minute, 1.0});
  }
  input.forecast.tomorrowKWh = 8.0;
  input.weather.actualToForecastRatio = valid(1.0);
  return input;
}

void scenario_target_reachable_minimum_power() {
  const std::string name = "target_reachable_minimum_power";
  EMSConfig config;
  config.longevityMaximumSocPct = 100.0;
  TargetAwareEMS core(config);
  const auto result = core.evaluate(baseline(), {}, 1000).recommendation;
  check(result.feasibility == Feasibility::Feasible, name,
        "target should be feasible");
  check(result.batteryPowerW > 0.0, name, "charge recommendation expected");
  check(result.batteryPowerW <= config.maximumChargePowerW, name,
        "must remain clamped");
}

void scenario_target_not_reachable() {
  const std::string name = "target_not_reachable";
  EMSConfig config;
  config.longevityMaximumSocPct = 100.0;
  TargetAwareEMS core(config);
  EMSInput input = baseline();
  input.socPct = valid(10.0);
  input.forecast.today.assign(1, {9 * 60, 0.1});
  const auto result = core.evaluate(input, {}, 1000).recommendation;
  check(result.feasibility == Feasibility::NotFeasible, name,
        "must declare NOT_FEASIBLE");
}

void scenario_low_soc_recovery_window() {
  const std::string name = "low_soc_recovery_window";
  EMSConfig config;
  config.longevityMaximumSocPct = 100.0;
  TargetAwareEMS core(config);
  EMSInput input = baseline();
  input.clock.minuteOfDay = 15 * 60;
  input.socPct = valid(20.0);
  const auto result = core.evaluate(input, {}, 1000).recommendation;
  check(result.reason == RecommendationReason::RecoveryCharge, name,
        "recovery priority expected");
  check(result.batteryPowerW > 0.0, name, "must recommend charging");
}

void scenario_high_soc_target_satisfied() {
  const std::string name = "high_soc_target_satisfied";
  EMSConfig config;
  config.targetSocPct = 95.0;
  config.longevityMaximumSocPct = 95.0;
  TargetAwareEMS core(config);
  EMSInput input = baseline();
  input.socPct = valid(96.0);
  const auto result = core.evaluate(input, {}, 1000).recommendation;
  check(result.reason == RecommendationReason::TargetSatisfied, name,
        "longevity target satisfied");
  check(result.batteryPowerW == 0.0, name, "idle expected");
}

void scenario_night_safe_idle() {
  const std::string name = "night_safe_idle";
  TargetAwareEMS core;
  EMSInput input = baseline();
  input.clock.minuteOfDay = 2 * 60;
  input.pvPowerW = valid(0.0);
  input.forecast.today.clear();
  const auto result = core.evaluate(input, {}, 1000).recommendation;
  check(result.reason == RecommendationReason::NightIdle, name,
        "night reason expected");
  check(result.batteryPowerW == 0.0, name, "night idle expected");
}

void scenario_fog_forecast_reduction_and_hysteresis() {
  const std::string name = "fog_forecast_reduction_and_hysteresis";
  TargetAwareEMS core;
  EMSInput input = baseline();
  input.weather.temperatureC = valid(5.0);
  input.weather.dewPointC = valid(4.0);
  input.weather.radiationWm2 = valid(50.0);
  auto first = core.evaluate(input, {}, 1000);
  check(first.recommendation.fogActive, name, "fog entry expected");
  input.weather.temperatureC = valid(7.0);
  input.weather.dewPointC = valid(4.0);
  input.weather.radiationWm2 = valid(200.0);
  input.weather.actualToForecastRatio = valid(0.8);
  auto second = core.evaluate(input, first.nextState, 1010);
  check(!second.recommendation.fogActive, name, "fog exit hysteresis expected");
}

void scenario_unexpected_load_delay() {
  const std::string name = "unexpected_load_delay";
  EMSConfig config;
  config.longevityMaximumSocPct = 100.0;
  TargetAwareEMS core(config);
  EMSInput input = baseline();
  input.socPct = valid(30.0);
  input.batteryPowerW = valid(100.0);
  const auto result = core.evaluate(input, {}, 1000).recommendation;
  check(result.energy.delayed, name, "low real charge rate must be delayed");
}

void scenario_invalid_and_stale_data_safe_idle() {
  const std::string name = "invalid_and_stale_data_safe_idle";
  TargetAwareEMS core;
  EMSInput input = baseline();
  input.socPct = Datum<double>{std::nullopt, DataQuality::Invalid, 0};
  auto invalid = core.evaluate(input, {}, 1000).recommendation;
  check(invalid.reason == RecommendationReason::DataInvalid &&
            invalid.batteryPowerW == 0.0,
        name, "invalid unknown must not become zero");
  input = baseline();
  input.pvPowerW.ageSeconds = 121;
  auto stale = core.evaluate(input, {}, 1000).recommendation;
  check(stale.reason == RecommendationReason::DataInvalid &&
            stale.batteryPowerW == 0.0,
        name, "stale data must safe-idle");
}

void scenario_clamp_ramp_deadband_hysteresis() {
  const std::string name = "clamp_ramp_deadband_hysteresis";
  EMSConfig config;
  check(setpoint_shaper::shape(6000.0, 0.0, config, true) ==
            config.maximumChargePowerW,
        name, "critical clamp expected");
  check(setpoint_shaper::shape(2000.0, 0.0, config, false) ==
            config.rampUpWPerCycle,
        name, "ramp-up expected");
  check(setpoint_shaper::shape(20.0, 0.0, config, false) == 0.0, name,
        "deadband expected");
  check(setpoint_shaper::shape(1050.0, 1000.0, config, false) == 1000.0, name,
        "hysteresis expected");
}

void scenario_seasonal_priority() {
  const std::string name = "seasonal_priority";
  TargetAwareEMS core;
  EMSInput input = baseline();
  input.clock = {9 * 60, 1, 11, 15};
  input.socPct = valid(20.0);
  input.forecast.tomorrowKWh = 2.0;
  input.forecast.today.assign(1, {9 * 60, 0.2});
  auto winter = core.evaluate(input, {}, 1000).recommendation;
  check(winter.priorityChargeActive, name, "winter priority expected");
  input.clock.month = 5;
  auto summer = core.evaluate(input, {}, 1000).recommendation;
  check(!summer.priorityChargeActive, name, "summer priority must be off");
}

void scenario_calibration_tracker() {
  const std::string name = "calibration_tracker";
  TargetAwareEMS core;
  EMSInput input = baseline();
  input.socPct = valid(100.0);
  EMSState state;
  state.calibrationRequested = true;
  auto first = core.evaluate(input, state, 1000);
  check(first.recommendation.targetSocPct == 100.0, name,
        "calibration overrides longevity");
  auto complete = core.evaluate(input, first.nextState, 1300);
  check(complete.recommendation.calibrationCompleted &&
            complete.nextState.calibrated,
        name, "100 percent for five minutes completes calibration");
  check(!complete.nextState.calibrationRequested, name,
        "normal strategy restored");
}

void scenario_discharge_gate() {
  const std::string name = "discharge_gate";
  EMSConfig config;
  check(setpoint_shaper::shape(-1000.0, 0.0, config, true) == 0.0, name,
        "discharge disabled by default");
  config.allowDischarge = true;
  check(setpoint_shaper::shape(-1000.0, 0.0, config, true) == -1000.0, name,
        "authorized discharge preserves negative convention");
}

void scenario_stale_forecast_conservative_fallback() {
  const std::string name = "stale_forecast_conservative_fallback";
  TargetAwareEMS core;
  EMSInput input = baseline();
  input.forecast.ageSeconds = 14 * 60 * 60 + 1;
  const auto result = core.evaluate(input, {}, 1000).recommendation;
  check(result.conservativeMode, name,
        "stale forecast must degrade explicitly");
  check(result.reason == RecommendationReason::ForecastUnavailable, name,
        "fallback reason expected");
  check(result.batteryPowerW >= 0.0, name,
        "fallback cannot recommend discharge");
}

void scenario_typed_override_is_safety_clamped() {
  const std::string name = "typed_override_is_safety_clamped";
  EMSConfig config;
  TargetAwareEMS core(config);
  EMSInput input = baseline();
  input.overrideInput.targetSocPct = 120.0;
  input.overrideInput.requestedBatteryPowerW = 9000.0;
  const auto result = core.evaluate(input, {}, 1000).recommendation;
  check(result.targetSocPct <= 100.0, name, "target override clamp expected");
  check(result.batteryPowerW <= config.maximumChargePowerW, name,
        "power override clamp expected");
  check(result.reason == RecommendationReason::Override, name,
        "override reason expected");
}

void scenario_approved_parameters_audit() {
  const std::string name = "approved_parameters_audit";
  const EMSConfig cfg;
  check(cfg.targetSocPct == 100.0, name, "target SOC default must be 100%");
  check(cfg.targetMinuteOfDay == 15 * 60 + 30, name,
        "target time default must be 15:30 (930 min)");
  check(cfg.anticipationMinutes == 20, name,
        "operational anticipation default must be 20 min");
  check(cfg.recoveryMinutes == 60, name,
        "final recovery window default must be 60 min");
  check(cfg.chargeEfficiency == 0.90, name,
        "charge efficiency default must be 0.90");
  check(cfg.safetyMargin == 1.20, name, "safety margin default must be 1.20");
  check(cfg.forecastMaximumAgeSeconds == 14 * 60 * 60, name,
        "forecast maximum age must be 14 hours");
  check(cfg.weekdayLoadKW == 0.6, name, "weekday load default must be 0.6 kW");
  check(cfg.weekendLoadKW == 0.9, name, "weekend load default must be 0.9 kW");
  check(cfg.winterStartMonthDay == 1025, name, "winter start must be Oct 25");
  check(cfg.winterEndMonthDay == 228, name, "winter end must be Feb 28");
  check(cfg.tomorrowFavorableKWh == 7.0, name,
        "tomorrow favorable forecast must be >= 7.0 kWh");
  check(cfg.tomorrowMinimumSocPct == 30.0, name,
        "tomorrow minimum SOC must be >= 30%");
  check(cfg.fullSocCalibrationDurationSeconds == 300, name,
        "calibration duration must be 5 minutes (300 s)");
  check(cfg.rampUpWPerCycle == 256.0, name, "ramp up must be 256 W/cycle");
  check(cfg.rampDownWPerCycle == 410.0, name, "ramp down must be 410 W/cycle");
  check(cfg.hysteresisW == 100.0, name, "hysteresis must be 100 W");
  check(!cfg.allowDischarge, name, "allow_discharge must be false by default");
}

void scenario_operational_anticipation_20min_and_recovery_window() {
  const std::string name = "operational_anticipation_20min_and_recovery_window";
  TargetAwareEMS core;
  EMSInput input = baseline();
  input.socPct = valid(80.0); // 20% deficit

  // 1. At 14:00 (minute 840): before recovery window (14:30), normal target
  // charge
  input.clock.minuteOfDay = 14 * 60;
  input.socPct = valid(90.0); // 10% deficit is comfortably feasible
  auto res1 = core.evaluate(input, {}, 1000).recommendation;
  check(res1.reason == RecommendationReason::TargetCharge, name,
        "14:00 must be in normal target charge when feasible");

  // 2. At 14:35 (minute 875): inside 60-min recovery window (14:30..15:30),
  // deficit causes recovery charge
  input.clock.minuteOfDay = 14 * 60 + 35;
  auto res2 = core.evaluate(input, {}, 1000).recommendation;
  check(res2.reason == RecommendationReason::RecoveryCharge, name,
        "14:35 must trigger recovery charge");

  // 3. At 15:10 (minute 910): anticipation deadline (15:30 - 20m),
  // remainingHours must be 0
  input.clock.minuteOfDay = 15 * 60 + 10;
  auto res3 = core.evaluate(input, {}, 1000).recommendation;
  check(
      res3.reason == RecommendationReason::RecoveryCharge, name,
      "15:10 anticipation deadline with deficit must request recovery charge");
}

void scenario_invalid_data_never_converted_to_valid_zero() {
  const std::string name = "invalid_data_never_converted_to_valid_zero";
  TargetAwareEMS core;
  EMSInput input = baseline();
  // Set SOC quality to Invalid, CommError, and Unavailable
  input.socPct.quality = DataQuality::Invalid;
  input.socPct.value =
      0.0; // explicit 0.0 in payload must NOT be trusted as valid
  auto res = core.evaluate(input, {}, 1000).recommendation;
  check(res.reason == RecommendationReason::DataInvalid, name,
        "invalid SOC must yield DataInvalid, never valid 0% evaluation");
  check(res.feasibility == Feasibility::Unknown, name,
        "invalid data must not declare Feasible or NotFeasible");
  check(res.batteryPowerW == 0.0, name,
        "invalid data must emit 0 W safe advisory power");
}

} // namespace

int main() {
  scenario_approved_parameters_audit();
  scenario_operational_anticipation_20min_and_recovery_window();
  scenario_invalid_data_never_converted_to_valid_zero();
  scenario_target_reachable_minimum_power();
  scenario_target_not_reachable();
  scenario_low_soc_recovery_window();
  scenario_high_soc_target_satisfied();
  scenario_night_safe_idle();
  scenario_fog_forecast_reduction_and_hysteresis();
  scenario_unexpected_load_delay();
  scenario_invalid_and_stale_data_safe_idle();
  scenario_clamp_ramp_deadband_hysteresis();
  scenario_seasonal_priority();
  scenario_calibration_tracker();
  scenario_discharge_gate();
  scenario_stale_forecast_conservative_fallback();
  scenario_typed_override_is_safety_clamped();
  if (failures != 0) {
    std::cerr << failures << " M4 assertion(s) failed\n";
    return EXIT_FAILURE;
  }
  std::cout << "PASS: 17 named M4 target-aware EMS scenarios\n";
  return EXIT_SUCCESS;
}
