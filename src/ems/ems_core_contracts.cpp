// SPDX-License-Identifier: MIT
//
// Afore Autonomous EMS Common Core Domain Contracts & Serialization (Milestone
// M3).

#include "ems_core_contracts.h"

#include <cstdio>
#include <cstring>

namespace heliograph {
namespace ems {

const char *strategyModeToString(StrategyMode mode) {
  switch (mode) {
  case StrategyMode::SelfUse:
    return "SelfUse";
  case StrategyMode::ChargeFirst:
    return "ChargeFirst";
  case StrategyMode::SolarOptimized:
    return "SolarOptimized";
  case StrategyMode::TouTariff:
    return "TouTariff";
  case StrategyMode::ScheduledMaintenance:
    return "ScheduledMaintenance";
  case StrategyMode::SafeIdle:
    return "SafeIdle";
  default:
    return "Unknown";
  }
}

StrategyMode strategyModeFromString(const char *str) {
  if (str == nullptr)
    return StrategyMode::SelfUse;
  if (std::strcmp(str, "SelfUse") == 0)
    return StrategyMode::SelfUse;
  if (std::strcmp(str, "ChargeFirst") == 0)
    return StrategyMode::ChargeFirst;
  if (std::strcmp(str, "SolarOptimized") == 0)
    return StrategyMode::SolarOptimized;
  if (std::strcmp(str, "TouTariff") == 0)
    return StrategyMode::TouTariff;
  if (std::strcmp(str, "ScheduledMaintenance") == 0)
    return StrategyMode::ScheduledMaintenance;
  if (std::strcmp(str, "SafeIdle") == 0)
    return StrategyMode::SafeIdle;
  return StrategyMode::SelfUse;
}

const char *authoritySourceToString(AuthoritySource auth) {
  switch (auth) {
  case AuthoritySource::LocalAutonomous:
    return "LocalAutonomous";
  case AuthoritySource::ScheduledMaintenance:
    return "ScheduledMaintenance";
  case AuthoritySource::MqttRemoteOverride:
    return "MqttRemoteOverride";
  case AuthoritySource::RestApiOverride:
    return "RestApiOverride";
  case AuthoritySource::SafetyEmergency:
    return "SafetyEmergency";
  default:
    return "Unknown";
  }
}

size_t EmsDecisionRecord::toJson(char *buffer, size_t capacity) const {
  if (buffer == nullptr || capacity == 0)
    return 0;

  int written = std::snprintf(
      buffer, capacity,
      "{\"seq\":%llu,\"ts\":%llu,\"strategy\":\"%s\",\"authority\":\"%s\","
      "\"proposed_w\":%d,\"authorized_w\":%d,\"actuated_w\":%d,"
      "\"constraints_mask\":%u,\"reason\":\"%s\"}",
      static_cast<unsigned long long>(sequenceId),
      static_cast<unsigned long long>(timestampMs),
      strategyModeToString(strategy), authoritySourceToString(authority),
      static_cast<int>(proposedPowerW), static_cast<int>(authorizedPowerW),
      static_cast<int>(actuatedPowerW),
      static_cast<unsigned int>(appliedConstraintsMask), justification);

  if (written < 0) {
    buffer[0] = '\0';
    return 0;
  }
  if (static_cast<size_t>(written) >= capacity) {
    return capacity - 1;
  }
  return static_cast<size_t>(written);
}

} // namespace ems
} // namespace heliograph
