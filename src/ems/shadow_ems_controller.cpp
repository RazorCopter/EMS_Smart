// SPDX-License-Identifier: MIT
//
// Shadow EMS Controller Implementation (CP4).

#include "shadow_ems_controller.h"

#include <algorithm>
#include <cmath>

namespace heliograph {
namespace ems {

ShadowEMSController::ShadowEMSController(const ShadowEMSConfig &config)
    : config_(config) {
  state_.constraints = config.constraints;
  state_.divergence.sessionStartMs = 0;
  state_.uptimeMs = 0;
}

bool ShadowEMSController::update(
    const std::map<std::string, TelemetrySample> &samples, uint64_t nowMs) {

  // Initialize session timing on first call. Keep lastUpdateMs_ at zero until
  // after computeRecommendation(), so applySmoothing() can recognize the first
  // update.
  if (lastUpdateMs_ == 0) {
    state_.divergence.sessionStartMs = nowMs;
  }

  state_.uptimeMs = nowMs - state_.divergence.sessionStartMs;

  // Store previous recommendation to detect changes
  OperatingMode previousMode = state_.recommendation.mode;

  // Check telemetry quality first (fail-safe gate)
  state_.telemetryActionable = checkTelemetryQuality(samples);

  if (!state_.telemetryActionable) {
    transitionToFailSafe(DecisionReason::TelemetryInvalid, nowMs);
    lastUpdateMs_ = nowMs;
    return previousMode != state_.recommendation.mode;
  }

  // Compute new recommendation
  computeRecommendation(samples, nowMs);

  // Extract actual battery power for divergence tracking
  auto it = samples.find("battery.power");
  if (it != samples.end() && it->second.actionable()) {
    updateDivergence(it->second.value, nowMs);
  }

  lastUpdateMs_ = nowMs;
  return previousMode != state_.recommendation.mode;
}

bool ShadowEMSController::checkTelemetryQuality(
    const std::map<std::string, TelemetrySample> &samples) {

  // Critical measurements for control decisions
  const char *criticalMeasurements[] = {"battery.soc", "battery.power",
                                        "grid.power", "pv.power_total"};

  for (const char *key : criticalMeasurements) {
    auto it = samples.find(key);
    if (it == samples.end() || !it->second.actionable()) {
      return false; // Missing or non-actionable critical data
    }
  }

  return true;
}

void ShadowEMSController::transitionToFailSafe(DecisionReason reason,
                                               uint64_t nowMs) {
  state_.recommendation.mode = OperatingMode::SafeIdle;
  state_.recommendation.reason = reason;
  state_.recommendation.recommendedBatteryPowerW = 0.0;
  state_.recommendation.allowedBatteryPowerW = 0.0;
  state_.recommendation.chargeLimitW = 0.0;
  state_.recommendation.dischargeLimitW = 0.0;
  state_.recommendation.timestampMs = nowMs;
  state_.recommendation.confidenceScore = 0.0;
  state_.failsafeActivations++;
}

void ShadowEMSController::computeRecommendation(
    const std::map<std::string, TelemetrySample> &samples, uint64_t nowMs) {

  // Extract key measurements (already validated by checkTelemetryQuality)
  double pvPowerW = samples.at("pv.power_total").value;
  double gridPowerW = samples.at("grid.power").value;
  double batteryPowerW = samples.at("battery.power").value;
  double socPct = samples.at("battery.soc").value;

  // Apply self-consumption strategy
  applySelfConsumptionStrategy(pvPowerW, gridPowerW, batteryPowerW, socPct,
                               nowMs);

  // Set confidence based on telemetry quality
  double minConfidence = 1.0;
  for (const auto &pair : samples) {
    if (pair.second.quality == Quality::Degraded) {
      minConfidence = std::min(minConfidence, 0.7);
    }
  }
  state_.recommendation.confidenceScore = minConfidence;
}

void ShadowEMSController::applySelfConsumptionStrategy(double pvPowerW,
                                                       double gridPowerW,
                                                       double batteryPowerW,
                                                       double socPct,
                                                       uint64_t nowMs) {
  (void)pvPowerW;      // Reserved for future solar-aware strategies
  (void)batteryPowerW; // Reserved for future battery-state strategies

  // Grid power convention: positive = import, negative = export
  double requestedPowerW = 0.0;
  OperatingMode mode = OperatingMode::Hold;
  DecisionReason reason = DecisionReason::BalancedOperation;

  // Check SOC boundaries first
  if (socPct >= state_.constraints.maxSOC) {
    // Battery full: cannot charge more
    if (gridPowerW < -config_.gridExportThresholdW) {
      // Export happening with full battery = solar clipping
      mode = OperatingMode::SolarClipping;
      reason = DecisionReason::SolarClipping;
    } else {
      mode = OperatingMode::Hold;
      reason = DecisionReason::SOCLimitReached;
    }
    requestedPowerW = 0.0;

  } else if (socPct <= state_.constraints.minSOC) {
    // Battery at reserve: cannot discharge more
    mode = OperatingMode::Hold;
    reason = DecisionReason::SOCLimitReached;
    requestedPowerW = 0.0;

  } else if (gridPowerW <
             -(config_.gridExportThresholdW + config_.hysteresisW)) {
    // Surplus solar: export detected, charge battery to absorb
    double excessW =
        -gridPowerW;           // Convert export (negative) to positive excess
    requestedPowerW = excessW; // Charge to eliminate export
    mode = OperatingMode::Charging;
    reason = DecisionReason::SurplusSolarAvailable;

  } else if (gridPowerW >
             (config_.gridImportThresholdW + config_.hysteresisW)) {
    // Deficit: import detected, discharge battery to eliminate import
    double deficitW = gridPowerW;
    requestedPowerW = -deficitW; // Discharge (negative) to offset import
    mode = OperatingMode::Discharging;
    reason = DecisionReason::DeficitGridImport;

  } else {
    // Within hysteresis band: hold battery state (balanced)
    requestedPowerW = 0.0;
    mode = OperatingMode::Hold;
    reason = DecisionReason::BalancedOperation;
  }

  // Apply constraints (SOC, power limits)
  double allowedPowerW = applyConstraints(requestedPowerW, socPct, nowMs);

  // Apply smoothing/ramping
  double smoothedPowerW = applySmoothing(allowedPowerW, nowMs);

  // Compute charge/discharge limits for reporting
  double chargeLimitW = state_.constraints.maxChargePowerW;
  double dischargeLimitW = state_.constraints.maxDischargePowerW;

  if (socPct >= state_.constraints.maxSOC) {
    chargeLimitW = 0.0;
  }
  if (socPct <= state_.constraints.minSOC) {
    dischargeLimitW = 0.0;
  }

  // Update recommendation state
  state_.recommendation.mode = mode;
  state_.recommendation.reason = reason;
  state_.recommendation.recommendedBatteryPowerW = requestedPowerW;
  state_.recommendation.allowedBatteryPowerW = smoothedPowerW;
  state_.recommendation.chargeLimitW = chargeLimitW;
  state_.recommendation.dischargeLimitW = dischargeLimitW;
  state_.recommendation.timestampMs = nowMs;

  // Update smoothing state for next iteration
  lastRecommendedPowerW_ = smoothedPowerW;
}

double ShadowEMSController::applyConstraints(double requestedPowerW,
                                             double socPct, uint64_t nowMs) {
  (void)nowMs; // Reserved for future time-based constraint logic

  // Clamp to battery/inverter power limits
  double constrainedW = state_.constraints.clampPower(requestedPowerW);

  // Additional SOC-based limiting (soft approach near boundaries)
  if (socPct > state_.constraints.maxSOC - 5.0) {
    // Near max SOC: reduce charge aggressiveness
    double factor = (state_.constraints.maxSOC - socPct) / 5.0;
    if (constrainedW > 0) {
      constrainedW *= std::max(0.0, factor);
    }
  }

  if (socPct < state_.constraints.minSOC + 5.0) {
    // Near min SOC: reduce discharge aggressiveness
    double factor = (socPct - state_.constraints.minSOC) / 5.0;
    if (constrainedW < 0) {
      constrainedW *= std::max(0.0, factor);
    }
  }

  return constrainedW;
}

double ShadowEMSController::applySmoothing(double targetPowerW,
                                           uint64_t nowMs) {
  if (lastUpdateMs_ == 0) {
    return targetPowerW; // First update, no smoothing
  }

  double deltaTimeS = (nowMs - lastUpdateMs_) / 1000.0;
  if (deltaTimeS <= 0.0) {
    return lastRecommendedPowerW_; // No time elapsed
  }

  double maxChangeW = config_.maxPowerRampWPerS * deltaTimeS;
  double deltaW = targetPowerW - lastRecommendedPowerW_;

  if (std::abs(deltaW) <= maxChangeW) {
    return targetPowerW; // Within ramp limit
  }

  // Limit rate of change
  if (deltaW > 0) {
    return lastRecommendedPowerW_ + maxChangeW;
  } else {
    return lastRecommendedPowerW_ - maxChangeW;
  }
}

void ShadowEMSController::updateDivergence(double actualBatteryPowerW,
                                           uint64_t nowMs) {
  double recommendedW = state_.recommendation.allowedBatteryPowerW;
  double divergenceW = recommendedW - actualBatteryPowerW;

  state_.divergence.divergencePowerW = divergenceW;

  // Track cumulative divergence (integrate power difference over time)
  if (state_.divergence.lastUpdateMs > 0) {
    double deltaTimeH =
        (nowMs - state_.divergence.lastUpdateMs) / 3600000.0; // ms to hours
    double avgDivergenceW = (lastDivergencePowerW_ + divergenceW) / 2.0;
    state_.divergence.cumulativeDivergenceKWh +=
        (avgDivergenceW * deltaTimeH) / 1000.0;
  }

  // Detect significant divergence events (>threshold for >duration)
  if (std::abs(divergenceW) > config_.divergenceThresholdW) {
    if (!inDivergenceEvent_) {
      divergenceStartMs_ = nowMs;
      inDivergenceEvent_ = true;
    } else if (nowMs - divergenceStartMs_ >= config_.divergenceDurationMs) {
      // Sustained divergence: count as event (once per sustained period)
      state_.divergence.divergenceEvents++;
      divergenceStartMs_ = nowMs; // Reset for next event
    }
  } else {
    inDivergenceEvent_ = false;
  }

  // Check for SOC constraint violations
  // (Would need SOC sample here; simplified for now)

  state_.divergence.lastUpdateMs = nowMs;
  lastDivergencePowerW_ = divergenceW;
}

} // namespace ems
} // namespace heliograph
