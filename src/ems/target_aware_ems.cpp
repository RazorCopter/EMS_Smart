// SPDX-License-Identifier: MIT

#include "ems/target_aware_ems.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace heliograph::ems::target {
namespace {

double clamp(double value, double low, double high) {
  return std::max(low, std::min(value, high));
}

bool coreDataValid(const EMSInput &input, const EMSConfig &config) {
  if (!input.socPct.actionable(config.telemetryMaximumAgeSeconds) ||
      !input.batteryPowerW.actionable(config.telemetryMaximumAgeSeconds) ||
      !input.pvPowerW.actionable(config.telemetryMaximumAgeSeconds)) {
    return false;
  }
  return *input.socPct.value >= 0.0 && *input.socPct.value <= 100.0 &&
         std::isfinite(*input.batteryPowerW.value) &&
         *input.pvPowerW.value >= 0.0;
}

double correctedForecastKWh(const EMSInput &input, const EMSConfig &config,
                            bool fog) {
  if (!input.forecast.available ||
      input.forecast.ageSeconds > config.forecastMaximumAgeSeconds) {
    return 0.0;
  }
  double ratio = 1.0;
  if (input.weather.actualToForecastRatio.actionable(
          config.telemetryMaximumAgeSeconds)) {
    ratio = clamp(*input.weather.actualToForecastRatio.value, 0.5, 1.0);
  }
  if (fog) {
    ratio *= 0.3;
  }
  double result = 0.0;
  for (const ForecastSlot &slot : input.forecast.today) {
    if (slot.startMinuteOfDay >= input.clock.minuteOfDay &&
        slot.startMinuteOfDay < config.targetMinuteOfDay &&
        slot.pvEstimateKWh > 0.0) {
      result += slot.pvEstimateKWh * ratio;
    }
  }
  return result;
}

void updateCalibration(double socPct, uint64_t nowSeconds, EMSState &state,
                       Recommendation &result) {
  if (socPct >= 99.9) {
    if (!state.fullSocTimerActive) {
      state.fullSocTimerActive = true;
      state.fullSocSinceSeconds = nowSeconds;
    } else if (!state.calibrated &&
               nowSeconds >= state.fullSocSinceSeconds + 300) {
      state.calibrated = true;
      state.calibrationRequested = false;
      result.calibrationCompleted = true;
    }
  } else {
    state.fullSocTimerActive = false;
    state.fullSocSinceSeconds = 0;
  }
}

} // namespace

namespace energy_model {

double deficitKWh(double currentSocPct, double targetSocPct,
                  double capacityKWh) {
  return std::max(0.0, targetSocPct - currentSocPct) * capacityKWh / 100.0;
}

double requiredPowerW(double deficit, double hours, double efficiency,
                      double safetyMargin) {
  if (deficit <= 0.0) {
    return 0.0;
  }
  if (hours <= 0.0 || efficiency <= 0.0) {
    return INFINITY;
  }
  return deficit * safetyMargin * 1000.0 / (hours * efficiency);
}

double projectedSocPct(double socPct, double chargePowerW, double hours,
                       double capacityKWh, double efficiency) {
  if (capacityKWh <= 0.0 || hours <= 0.0) {
    return socPct;
  }
  const double storedKWh =
      std::max(0.0, chargePowerW) * hours * efficiency / 1000.0;
  return clamp(socPct + storedKWh / capacityKWh * 100.0, 0.0, 100.0);
}

} // namespace energy_model

namespace load_profile {

double estimatedLoadKW(const EMSConfig &config, uint8_t dayOfWeek) {
  return dayOfWeek < config.weekendDays.size() && config.weekendDays[dayOfWeek]
             ? config.weekendLoadKW
             : config.weekdayLoadKW;
}

} // namespace load_profile

namespace fog_detector {

bool update(const WeatherInput &weather, bool previousFog) {
  if (weather.stableFog.has_value()) {
    return *weather.stableFog;
  }
  if (!weather.temperatureC.value || !weather.dewPointC.value ||
      !weather.radiationWm2.value) {
    return previousFog;
  }
  const double spread = *weather.temperatureC.value - *weather.dewPointC.value;
  const double radiation = *weather.radiationWm2.value;
  if (!previousFog) {
    return spread <= 1.5 && radiation < 100.0;
  }
  const bool ratioRecovered = weather.actualToForecastRatio.value.has_value() &&
                              *weather.actualToForecastRatio.value >= 0.75;
  return !(spread >= 2.5 && radiation >= 180.0 && ratioRecovered);
}

} // namespace fog_detector

namespace seasonal_strategy {

bool isWinter(const EMSConfig &config, uint8_t month, uint8_t dayOfMonth) {
  const uint16_t value = static_cast<uint16_t>(month) * 100 + dayOfMonth;
  if (config.winterStartMonthDay <= config.winterEndMonthDay) {
    return value >= config.winterStartMonthDay &&
           value <= config.winterEndMonthDay;
  }
  return value >= config.winterStartMonthDay ||
         value <= config.winterEndMonthDay;
}

bool priorityCharge(const EMSConfig &config, const EMSInput &input,
                    double todaySurplusKWh, double deficitToFullKWh) {
  if (!isWinter(config, input.clock.month, input.clock.dayOfMonth) ||
      input.clock.minuteOfDay < config.seasonalEvaluationMinute) {
    return false;
  }
  const bool tomorrowFavorable =
      input.forecast.available &&
      input.forecast.tomorrowKWh >= config.tomorrowFavorableKWh &&
      input.socPct.value.has_value() &&
      *input.socPct.value >= config.tomorrowMinimumSocPct;
  return todaySurplusKWh < deficitToFullKWh * 1.1 && !tomorrowFavorable;
}

} // namespace seasonal_strategy

namespace setpoint_shaper {

double shape(double requestedW, double previousW, const EMSConfig &config,
             bool critical) {
  double result = clamp(requestedW, -config.maximumDischargePowerW,
                        config.maximumChargePowerW);
  if (!config.allowDischarge && result < 0.0) {
    result = 0.0;
  }
  if (std::abs(result) < config.deadbandW) {
    result = 0.0;
  }
  if (std::abs(result - previousW) < config.hysteresisW) {
    return previousW;
  }
  if (!critical) {
    const double delta = result - previousW;
    result = previousW +
             clamp(delta, -config.rampDownWPerCycle, config.rampUpWPerCycle);
  }
  return result;
}

} // namespace setpoint_shaper

TargetAwareEMS::TargetAwareEMS(EMSConfig config) : config_(std::move(config)) {}

Evaluation TargetAwareEMS::evaluate(const EMSInput &input,
                                    const EMSState &state,
                                    uint64_t nowSeconds) const {
  Evaluation evaluation;
  evaluation.nextState = state;
  Recommendation &result = evaluation.recommendation;

  if (!coreDataValid(input, config_)) {
    result.batteryPowerW = 0.0;
    result.reason = RecommendationReason::DataInvalid;
    result.explanation = "Required local telemetry is missing, stale, or "
                         "invalid; safe idle recommended";
    result.confidence = 0.0;
    evaluation.nextState.previousRecommendationW = 0.0;
    return evaluation;
  }

  const double soc = *input.socPct.value;
  updateCalibration(soc, nowSeconds, evaluation.nextState, result);
  evaluation.nextState.fogActive =
      fog_detector::update(input.weather, state.fogActive);
  result.fogActive = evaluation.nextState.fogActive;

  const bool forecastValid =
      input.forecast.available &&
      input.forecast.ageSeconds <= config_.forecastMaximumAgeSeconds;
  result.conservativeMode = !forecastValid;
  const double forecastKWh =
      correctedForecastKWh(input, config_, result.fogActive);
  const double remainingHours =
      input.clock.minuteOfDay < config_.targetMinuteOfDay
          ? (config_.targetMinuteOfDay - input.clock.minuteOfDay) / 60.0
          : 0.0;
  const double loadKWh =
      load_profile::estimatedLoadKW(config_, input.clock.dayOfWeek) *
      remainingHours;
  const double surplusKWh = std::max(0.0, forecastKWh - loadKWh);

  double targetSoc =
      input.overrideInput.targetSocPct.value_or(config_.targetSocPct);
  targetSoc = clamp(targetSoc, config_.minimumSocPct, 100.0);
  if (!evaluation.nextState.calibrationRequested) {
    targetSoc = std::min(targetSoc, config_.longevityMaximumSocPct);
  } else {
    targetSoc = 100.0;
  }
  result.targetSocPct = targetSoc;

  result.energy.deficitKWh =
      energy_model::deficitKWh(soc, targetSoc, config_.batteryCapacityKWh);
  result.energy.requiredChargePowerW = energy_model::requiredPowerW(
      result.energy.deficitKWh, remainingHours, config_.chargeEfficiency,
      config_.safetyMargin);
  result.energy.projectedSocPct = energy_model::projectedSocPct(
      soc, std::max(0.0, *input.batteryPowerW.value), remainingHours,
      config_.batteryCapacityKWh, config_.chargeEfficiency);
  result.energy.delayed =
      result.energy.requiredChargePowerW > 0.0 &&
      *input.batteryPowerW.value < result.energy.requiredChargePowerW * 0.85;
  result.energy.critical =
      targetSoc - result.energy.projectedSocPct >= 8.0 ||
      result.energy.requiredChargePowerW >= config_.maximumChargePowerW * 0.9;

  const double deficitToFull =
      energy_model::deficitKWh(soc, 100.0, config_.batteryCapacityKWh);
  evaluation.nextState.priorityChargeActive = seasonal_strategy::priorityCharge(
      config_, input, surplusKWh, deficitToFull);
  result.priorityChargeActive = evaluation.nextState.priorityChargeActive;

  const double availableInputKWh =
      forecastValid ? surplusKWh
                    : config_.maximumChargePowerW * remainingHours / 1000.0;
  const double maximumInputKWh =
      config_.maximumChargePowerW * remainingHours / 1000.0;
  const double requiredInputKWh = result.energy.deficitKWh *
                                  config_.safetyMargin /
                                  config_.chargeEfficiency;
  const double feasibleInputKWh = std::min(availableInputKWh, maximumInputKWh);
  result.feasibility = remainingHours <= 0.0 && result.energy.deficitKWh > 0.0
                           ? Feasibility::NotFeasible
                           : (feasibleInputKWh + 1e-9 >= requiredInputKWh
                                  ? Feasibility::Feasible
                                  : Feasibility::NotFeasible);

  double requestedW = 0.0;
  const bool night =
      *input.pvPowerW.value < config_.deadbandW && forecastKWh <= 0.0;
  const bool inRecovery = input.clock.minuteOfDay < config_.targetMinuteOfDay &&
                          input.clock.minuteOfDay + config_.recoveryMinutes >=
                              config_.targetMinuteOfDay;

  if (input.overrideInput.requestedBatteryPowerW.has_value()) {
    requestedW = *input.overrideInput.requestedBatteryPowerW;
    result.reason = RecommendationReason::Override;
    result.explanation =
        "Typed override recommendation, constrained by safety limits";
  } else if (result.energy.deficitKWh <= 0.0) {
    result.reason = RecommendationReason::TargetSatisfied;
    result.explanation = "SOC target already satisfied";
  } else if (night && !inRecovery) {
    result.reason = RecommendationReason::NightIdle;
    result.explanation =
        "No local or forecast PV before the target; idle recommendation";
  } else if (inRecovery || evaluation.nextState.priorityChargeActive ||
             !forecastValid) {
    requestedW = config_.maximumChargePowerW;
    result.reason =
        inRecovery ? RecommendationReason::RecoveryCharge
                   : (!forecastValid ? RecommendationReason::ForecastUnavailable
                                     : RecommendationReason::TargetCharge);
    result.explanation = "Conservative or priority charging recommendation";
  } else {
    requestedW = std::min(config_.maximumChargePowerW,
                          result.energy.requiredChargePowerW);
    result.reason = result.feasibility == Feasibility::NotFeasible
                        ? RecommendationReason::TargetNotFeasible
                        : RecommendationReason::TargetCharge;
    result.explanation =
        result.feasibility == Feasibility::NotFeasible
            ? "Target is not feasible with forecast energy and configured "
              "limits"
            : "Minimum average charge compatible with the target";
  }

  result.batteryPowerW =
      setpoint_shaper::shape(requestedW, state.previousRecommendationW, config_,
                             result.energy.critical);
  evaluation.nextState.previousRecommendationW = result.batteryPowerW;
  result.confidence =
      forecastValid ? (input.socPct.quality == DataQuality::Valid ? 1.0 : 0.7)
                    : 0.5;
  return evaluation;
}

} // namespace heliograph::ems::target
