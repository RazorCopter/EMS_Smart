// SPDX-License-Identifier: MIT
//
// Shadow EMS Type Definitions (CP4).
//
// Data structures for Shadow EMS controller: constraints, decisions, recommendations,
// and divergence tracking. Shadow mode computes control strategies without actuating.

#pragma once

#include <cstdint>
#include <string>

namespace heliograph {
namespace ems {

/// Operating mode for Shadow EMS controller.
enum class OperatingMode : uint8_t {
    SafeIdle = 0,        ///< Fail-safe conservative mode (poor telemetry, startup, error)
    PassThrough = 1,     ///< Observing only, no recommendation (normal inverter operation)
    Charging = 2,        ///< Recommend charging battery from surplus solar
    Discharging = 3,     ///< Recommend discharging battery to offset grid import
    Hold = 4,            ///< Recommend holding battery state (balanced)
    SolarClipping = 5,   ///< Battery full, solar clipping detected
};

const char* operatingModeToString(OperatingMode mode);

/// Reason code for EMS decision.
enum class DecisionReason : uint8_t {
    Unknown = 0,
    TelemetryInvalid = 1,           ///< Telemetry quality too poor (Stale/Invalid)
    TelemetryDegraded = 2,          ///< Telemetry degraded, conservative action
    SurplusSolarAvailable = 3,      ///< Grid export detected, charge battery
    DeficitGridImport = 4,          ///< Grid import detected, discharge battery
    SOCLimitReached = 5,            ///< Battery SOC at min/max constraint
    PowerLimitReached = 6,          ///< Battery/inverter power limit reached
    BalancedOperation = 7,          ///< Grid near zero, battery held
    SolarClipping = 8,              ///< Battery full, cannot absorb more solar
    InitialState = 9,               ///< No decision made yet (startup)
};

const char* decisionReasonToString(DecisionReason reason);

/// Technical constraints for battery control.
struct BatteryConstraints {
    double minSOC = 15.0;            ///< Minimum SOC % (reserve)
    double maxSOC = 95.0;            ///< Maximum SOC % (protection)
    double maxChargePowerW = 5000.0; ///< Max charge power (inverter/BMS limit)
    double maxDischargePowerW = 5000.0; ///< Max discharge power
    double ratedCapacityKWh = 10.24; ///< Total battery capacity (for projections)
    
    /// Check if SOC is within operational range.
    bool socInRange(double soc) const {
        return soc >= minSOC && soc <= maxSOC;
    }
    
    /// Clamp power to allowed range (positive = charge, negative = discharge).
    double clampPower(double requestedW) const {
        if (requestedW > 0) {
            return requestedW > maxChargePowerW ? maxChargePowerW : requestedW;
        } else {
            return requestedW < -maxDischargePowerW ? -maxDischargePowerW : requestedW;
        }
    }
};

/// Shadow EMS control recommendation.
///
/// Represents what the EMS *would* command if active, without actuating.
/// Enables comparison with actual inverter behavior for validation and tuning.
struct ControlRecommendation {
    OperatingMode mode = OperatingMode::SafeIdle;
    DecisionReason reason = DecisionReason::InitialState;
    
    // Power setpoints (canonical convention: positive = charge, negative = discharge)
    double recommendedBatteryPowerW = 0.0;  ///< Requested battery power
    double allowedBatteryPowerW = 0.0;      ///< After constraints applied
    double chargeLimitW = 0.0;              ///< Max charge rate allowed now
    double dischargeLimitW = 0.0;           ///< Max discharge rate allowed now
    
    uint64_t timestampMs = 0;               ///< Decision timestamp
    double confidenceScore = 0.0;           ///< [0..1] confidence in recommendation
};

/// Divergence metrics between Shadow EMS and actual inverter behavior.
///
/// Tracks differences to evaluate potential gains/risks of active control.
struct DivergenceMetrics {
    // Instantaneous divergence
    double divergencePowerW = 0.0;          ///< Difference: recommended - actual battery power
    
    // Cumulative tracking
    double cumulativeDivergenceKWh = 0.0;   ///< Integrated divergence over session
    uint32_t divergenceEvents = 0;          ///< Count of significant divergences (>100W for >30s)
    
    // Constraint violations observed
    uint32_t socViolations = 0;             ///< Times SOC hit min/max unexpectedly
    uint32_t powerViolations = 0;           ///< Times power exceeded safe limits
    
    // Timestamps
    uint64_t lastUpdateMs = 0;
    uint64_t sessionStartMs = 0;
    
    /// Reset cumulative metrics (start new evaluation session).
    void reset(uint64_t nowMs) {
        cumulativeDivergenceKWh = 0.0;
        divergenceEvents = 0;
        socViolations = 0;
        powerViolations = 0;
        sessionStartMs = nowMs;
        lastUpdateMs = nowMs;
    }
};

/// Complete Shadow EMS state snapshot.
struct ShadowEMSState {
    ControlRecommendation recommendation;
    DivergenceMetrics divergence;
    BatteryConstraints constraints;
    
    // Quality summary from telemetry
    bool telemetryActionable = false;       ///< All critical measurements Good/Degraded
    uint32_t failsafeActivations = 0;       ///< Count of transitions to SafeIdle
    
    uint64_t uptimeMs = 0;                  ///< Shadow EMS uptime
};

}  // namespace ems
}  // namespace heliograph
