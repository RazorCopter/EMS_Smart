// SPDX-License-Identifier: MIT
// Historical Home Assistant offline replay engine for TargetAwareEMS.
// Performs purely offline, deterministic evaluation of historical data.

#pragma once

#include "ems/target_aware_ems.h"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace heliograph::ems::replay {

struct HAReplayRecord {
  std::string time;
  uint16_t minuteOfDay = 0;
  uint8_t dayOfWeek = 1; // 0=Sunday .. 6=Saturday
  uint8_t month = 6;     // 1..12
  uint8_t dayOfMonth = 15;
  double soc = 0.0;
  double batteryPowerW = 0.0;
  double pvPowerW = 0.0;
  double gridPowerW = 0.0;
  double loadPowerW = 0.0;
  double forecastTodayKWh = 0.0;
  double forecastTomorrowKWh = 0.0;
  uint64_t forecastAgeSeconds = 0;
  double temperatureC = 15.0;
  double dewPointC = 8.0;
  double radiationWm2 = 0.0;
  double actualToForecastRatio = 1.0;
  bool stableFog = false;
  double nativeInverterPowerW = 0.0;
};

struct ReplayStepResult {
  HAReplayRecord record;
  target::Evaluation evaluation;
  double setpointRampWPerHour = 0.0;
  double nativeDivergenceW = 0.0;
};

struct DailyReplayMetrics {
  std::string scenarioName;
  size_t totalSteps = 0;
  double targetTimeSocPct = 0.0; // SOC at target time (15:30)
  int targetReachedMinute =
      -1; // First minute target SOC was reached (-1 if not)
  int fullSocReachedMinute =
      -1; // First minute 100% SOC was reached (-1 if not)
  double maxSetpointRampWPerHour = 0.0;
  double avgSetpointRampWPerHour = 0.0;
  std::map<target::RecommendationReason, uint32_t> reasonCounts;
  double nativeDivergenceMAE =
      0.0; // Mean Absolute Error vs native inverter (W)
  double nativeDivergenceMaxW = 0.0; // Peak divergence vs native inverter (W)
  double totalEnergyRecommendedKWh = 0.0;
  std::vector<ReplayStepResult> stepResults;
};

const char *reasonToString(target::RecommendationReason reason);

class HAReplayEngine {
public:
  explicit HAReplayEngine(target::EMSConfig config = {});

  // Load records from CSV string or file
  static std::vector<HAReplayRecord> parseCsv(const std::string &csvContent);
  static std::vector<HAReplayRecord> loadCsvFile(const std::string &filePath);

  // Load records from JSON string or file
  static std::vector<HAReplayRecord> parseJson(const std::string &jsonContent);
  static std::vector<HAReplayRecord> loadJsonFile(const std::string &filePath);

  // Run replay on a sequence of records
  DailyReplayMetrics runReplay(const std::vector<HAReplayRecord> &records,
                               const std::string &scenarioName = "replay");

  // Format metrics as Markdown report
  static std::string formatMarkdownReport(const DailyReplayMetrics &metrics);

  const target::EMSConfig &config() const { return config_; }

private:
  target::TargetAwareEMS ems_;
  target::EMSConfig config_;
};

} // namespace heliograph::ems::replay
