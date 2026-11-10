// SPDX-License-Identifier: MIT
//
// Shadow EMS Controller (CP4).
//
// Implements self-consumption optimization strategy without actuating inverter.
// Computes control recommendations based on telemetry, tracks divergence from
// actual inverter behavior, and provides fail-safe transitions on poor data quality.
//
// Core strategy (Self-Consumption Optimization):
//   1. Grid Export (surplus solar): Recommend charging battery to absorb excess
//   2. Grid Import (deficit): Recommend discharging battery to eliminate import
//   3. Balanced: Hold battery state
//   4. Quality fail: Transition to SafeIdle conservatively
//
// Phase 0 Shadow Mode: strictly no write operations to inverter.

#pragma once

#include <map>
#include <string>

#include "device/telemetry_quality.h"
#include "shadow_ems_types.h"

namespace heliograph {
namespace ems {

/// Shadow EMS controller configuration.
struct ShadowEMSConfig {
    // Self-consumption strategy tuning
    double gridExportThresholdW = 50.0;     ///< Surplus above this triggers charge
    double gridImportThresholdW = 50.0;     ///< Deficit above this triggers discharge
    double hysteresisW = 30.0;              ///< Hysteresis band around zero to prevent oscillation
    
    // Power control smoothing
    double maxPowerRampWPerS = 500.0;       ///< Max power change rate (W/s) for smoothing
    
    // Divergence tracking
    double divergenceThresholdW = 100.0;    ///< Significant divergence threshold
    uint32_t divergenceDurationMs = 30000;  ///< Duration for counting divergence event (30s)
    
    // Constraints (can be overridden at runtime)
    BatteryConstraints constraints;
};

/// Shadow EMS controller for Phase 0 validation.
///
/// Usage:
///   ShadowEMSController controller(config);
///   
///   loop:
///     auto samples = telemetryEngine.getTelemetrySamples();
///     controller.update(samples, nowMs);
///     
///     auto state = controller.getState();
///     // Publish state.recommendation, state.divergence to MQTT
///     // Log for comparison with actual inverter behavior
class ShadowEMSController {
public:
    explicit ShadowEMSController(const ShadowEMSConfig& config = ShadowEMSConfig{});
    
    /// Updates EMS logic with current telemetry samples.
    ///
    /// Computes control recommendation based on:
    ///   - Telemetry quality (fail-safe if Stale/Invalid)
    ///   - Grid power (export → charge, import → discharge)
    ///   - Battery SOC limits
    ///   - Battery power limits
    ///
    /// Updates divergence metrics by comparing recommendation to actual battery power.
    ///
    /// Returns: true if recommendation changed (new decision)
    bool update(const std::map<std::string, TelemetrySample>& samples, uint64_t nowMs);
    
    /// Retrieves current Shadow EMS state (recommendation, divergence, constraints).
    const ShadowEMSState& getState() const { return state_; }
    
    /// Updates battery constraints (SOC limits, power limits, capacity).
    void setConstraints(const BatteryConstraints& constraints) {
        state_.constraints = constraints;
    }
    
    /// Resets divergence metrics (start new evaluation session).
    void resetDivergenceMetrics(uint64_t nowMs) {
        state_.divergence.reset(nowMs);
    }
    
    /// Configuration accessor.
    const ShadowEMSConfig& getConfig() const { return config_; }

private:
    // Core decision logic
    void computeRecommendation(
        const std::map<std::string, TelemetrySample>& samples,
        uint64_t nowMs);
    
    // Quality and fail-safe checks
    bool checkTelemetryQuality(const std::map<std::string, TelemetrySample>& samples);
    void transitionToFailSafe(DecisionReason reason, uint64_t nowMs);
    
    // Strategy implementation
    void applySelfConsumptionStrategy(
        double pvPowerW,
        double gridPowerW,
        double batteryPowerW,
        double socPct,
        uint64_t nowMs);
    
    // Divergence tracking
    void updateDivergence(double actualBatteryPowerW, uint64_t nowMs);
    
    // Constraint enforcement
    double applyConstraints(double requestedPowerW, double socPct, uint64_t nowMs);
    
    // Power smoothing/ramping
    double applySmoothing(double targetPowerW, uint64_t nowMs);
    
    ShadowEMSConfig config_;
    ShadowEMSState state_;
    
    // Smoothing state
    double lastRecommendedPowerW_ = 0.0;
    uint64_t lastUpdateMs_ = 0;
    
    // Divergence tracking state
    double lastDivergencePowerW_ = 0.0;
    uint64_t divergenceStartMs_ = 0;
    bool inDivergenceEvent_ = false;
};

}  // namespace ems
}  // namespace heliograph
