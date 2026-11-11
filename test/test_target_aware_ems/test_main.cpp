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

} // namespace

int main() {
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
  std::cout << "PASS: 14 named M4 target-aware EMS scenarios\n";
  return EXIT_SUCCESS;
}
