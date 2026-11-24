// SPDX-License-Identifier: MIT
//
// Afore Autonomous EMS High-Level Strategy Engine (Milestone M4).
// Implements Self-Use, Charge-First, SOC reserve floors, hysteresis, and
// battery dispatch logic.

#pragma once

#include "constraint_pipeline.h"
#include "ems_core_contracts.h"

namespace heliograph {
namespace ems {

struct StrategyConfig {
  StrategyMode mode{StrategyMode::SelfUse};
  float socReserveFloorPct{
      15.0f}; ///< Self-use will not discharge below this SOC
  float socChargeFirstTargetPct{
      90.0f}; ///< Charge-first mode priority charging up to this SOC
  int32_t chargeFirstGridAssistW{
      1000}; ///< Additional grid charging assist for Charge-First
  int32_t targetGridExchangeW{
      0}; ///< Target grid exchange for Self-Use (default 0 W)

  // Milestone M5: Target SOC programmato e Maintenance
  float maintenanceTargetSocPct{
      100.0f}; ///< Maintenance calibration target SOC (typically 100%)
  int32_t maintenancePowerW{1500};       ///< Maintenance charge power
  uint64_t maintenanceTimeoutSec{14400}; ///< Safety abort timeout (4h default)

  // Milestone M6-M7: Solar Forecast & TOU Tariff Optimization
  bool allowSolarForecastCurtailmentPrevention{true};
  float solarForecastExcessKwh{0.0f};
  bool touLowTariffWindowActive{false};
  int32_t touScheduledChargePowerW{2000};
  float touTargetSocPct{85.0f};
};

class StrategyEngine {
public:
  StrategyEngine(StrategyConfig config, TechnicalConstraints constraints)
      : config_(config), pipeline_(constraints), currentActualPowerW_(0),
        sequenceCounter_(0) {
    auto tc = pipeline_.constraints();
    tc.socReserveFloorPct = config_.socReserveFloorPct;
    pipeline_.updateConstraints(tc);
  }

  const StrategyConfig &config() const { return config_; }
  void updateConfig(const StrategyConfig &cfg) {
    config_ = cfg;
    auto tc = pipeline_.constraints();
    tc.socReserveFloorPct = cfg.socReserveFloorPct;
    pipeline_.updateConstraints(tc);
  }

  const ConstraintPipeline &pipeline() const { return pipeline_; }
  const TechnicalConstraints &constraints() const {
    return pipeline_.constraints();
  }
  void updateConstraints(const TechnicalConstraints &tc) {
    pipeline_.updateConstraints(tc);
  }

  /// Sets the known previous power setpoint or measured actual battery power.
  void setCurrentPower(int32_t powerW) { currentActualPowerW_ = powerW; }
  int32_t currentPower() const { return currentActualPowerW_; }

  /// Executes one evaluation cycle (1 Hz) on the incoming telemetry snapshot.
  EmsDecisionRecord
  evaluate(const TelemetryInput &telemetry,
           AuthoritySource authority = AuthoritySource::LocalAutonomous);

private:
  StrategyConfig config_;
  ConstraintPipeline pipeline_;
  int32_t currentActualPowerW_{0};
  uint64_t sequenceCounter_{0};

  uint64_t maintenanceStartTimestampMs_{0};
  bool maintenanceCompleted_{false};

  int32_t computeSelfUseIntent(const TelemetryInput &telemetry) const;
  int32_t computeChargeFirstIntent(const TelemetryInput &telemetry) const;
  int32_t computeMaintenanceIntent(const TelemetryInput &telemetry);
  int32_t computeSolarOptimizedIntent(const TelemetryInput &telemetry) const;
  int32_t computeTouTariffIntent(const TelemetryInput &telemetry) const;
};

} // namespace ems
} // namespace heliograph
