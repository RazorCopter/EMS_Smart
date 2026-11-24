// SPDX-License-Identifier: MIT

#include "ems/ems_rest_api.h"
#include <cstdio>
#include <sstream>

namespace heliograph::ems {

std::string formatEmsStateJson(const StrategyEngine &engine,
                               const TelemetryInput &telemetry) {
  char buf[1024];
  const auto &sc = engine.config();
  const auto &tc = engine.constraints();

  std::snprintf(
      buf, sizeof(buf),
      "{"
      "\"mode\":\"%s\","
      "\"current_power_w\":%ld,"
      "\"battery_soc_pct\":%.1f,"
      "\"grid_power_w\":%ld,"
      "\"pv_power_w\":%ld,"
      "\"load_power_w\":%ld,"
      "\"config\":{"
      "\"soc_reserve_floor_pct\":%.1f,"
      "\"soc_charge_first_target_pct\":%.1f,"
      "\"charge_first_grid_assist_w\":%ld,"
      "\"maintenance_target_soc_pct\":%.1f,"
      "\"maintenance_power_w\":%ld,"
      "\"tou_active\":%s,"
      "\"tou_target_soc_pct\":%.1f"
      "},"
      "\"constraints\":{"
      "\"max_charge_power_w\":%ld,"
      "\"max_discharge_power_w\":%ld,"
      "\"allow_grid_charging\":%s,"
      "\"max_ramp_rate_w_per_s\":%ld,"
      "\"deadband_w\":%ld"
      "}"
      "}",
      strategyModeToString(sc.mode), static_cast<long>(engine.currentPower()),
      static_cast<double>(telemetry.batterySocPct),
      static_cast<long>(telemetry.gridPowerW),
      static_cast<long>(telemetry.pvPowerW),
      static_cast<long>(telemetry.loadPowerW),
      static_cast<double>(sc.socReserveFloorPct),
      static_cast<double>(sc.socChargeFirstTargetPct),
      static_cast<long>(sc.chargeFirstGridAssistW),
      static_cast<double>(sc.maintenanceTargetSocPct),
      static_cast<long>(sc.maintenancePowerW),
      sc.touLowTariffWindowActive ? "true" : "false",
      static_cast<double>(sc.touTargetSocPct),
      static_cast<long>(tc.maxChargePowerW),
      static_cast<long>(tc.maxDischargePowerW),
      tc.allowGridCharging ? "true" : "false",
      static_cast<long>(tc.maxRampRateWPerS), static_cast<long>(tc.deadbandW));

  return std::string(buf);
}

std::string formatEmsDecisionJson(const EmsDecisionRecord &record) {
  char buf[1024];
  record.toJson(buf, sizeof(buf));
  return std::string(buf);
}

bool updateEmsFromJson(StrategyEngine &engine, const std::string &jsonBody,
                       std::string & /*errorMsg*/) {
  // Lightweight C++ parsing of key fields without huge runtime dependencies
  auto sc = engine.config();
  auto tc = engine.constraints();

  // Check for "mode"
  auto modePos = jsonBody.find("\"mode\"");
  if (modePos != std::string::npos) {
    auto valStart = jsonBody.find('"', modePos + 6);
    if (valStart != std::string::npos) {
      auto valEnd = jsonBody.find('"', valStart + 1);
      if (valEnd != std::string::npos) {
        std::string modeStr =
            jsonBody.substr(valStart + 1, valEnd - valStart - 1);
        sc.mode = strategyModeFromString(modeStr.c_str());
      }
    }
  }

  // Check for "soc_reserve_floor_pct"
  auto socFloorPos = jsonBody.find("\"soc_reserve_floor_pct\"");
  if (socFloorPos != std::string::npos) {
    auto colonPos = jsonBody.find(':', socFloorPos);
    if (colonPos != std::string::npos) {
      float val = std::stof(jsonBody.substr(colonPos + 1));
      sc.socReserveFloorPct = val;
      tc.socReserveFloorPct = val;
    }
  }

  // Check for "allow_grid_charging"
  auto gridChargePos = jsonBody.find("\"allow_grid_charging\"");
  if (gridChargePos != std::string::npos) {
    if (jsonBody.find("true", gridChargePos) != std::string::npos &&
        jsonBody.find("true", gridChargePos) < gridChargePos + 30) {
      tc.allowGridCharging = true;
    } else if (jsonBody.find("false", gridChargePos) != std::string::npos &&
               jsonBody.find("false", gridChargePos) < gridChargePos + 30) {
      tc.allowGridCharging = false;
    }
  }

  engine.updateConfig(sc);
  engine.updateConstraints(tc);
  return true;
}

} // namespace heliograph::ems
