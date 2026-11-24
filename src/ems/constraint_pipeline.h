// SPDX-License-Identifier: MIT
//
// Afore Autonomous EMS Constraint Enforcement & Intent Arbitration Pipeline
// (Milestone M3).

#pragma once

#include "ems_core_contracts.h"

namespace heliograph {
namespace ems {

class ConstraintPipeline {
public:
  explicit ConstraintPipeline(TechnicalConstraints constraints = {})
      : constraints_(constraints) {}

  const TechnicalConstraints &constraints() const { return constraints_; }
  void updateConstraints(const TechnicalConstraints &c) { constraints_ = c; }

  /// Evaluates raw intent against physical and safety constraints, returning an
  /// explainable decision.
  /// \param rawIntentW The proposed target power (W): >0 charge, <0 discharge.
  /// \param telemetry Current telemetry snapshot.
  /// \param currentActualPowerW Previous setpoint or measured actual battery
  /// power (used for slew rate).
  /// \param strategy Active strategy mode.
  /// \param authority Command authority source.
  /// \param sequenceId Monotonic evaluation counter.
  EmsDecisionRecord
  applyConstraints(int32_t rawIntentW, const TelemetryInput &telemetry,
                   int32_t currentActualPowerW, StrategyMode strategy,
                   AuthoritySource authority, uint64_t sequenceId) const;

private:
  TechnicalConstraints constraints_;
};

} // namespace ems
} // namespace heliograph
