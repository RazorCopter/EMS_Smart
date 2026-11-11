// SPDX-License-Identifier: MIT

#include "ems/ha_replay_engine.h"

#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>

using namespace heliograph::ems::replay;
using namespace heliograph::ems::target;

namespace {

int failures = 0;

void check(bool condition, const std::string &scenario,
           const std::string &detail) {
  if (!condition) {
    ++failures;
    std::cerr << "FAIL [" << scenario << "]: " << detail << '\n';
  }
}

std::string resolveFixture(const std::string &relPath) {
  const std::string candidates[] = {
      relPath, "test/test_ha_replay/fixtures/" + relPath,
      "../test/test_ha_replay/fixtures/" + relPath,
      "/workspace/test/test_ha_replay/fixtures/" + relPath};
  for (const auto &p : candidates) {
    std::ifstream f(p);
    if (f.good()) {
      return p;
    }
  }
  return "test/test_ha_replay/fixtures/" + relPath;
}

void test_parse_csv_and_json_parity() {
  const std::string name = "parse_csv_and_json_parity";
  auto csvRecords =
      HAReplayEngine::loadCsvFile(resolveFixture("sunny_day.csv"));
  auto jsonRecords =
      HAReplayEngine::loadJsonFile(resolveFixture("sunny_day.json"));

  check(!csvRecords.empty(), name, "CSV records must not be empty");
  check(!jsonRecords.empty(), name, "JSON records must not be empty");
  check(csvRecords.size() == 96, name,
        "Expected 96 CSV records (15-min intervals)");
  check(jsonRecords.size() == 96, name,
        "Expected 96 JSON records (15-min intervals)");
  check(csvRecords.size() == jsonRecords.size(), name,
        "Record count parity between CSV and JSON");

  // Check first record parity
  if (!csvRecords.empty() && !jsonRecords.empty()) {
    check(csvRecords[0].minuteOfDay == jsonRecords[0].minuteOfDay, name,
          "Minute parity on step 0");
    check(std::abs(csvRecords[0].soc - jsonRecords[0].soc) < 1e-3, name,
          "SOC parity on step 0");
    check(std::abs(csvRecords[0].pvPowerW - jsonRecords[0].pvPowerW) < 1e-3,
          name, "PV parity on step 0");
  }
}

void test_replay_sunny_day() {
  const std::string name = "replay_sunny_day";
  auto records = HAReplayEngine::loadJsonFile(resolveFixture("sunny_day.json"));
  check(!records.empty(), name, "Records loaded successfully");

  EMSConfig config;
  config.targetSocPct = 100.0;
  config.targetMinuteOfDay = 15 * 60 + 30; // 15:30
  HAReplayEngine engine(config);

  DailyReplayMetrics metrics = engine.runReplay(records, "sunny_day");

  check(metrics.totalSteps == 96, name, "Total steps must be 96");
  check(metrics.targetTimeSocPct >= 95.0, name,
        "Target time SOC at 15:30 must reach at least 95%");
  check(metrics.targetReachedMinute > 0 &&
            metrics.targetReachedMinute <= 15 * 60 + 30,
        name, "Target reached before or at target time 15:30");
  check(metrics.fullSocReachedMinute > 0 &&
            metrics.fullSocReachedMinute <= 15 * 60 + 30,
        name, "100% full reached before or at target time 15:30");

  // Verify markdown report generation
  std::string report = HAReplayEngine::formatMarkdownReport(metrics);
  std::cout << report << std::endl;
  check(!report.empty(), name, "Markdown report string generated");

  // Anti-oscillation: setpoint variation rate bounded
  check(metrics.maxSetpointRampWPerHour <= 12000.0, name,
        "Ramp rate should be safely bounded");
  check(metrics.avgSetpointRampWPerHour > 0.0, name,
        "Ramp rate average computed");

  // Divergence from native inverter
  check(metrics.nativeDivergenceMAE > 0.0, name,
        "Measurable divergence from native unmanaged inverter charging");

  // Reasons breakdown check
  check(metrics.reasonCounts[RecommendationReason::NightIdle] > 0, name,
        "NightIdle expected during night hours");
  check(metrics.reasonCounts[RecommendationReason::TargetSatisfied] > 0, name,
        "TargetSatisfied expected once full");
  check(report.find("Replay Report: `sunny_day`") != std::string::npos, name,
        "Markdown report contains title");
  check(report.find("15:30") != std::string::npos, name,
        "Markdown report contains 15:30 target time");
}

void test_replay_foggy_day() {
  const std::string name = "replay_foggy_day";
  auto records = HAReplayEngine::loadJsonFile(resolveFixture("foggy_day.json"));
  check(!records.empty(), name, "Foggy records loaded");

  EMSConfig config;
  HAReplayEngine engine(config);
  DailyReplayMetrics metrics = engine.runReplay(records, "foggy_day");

  // In foggy morning, fog flag should be detected in early daytime steps
  bool fogObserved = false;
  for (const auto &step : metrics.stepResults) {
    if (step.evaluation.recommendation.fogActive) {
      fogObserved = true;
      break;
    }
  }
  check(fogObserved, name, "Fog detector active during foggy conditions");
  check(metrics.nativeDivergenceMAE > 0.0, name,
        "Divergence observed in foggy conditions");
}

void test_replay_evening_load_spike() {
  const std::string name = "replay_evening_load_spike";
  auto records =
      HAReplayEngine::loadJsonFile(resolveFixture("evening_load_spike.json"));
  check(!records.empty(), name, "Evening spike records loaded");

  EMSConfig config;
  HAReplayEngine engine(config);
  DailyReplayMetrics metrics = engine.runReplay(records, "evening_load_spike");

  check(metrics.targetTimeSocPct >= 95.0, name,
        "Battery prepared before evening spike at 15:30");
  check(metrics.targetReachedMinute > 0 &&
            metrics.targetReachedMinute <= 15 * 60 + 30,
        name, "Target reached before evening peak load arrives");
}

void test_replay_missing_forecast_day() {
  const std::string name = "replay_missing_forecast_day";
  auto records =
      HAReplayEngine::loadJsonFile(resolveFixture("missing_forecast_day.json"));
  check(!records.empty(), name, "Missing forecast records loaded");

  EMSConfig config;
  HAReplayEngine engine(config);
  DailyReplayMetrics metrics =
      engine.runReplay(records, "missing_forecast_day");

  // Forecast is stale/missing, should trigger ForecastUnavailable in daytime
  bool forecastUnavailableObserved = false;
  for (const auto &step : metrics.stepResults) {
    if (step.evaluation.recommendation.reason ==
        RecommendationReason::ForecastUnavailable) {
      forecastUnavailableObserved = true;
      break;
    }
  }
  check(forecastUnavailableObserved, name,
        "EMS correctly falls back to ForecastUnavailable when Solcast is "
        "absent/stale");
  check(metrics.reasonCounts[RecommendationReason::ForecastUnavailable] > 0,
        name, "Reason count recorded ForecastUnavailable");
}

void test_corrupted_and_empty_inputs() {
  const std::string name = "corrupted_and_empty_inputs";
  auto empty = HAReplayEngine::parseCsv("");
  check(empty.empty(), name, "Empty CSV string yields empty records");

  auto emptyJson = HAReplayEngine::parseJson("{}");
  check(emptyJson.empty(), name, "Empty JSON object yields empty records");

  std::string corruptedCsv = "sensor.afore_soc,sensor.afore_pv_power\n"
                             "unavailable,unknown\n"
                             "abc,def\n";
  auto corrupted = HAReplayEngine::parseCsv(corruptedCsv);
  check(corrupted.size() == 2, name, "Corrupted rows handled without throwing");
  check(corrupted[0].soc == 0.0, name, "Safe fallback for invalid numbers");
}

} // namespace

int main() {
  std::cout << "--- Starting M5 Historical HA Replay Tests ---\n";
  test_parse_csv_and_json_parity();
  test_replay_sunny_day();
  test_replay_foggy_day();
  test_replay_evening_load_spike();
  test_replay_missing_forecast_day();
  test_corrupted_and_empty_inputs();

  if (failures != 0) {
    std::cerr << failures << " assertion(s) failed in M5 HA Replay\n";
    return EXIT_FAILURE;
  }
  std::cout
      << "PASS: All M5 Historical HA Replay test suites passed successfully!\n";
  return EXIT_SUCCESS;
}
