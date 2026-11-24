// SPDX-License-Identifier: MIT
//
// Afore Autonomous EMS Constraint Enforcement & Intent Arbitration Pipeline
// (Milestone M3).

#include "constraint_pipeline.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace heliograph {
namespace ems {

EmsDecisionRecord ConstraintPipeline::applyConstraints(
    int32_t rawIntentW, const TelemetryInput &telemetry,
    int32_t currentActualPowerW, StrategyMode strategy,
    AuthoritySource authority, uint64_t sequenceId) const {

  EmsDecisionRecord record;
  record.sequenceId = sequenceId;
  record.timestampMs = telemetry.timestampMs;
  record.strategy = strategy;
  record.authority = authority;
  record.proposedPowerW = rawIntentW;
  record.actuatedPowerW = 0; // Remains 0 in shadow / non-actuating mode

  int32_t power = rawIntentW;
  uint32_t flags = ConstraintNone;

  // 1. Invalidation fallback: if telemetry invalid, command 0 W idle
  if (!telemetry.batteryBmsValid || !telemetry.gridMeterValid) {
    record.authorizedPowerW = 0;
    record.appliedConstraintsMask = ConstraintNone;
    record.strategy = StrategyMode::SafeIdle;
    std::snprintf(record.justification, sizeof(record.justification),
                  "Telemetry invalid or unverified - safe idle hold");
    return record;
  }

  // 2. Battery SOC floor reserve constraint (prevent over-discharge)
  if (power < 0 && telemetry.batterySocPct <= constraints_.socReserveFloorPct) {
    power = 0;
    flags |= ConstraintSocReserveFloor;
  }

  // 3. Battery SOC ceiling constraint (prevent overcharge beyond 100%)
  if (power > 0 && telemetry.batterySocPct >= 100.0f) {
    power = 0;
    flags |= ConstraintSocMaxCeiling;
  }

  // 4. BMS Hardware charge/discharge capability limits
  if (power > constraints_.maxChargePowerW) {
    power = constraints_.maxChargePowerW;
    flags |= ConstraintBmsChargeLimit;
  } else if (power < -constraints_.maxDischargePowerW) {
    power = -constraints_.maxDischargePowerW;
    flags |= ConstraintBmsDischargeLimit;
  }

  // 5. Grid import restriction
  if (power > 0 && !constraints_.allowGridCharging) {
    // Only solar PV surplus can be used to charge without drawing from the
    // grid. If solar PV is actively reported:
    if (telemetry.pvPowerW > 0) {
      int32_t maxSolarSurplusW =
          std::max<int32_t>(0, telemetry.pvPowerW - telemetry.loadPowerW);
      if (power > maxSolarSurplusW) {
        power = maxSolarSurplusW;
        flags |= ConstraintGridImportDisallowed;
      }
    } else if (telemetry.gridPowerW > 0) {
      // If PV is 0 and grid is already importing, charging from grid is
      // completely blocked
      power = 0;
      flags |= ConstraintGridImportDisallowed;
    }
  }

  // 6. Inverter apparent power rating (Total AC/DC throughput)
  if (std::abs(power) > constraints_.inverterMaxVa) {
    power =
        (power > 0) ? constraints_.inverterMaxVa : -constraints_.inverterMaxVa;
    flags |= ConstraintInverterApparentVa;
  }

  // 7. Slew rate / Ramp rate limiting
  if (constraints_.maxRampRateWPerS > 0) {
    int32_t delta = power - currentActualPowerW;
    if (delta > constraints_.maxRampRateWPerS) {
      power = currentActualPowerW + constraints_.maxRampRateWPerS;
      flags |= ConstraintRampRateSlew;
    } else if (delta < -constraints_.maxRampRateWPerS) {
      power = currentActualPowerW - constraints_.maxRampRateWPerS;
      flags |= ConstraintRampRateSlew;
    }
  }

  // 8. Deadband filter around previous setpoint
  if (constraints_.deadbandW > 0 &&
      std::abs(power - currentActualPowerW) < constraints_.deadbandW) {
    power = currentActualPowerW;
    flags |= ConstraintDeadbandJitter;
  }

  record.authorizedPowerW = power;
  record.appliedConstraintsMask = flags;

  // Build explanatory justification
  if (flags == ConstraintNone) {
    if (power > 0) {
      std::snprintf(record.justification, sizeof(record.justification),
                    "Charging authorized at %d W (unconstrained)", power);
    } else if (power < 0) {
      std::snprintf(record.justification, sizeof(record.justification),
                    "Discharging authorized at %d W (unconstrained)", -power);
    } else {
      std::snprintf(record.justification, sizeof(record.justification),
                    "Net balance satisfied at 0 W idle");
    }
  } else {
    std::snprintf(record.justification, sizeof(record.justification),
                  "Setpoint constrained from %d W to %d W (flags: 0x%02X)",
                  rawIntentW, power, flags);
  }

  return record;
}

} // namespace ems
} // namespace heliograph
