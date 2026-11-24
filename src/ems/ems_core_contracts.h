// SPDX-License-Identifier: MIT
//
// Afore Autonomous EMS Common Core Domain Contracts & Serialization (Milestone
// M3).
//
// Pure C++17 value types, zero dynamic allocation on the fast 1 Hz cycle,
// explicit physical sign conventions, constraint bitmasks, and JSON
// serialization.

#pragma once

#include <cstdint>
#include <cstring>
#include <string>

namespace heliograph {
namespace ems {

/// Operating strategy mode for the EMS controller.
enum class StrategyMode : uint8_t {
  SelfUse = 0,        ///< Standard closed-loop balancing to 0 W grid exchange
  ChargeFirst = 1,    ///< Prioritize charging battery from solar/grid
  SolarOptimized = 2, ///< Defer early morning charge for solar peak window
  TouTariff = 3,      // Dynamic tariff arbitrage
  ScheduledMaintenance = 4, // BMS cell balancing and full SOC recalibration
  SafeIdle = 5              // Safe idle fallback (telemetry invalid/offline)
};

const char *strategyModeToString(StrategyMode mode);
StrategyMode strategyModeFromString(const char *str);

/// Authority responsible for setpoint or override command.
enum class AuthoritySource : uint8_t {
  LocalAutonomous = 0,
  ScheduledMaintenance = 1,
  MqttRemoteOverride = 2,
  RestApiOverride = 3,
  SafetyEmergency = 4
};

const char *authoritySourceToString(AuthoritySource auth);

/// Bitmask of physical constraints applied to curtail or shape the proposed
/// setpoint.
enum ConstraintFlag : uint32_t {
  ConstraintNone = 0,
  ConstraintSocReserveFloor = 1
      << 0, ///< Discharging blocked due to SOC <= reserve floor
  ConstraintSocMaxCeiling = 1 << 1, ///< Charging blocked due to SOC >= 100%
  ConstraintBmsChargeLimit = 1
      << 2, ///< Clamped to BMS max charge current/power
  ConstraintBmsDischargeLimit = 1
      << 3, ///< Clamped to BMS max discharge current/power
  ConstraintGridImportDisallowed = 1
      << 4, ///< Grid charging disallowed by user config
  ConstraintInverterApparentVa = 1
      << 5, ///< Total throughput clamped to inverter rating
  ConstraintRampRateSlew = 1 << 6, ///< Clipped by maximum slew rate (W/s)
  ConstraintDeadbandJitter = 1
      << 7 ///< Minor setpoint deviation suppressed by deadband
};

/// High-speed telemetry snapshot passed to the decision pipeline every cycle.
struct TelemetryInput {
  uint64_t timestampMs{0};
  int32_t gridPowerW{0};     ///< > 0 import from grid, < 0 export to grid
  int32_t batteryPowerW{0};  ///< > 0 charging into battery, < 0 discharging
  int32_t pvPowerW{0};       ///< >= 0 solar PV generation
  int32_t loadPowerW{0};     ///< >= 0 domestic house consumption
  float batterySocPct{0.0f}; ///< 0.0 to 100.0%
  float batteryVoltageV{0.0f};
  float batteryCurrentA{0.0f};
  bool gridMeterValid{false};
  bool batteryBmsValid{false};
};

/// Dynamic hardware boundaries and technical safety ratings.
struct TechnicalConstraints {
  int32_t maxChargePowerW{3000};
  int32_t maxDischargePowerW{3000};
  int32_t inverterMaxVa{5000};
  float socReserveFloorPct{15.0f};
  float socCriticalFloorPct{10.0f};
  int32_t maxRampRateWPerS{250};
  int32_t deadbandW{20};
  bool allowGridCharging{false};
};

/// Explainable decision record output at each evaluation cycle (1 Hz).
struct EmsDecisionRecord {
  uint64_t sequenceId{0};
  uint64_t timestampMs{0};
  StrategyMode strategy{StrategyMode::SelfUse};
  AuthoritySource authority{AuthoritySource::LocalAutonomous};
  int32_t proposedPowerW{0}; ///< Raw intent prior to constraints
  int32_t authorizedPowerW{
      0};                    ///< Final setpoint after hard constraint pipeline
  int32_t actuatedPowerW{0}; ///< Actually dispatched (0 in shadow mode)
  uint32_t appliedConstraintsMask{0};
  char justification[96]{""};

  /// Checks if a specific constraint was triggered during evaluation.
  bool hasConstraint(ConstraintFlag flag) const {
    return (appliedConstraintsMask & static_cast<uint32_t>(flag)) != 0;
  }

  /// Serializes decision record to JSON buffer without heap allocations.
  /// Returns number of bytes written.
  size_t toJson(char *buffer, size_t capacity) const;
};

} // namespace ems
} // namespace heliograph
