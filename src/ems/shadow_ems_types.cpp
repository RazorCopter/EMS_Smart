// SPDX-License-Identifier: MIT
//
// Shadow EMS Type Definitions Implementation (CP4).

#include "shadow_ems_types.h"

namespace heliograph {
namespace ems {

const char* operatingModeToString(OperatingMode mode) {
    switch (mode) {
        case OperatingMode::SafeIdle: return "SafeIdle";
        case OperatingMode::PassThrough: return "PassThrough";
        case OperatingMode::Charging: return "Charging";
        case OperatingMode::Discharging: return "Discharging";
        case OperatingMode::Hold: return "Hold";
        case OperatingMode::SolarClipping: return "SolarClipping";
        default: return "Unknown";
    }
}

const char* decisionReasonToString(DecisionReason reason) {
    switch (reason) {
        case DecisionReason::Unknown: return "Unknown";
        case DecisionReason::TelemetryInvalid: return "TelemetryInvalid";
        case DecisionReason::TelemetryDegraded: return "TelemetryDegraded";
        case DecisionReason::SurplusSolarAvailable: return "SurplusSolarAvailable";
        case DecisionReason::DeficitGridImport: return "DeficitGridImport";
        case DecisionReason::SOCLimitReached: return "SOCLimitReached";
        case DecisionReason::PowerLimitReached: return "PowerLimitReached";
        case DecisionReason::BalancedOperation: return "BalancedOperation";
        case DecisionReason::SolarClipping: return "SolarClipping";
        case DecisionReason::InitialState: return "InitialState";
        default: return "Unknown";
    }
}

}  // namespace ems
}  // namespace heliograph
