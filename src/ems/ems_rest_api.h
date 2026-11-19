// SPDX-License-Identifier: MIT

#pragma once

#include "ems/strategy_engine.h"
#include <string>

namespace heliograph::ems {

/// Formats a complete EMS status and configuration JSON payload for REST
/// /api/v1/ems/state
std::string formatEmsStateJson(const StrategyEngine &engine,
                               const TelemetryInput &telemetry);

/// Formats an EMS decision record history entry as JSON
std::string formatEmsDecisionJson(const EmsDecisionRecord &record);

/// Parses an incoming JSON request body to reconfigure EMS strategies or
/// constraints. Returns true if parsing was successful and engine was updated.
bool updateEmsFromJson(StrategyEngine &engine, const std::string &jsonBody,
                       std::string &errorMsg);

} // namespace heliograph::ems
