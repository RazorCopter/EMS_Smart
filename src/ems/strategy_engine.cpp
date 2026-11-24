// SPDX-License-Identifier: MIT
//
// Afore Autonomous EMS High-Level Strategy Engine (Milestone M4).

#include "strategy_engine.h"
#include <algorithm>

namespace heliograph {
namespace ems {

int32_t
StrategyEngine::computeSelfUseIntent(const TelemetryInput &telemetry) const {
  // Self-Use objective: Grid exchange = targetGridExchangeW (default 0 W).
  // Sign conventions:
  // gridPowerW > 0: Importing from grid (deficit)
  // gridPowerW < 0: Exporting to grid (surplus)
  // batteryPowerW > 0: Battery charging
  // batteryPowerW < 0: Battery discharging
  //
  // Current domestic net balance:
  // Net Surplus = pvPowerW - loadPowerW
  // If Surplus > 0 (PV > Load), we want to charge the battery:
  // rawIntent = Surplus - targetGridExchangeW (> 0 -> Charge)
  // If Deficit (Load > PV, surplus < 0), we want to discharge the battery:
  // rawIntent = -(Load - PV) + targetGridExchangeW (< 0 -> Discharge)
  //
  // When loadPowerW is computed or measured:
  // Net balance = pvPowerW - loadPowerW.
  // Notice that if gridPowerW is measured at the meter:
  // Grid Import (gridPowerW > 0) means shortfall -> we should discharge:
  // -gridPowerW. Grid Export (gridPowerW < 0) means surplus -> we should
  // charge: -gridPowerW (> 0).
  //
  // Combining both:
  // Required adjustment from current battery power:
  // targetBatteryPower = currentActualPowerW_ - (gridPowerW -
  // config_.targetGridExchangeW);
  //
  // If battery is already running at currentActualPowerW, the grid sees:
  // grid = (Load - PV) - batteryDischarge (where batteryDischarge =
  // -batteryPower) To bring grid to target: Delta = gridPowerW -
  // targetGridExchangeW. New Battery Power = currentActualPowerW_ - Delta. (If
  // grid > 0 (importing 500W), New Battery Power = current - 500W (more
  // discharging / less charging)). (If grid < -1000W (exporting 1000W), New
  // Battery Power = current - (-1000) = current + 1000W (more charging)).
  return currentActualPowerW_ -
         (telemetry.gridPowerW - config_.targetGridExchangeW);
}

int32_t StrategyEngine::computeChargeFirstIntent(
    const TelemetryInput &telemetry) const {
  // If battery has already reached target SOC, transition to normal SelfUse
  // balance
  if (telemetry.batterySocPct >= config_.socChargeFirstTargetPct) {
    return computeSelfUseIntent(telemetry);
  }

  // In Charge-First, all available solar PV generation goes directly to
  // battery. In addition, if grid assist is permitted by config, charge with
  // grid assist.
  int32_t solarSurplus =
      std::max<int32_t>(0, telemetry.pvPowerW - telemetry.loadPowerW);
  int32_t desiredCharge = solarSurplus;

  if (config_.chargeFirstGridAssistW > 0) {
    desiredCharge += config_.chargeFirstGridAssistW;
  }

  // Must be positive (charging)
  return std::max<int32_t>(0, desiredCharge);
}

int32_t
StrategyEngine::computeMaintenanceIntent(const TelemetryInput &telemetry) {
  if (maintenanceCompleted_) {
    return 0;
  }

  if (maintenanceStartTimestampMs_ == 0) {
    maintenanceStartTimestampMs_ = telemetry.timestampMs;
  }

  uint64_t elapsedSec =
      (telemetry.timestampMs >= maintenanceStartTimestampMs_)
          ? (telemetry.timestampMs - maintenanceStartTimestampMs_) / 1000
          : 0;

  if (elapsedSec > config_.maintenanceTimeoutSec) {
    maintenanceCompleted_ = true;
    return 0;
  }

  if (telemetry.batterySocPct >= config_.maintenanceTargetSocPct) {
    maintenanceCompleted_ = true;
    return 0;
  }

  return config_.maintenancePowerW;
}

int32_t StrategyEngine::computeSolarOptimizedIntent(
    const TelemetryInput &telemetry) const {
  int32_t selfUseIntent = computeSelfUseIntent(telemetry);

  if (config_.allowSolarForecastCurtailmentPrevention &&
      config_.solarForecastExcessKwh > 5.0f &&
      telemetry.batterySocPct >= 50.0f && selfUseIntent > 0) {
    return selfUseIntent / 2;
  }

  return selfUseIntent;
}

int32_t
StrategyEngine::computeTouTariffIntent(const TelemetryInput &telemetry) const {
  if (config_.touLowTariffWindowActive &&
      telemetry.batterySocPct < config_.touTargetSocPct) {
    return config_.touScheduledChargePowerW;
  }

  return computeSelfUseIntent(telemetry);
}

EmsDecisionRecord StrategyEngine::evaluate(const TelemetryInput &telemetry,
                                           AuthoritySource authority) {
  ++sequenceCounter_;

  int32_t rawIntentW = 0;

  switch (config_.mode) {
  case StrategyMode::SelfUse:
    rawIntentW = computeSelfUseIntent(telemetry);
    break;

  case StrategyMode::ChargeFirst:
    rawIntentW = computeChargeFirstIntent(telemetry);
    break;

  case StrategyMode::SolarOptimized:
    rawIntentW = computeSolarOptimizedIntent(telemetry);
    break;

  case StrategyMode::TouTariff:
    rawIntentW = computeTouTariffIntent(telemetry);
    break;

  case StrategyMode::ScheduledMaintenance:
    rawIntentW = computeMaintenanceIntent(telemetry);
    break;

  case StrategyMode::SafeIdle:
  default:
    rawIntentW = 0;
    break;
  }

  // Apply technical constraint pipeline (SOC floor/ceiling, BMS rates, ramp
  // slew, deadband)
  EmsDecisionRecord decision =
      pipeline_.applyConstraints(rawIntentW, telemetry, currentActualPowerW_,
                                 config_.mode, authority, sequenceCounter_);

  // Update tracking power with authorized setpoint
  currentActualPowerW_ = decision.authorizedPowerW;

  return decision;
}

} // namespace ems
} // namespace heliograph
