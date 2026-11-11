// SPDX-License-Identifier: MIT

#include "ems/ha_replay_engine.h"

#include <ArduinoJson.h>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace heliograph::ems::replay {
namespace {

std::string trim(const std::string &str) {
  const size_t first = str.find_first_not_of(" \t\r\n\"");
  if (first == std::string::npos) {
    return "";
  }
  const size_t last = str.find_last_not_of(" \t\r\n\"");
  return str.substr(first, (last - first + 1));
}

std::vector<std::string> splitCsvLine(const std::string &line) {
  std::vector<std::string> result;
  std::stringstream ss(line);
  std::string item;
  while (std::getline(ss, item, ',')) {
    result.push_back(trim(item));
  }
  return result;
}

double safeStod(const std::string &val, double defaultVal = 0.0) {
  if (val.empty() || val == "unavailable" || val == "unknown") {
    return defaultVal;
  }
  try {
    return std::stod(val);
  } catch (...) {
    return defaultVal;
  }
}

uint16_t parseTimeOrMinute(const std::string &val) {
  const size_t colon = val.find(':');
  if (colon != std::string::npos) {
    try {
      int h = std::stoi(val.substr(0, colon));
      int m = std::stoi(val.substr(colon + 1));
      return static_cast<uint16_t>(std::max(0, h * 60 + m));
    } catch (...) {
      return 0;
    }
  }
  try {
    return static_cast<uint16_t>(std::max(0, std::stoi(val)));
  } catch (...) {
    return 0;
  }
}

} // namespace

const char *reasonToString(target::RecommendationReason reason) {
  switch (reason) {
  case target::RecommendationReason::TargetSatisfied:
    return "TargetSatisfied";
  case target::RecommendationReason::TargetCharge:
    return "TargetCharge";
  case target::RecommendationReason::RecoveryCharge:
    return "RecoveryCharge";
  case target::RecommendationReason::Override:
    return "Override";
  case target::RecommendationReason::ForecastUnavailable:
    return "ForecastUnavailable";
  case target::RecommendationReason::TargetNotFeasible:
    return "TargetNotFeasible";
  case target::RecommendationReason::DataInvalid:
    return "DataInvalid";
  case target::RecommendationReason::NightIdle:
    return "NightIdle";
  }
  return "Unknown";
}

HAReplayEngine::HAReplayEngine(target::EMSConfig config)
    : ems_(config), config_(std::move(config)) {}

std::vector<HAReplayRecord>
HAReplayEngine::parseCsv(const std::string &csvContent) {
  std::vector<HAReplayRecord> records;
  std::stringstream ss(csvContent);
  std::string line;
  if (!std::getline(ss, line)) {
    return records;
  }

  std::vector<std::string> headers = splitCsvLine(line);
  std::map<std::string, size_t> colMap;
  for (size_t i = 0; i < headers.size(); ++i) {
    colMap[headers[i]] = i;
  }

  auto getCol = [&](const std::vector<std::string> &row,
                    const std::string &key) -> std::string {
    auto it = colMap.find(key);
    if (it != colMap.end() && it->second < row.size()) {
      return row[it->second];
    }
    return "";
  };

  while (std::getline(ss, line)) {
    std::string trimmedLine = trim(line);
    if (trimmedLine.empty()) {
      continue;
    }
    std::vector<std::string> row = splitCsvLine(trimmedLine);
    HAReplayRecord rec;

    // Time & Minute
    std::string timeVal = getCol(row, "time");
    if (timeVal.empty()) {
      timeVal = getCol(row, "timestamp");
    }
    rec.time = timeVal;

    std::string minVal = getCol(row, "minute_of_day");
    if (!minVal.empty()) {
      rec.minuteOfDay = parseTimeOrMinute(minVal);
    } else if (!rec.time.empty()) {
      rec.minuteOfDay = parseTimeOrMinute(rec.time);
    }

    // SOC
    std::string socVal = getCol(row, "sensor.afore_soc");
    if (socVal.empty())
      socVal = getCol(row, "soc");
    rec.soc = safeStod(socVal, 0.0);

    // Battery Power
    std::string bpVal = getCol(row, "sensor.afore_battery_power");
    if (bpVal.empty())
      bpVal = getCol(row, "battery_power");
    rec.batteryPowerW = safeStod(bpVal, 0.0);

    // PV Power
    std::string pvVal = getCol(row, "sensor.afore_pv_power");
    if (pvVal.empty())
      pvVal = getCol(row, "pv_power");
    rec.pvPowerW = safeStod(pvVal, 0.0);

    // Grid Power
    std::string gpVal = getCol(row, "sensor.afore_grid_power");
    if (gpVal.empty())
      gpVal = getCol(row, "grid_power");
    rec.gridPowerW = safeStod(gpVal, 0.0);

    // Load Power
    std::string lpVal = getCol(row, "sensor.afore_load_power");
    if (lpVal.empty())
      lpVal = getCol(row, "load_power");
    rec.loadPowerW = safeStod(lpVal, 0.0);

    // Forecasts
    std::string ftVal = getCol(row, "sensor.solcast_pv_forecast_today");
    if (ftVal.empty())
      ftVal = getCol(row, "forecast_today");
    rec.forecastTodayKWh = safeStod(ftVal, 0.0);

    std::string ftmVal = getCol(row, "sensor.solcast_pv_forecast_tomorrow");
    if (ftmVal.empty())
      ftmVal = getCol(row, "forecast_tomorrow");
    rec.forecastTomorrowKWh = safeStod(ftmVal, 0.0);

    std::string faVal = getCol(row, "forecast_age_seconds");
    rec.forecastAgeSeconds = static_cast<uint64_t>(safeStod(faVal, 300.0));

    // Weather
    std::string tVal = getCol(row, "sensor.temperatura_esterna");
    if (tVal.empty())
      tVal = getCol(row, "temperature");
    rec.temperatureC = safeStod(tVal, 15.0);

    std::string dpVal = getCol(row, "sensor.punto_rugiada");
    if (dpVal.empty())
      dpVal = getCol(row, "dew_point");
    rec.dewPointC = safeStod(dpVal, 8.0);

    std::string rVal = getCol(row, "sensor.radiazione");
    if (rVal.empty())
      rVal = getCol(row, "radiation");
    rec.radiationWm2 = safeStod(rVal, 0.0);

    std::string ratioVal = getCol(row, "actual_to_forecast_ratio");
    rec.actualToForecastRatio = safeStod(ratioVal, 1.0);

    std::string fogVal = getCol(row, "binary_sensor.sem_nebbia_stabile");
    if (fogVal.empty())
      fogVal = getCol(row, "fog");
    rec.stableFog = (fogVal == "1" || fogVal == "true" || fogVal == "on");

    // Native Inverter Power
    std::string nipVal = getCol(row, "native_inverter_battery_power");
    if (nipVal.empty())
      nipVal = getCol(row, "native_battery_power");
    rec.nativeInverterPowerW = safeStod(nipVal, rec.batteryPowerW);

    records.push_back(rec);
  }
  return records;
}

std::vector<HAReplayRecord>
HAReplayEngine::loadCsvFile(const std::string &filePath) {
  std::ifstream file(filePath);
  if (!file.is_open()) {
    return {};
  }
  std::stringstream buffer;
  buffer << file.rdbuf();
  return parseCsv(buffer.str());
}

std::vector<HAReplayRecord>
HAReplayEngine::parseJson(const std::string &jsonContent) {
  std::vector<HAReplayRecord> records;
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, jsonContent);
  if (err) {
    return records;
  }

  JsonArrayConst array;
  if (doc["records"].is<JsonArrayConst>()) {
    array = doc["records"].as<JsonArrayConst>();
  } else if (doc.is<JsonArrayConst>()) {
    array = doc.as<JsonArrayConst>();
  } else {
    return records;
  }

  for (JsonObjectConst item : array) {
    HAReplayRecord rec;
    rec.time = item["time"] | item["timestamp"] | "";
    if (item["minute_of_day"].is<uint16_t>()) {
      rec.minuteOfDay = item["minute_of_day"].as<uint16_t>();
    } else if (!rec.time.empty()) {
      rec.minuteOfDay = parseTimeOrMinute(rec.time);
    }

    rec.soc = item["sensor.afore_soc"] | item["soc"] | 0.0;
    rec.batteryPowerW =
        item["sensor.afore_battery_power"] | item["battery_power"] | 0.0;
    rec.pvPowerW = item["sensor.afore_pv_power"] | item["pv_power"] | 0.0;
    rec.gridPowerW = item["sensor.afore_grid_power"] | item["grid_power"] | 0.0;
    rec.loadPowerW = item["sensor.afore_load_power"] | item["load_power"] | 0.0;

    rec.forecastTodayKWh =
        item["sensor.solcast_pv_forecast_today"] | item["forecast_today"] | 0.0;
    rec.forecastTomorrowKWh = item["sensor.solcast_pv_forecast_tomorrow"] |
                              item["forecast_tomorrow"] | 0.0;
    rec.forecastAgeSeconds = item["forecast_age_seconds"] | 300ULL;

    rec.temperatureC =
        item["sensor.temperatura_esterna"] | item["temperature"] | 15.0;
    rec.dewPointC = item["sensor.punto_rugiada"] | item["dew_point"] | 8.0;
    rec.radiationWm2 = item["sensor.radiazione"] | item["radiation"] | 0.0;
    rec.actualToForecastRatio = item["actual_to_forecast_ratio"] | 1.0;

    if (item["binary_sensor.sem_nebbia_stabile"].is<bool>()) {
      rec.stableFog = item["binary_sensor.sem_nebbia_stabile"].as<bool>();
    } else if (item["binary_sensor.sem_nebbia_stabile"].is<int>()) {
      rec.stableFog = item["binary_sensor.sem_nebbia_stabile"].as<int>() != 0;
    } else if (item["fog"].is<bool>()) {
      rec.stableFog = item["fog"].as<bool>();
    }

    rec.nativeInverterPowerW = item["native_inverter_battery_power"] |
                               item["native_battery_power"] | rec.batteryPowerW;

    records.push_back(rec);
  }
  return records;
}

std::vector<HAReplayRecord>
HAReplayEngine::loadJsonFile(const std::string &filePath) {
  std::ifstream file(filePath);
  if (!file.is_open()) {
    return {};
  }
  std::stringstream buffer;
  buffer << file.rdbuf();
  return parseJson(buffer.str());
}

DailyReplayMetrics
HAReplayEngine::runReplay(const std::vector<HAReplayRecord> &records,
                          const std::string &scenarioName) {
  DailyReplayMetrics metrics;
  metrics.scenarioName = scenarioName;
  metrics.totalSteps = records.size();

  if (records.empty()) {
    return metrics;
  }

  target::EMSState state;
  double previousSetpointW = 0.0;
  uint16_t previousMinute = 0;
  double totalRampAccum = 0.0;
  double totalDivAccum = 0.0;
  int targetMinuteDiff = 9999;

  for (size_t i = 0; i < records.size(); ++i) {
    const HAReplayRecord &rec = records[i];

    target::EMSInput input;
    input.clock.minuteOfDay = rec.minuteOfDay;
    input.clock.dayOfWeek = rec.dayOfWeek;
    input.clock.month = rec.month;
    input.clock.dayOfMonth = rec.dayOfMonth;

    input.socPct =
        target::Datum<double>{{rec.soc}, target::DataQuality::Valid, 0};
    input.batteryPowerW = target::Datum<double>{
        {rec.batteryPowerW}, target::DataQuality::Valid, 0};
    input.pvPowerW =
        target::Datum<double>{{rec.pvPowerW}, target::DataQuality::Valid, 0};

    // Forecast input
    if (rec.forecastTodayKWh > 0.0 &&
        rec.forecastAgeSeconds <= config_.forecastMaximumAgeSeconds) {
      input.forecast.available = true;
      input.forecast.ageSeconds = rec.forecastAgeSeconds;
      input.forecast.tomorrowKWh = rec.forecastTomorrowKWh;

      // Distribute today forecast across daytime slots up to target time
      input.forecast.today.clear();
      for (uint16_t slotM = 6 * 60; slotM < config_.targetMinuteOfDay;
           slotM += 30) {
        double angle =
            (slotM - 360.0) / (config_.targetMinuteOfDay - 360.0) * 3.14159265;
        double weight = std::sin(angle);
        if (weight > 0.0) {
          input.forecast.today.push_back(
              {slotM, rec.forecastTodayKWh * (weight / 8.0)});
        }
      }
    } else {
      input.forecast.available = false;
      input.forecast.ageSeconds = rec.forecastAgeSeconds;
      input.forecast.tomorrowKWh = rec.forecastTomorrowKWh;
    }

    // Weather input
    input.weather.temperatureC = target::Datum<double>{
        {rec.temperatureC}, target::DataQuality::Valid, 0};
    input.weather.dewPointC =
        target::Datum<double>{{rec.dewPointC}, target::DataQuality::Valid, 0};
    input.weather.radiationWm2 = target::Datum<double>{
        {rec.radiationWm2}, target::DataQuality::Valid, 0};
    input.weather.actualToForecastRatio = target::Datum<double>{
        {rec.actualToForecastRatio}, target::DataQuality::Valid, 0};
    input.weather.stableFog = rec.stableFog;

    // Simulation time in seconds
    uint64_t nowSeconds = static_cast<uint64_t>(rec.minuteOfDay) * 60;
    target::Evaluation eval = ems_.evaluate(input, state, nowSeconds);

    // Compute step interval dt (hours)
    double dtHours = 0.25; // default 15 min
    if (i > 0 && rec.minuteOfDay > previousMinute) {
      dtHours = (rec.minuteOfDay - previousMinute) / 60.0;
    }

    // Anti-oscillation metric: ramp rate in W / hour
    double rampWPerHour = 0.0;
    if (i > 0 && dtHours > 0.0) {
      rampWPerHour =
          std::abs(eval.recommendation.batteryPowerW - previousSetpointW) /
          dtHours;
    }
    metrics.maxSetpointRampWPerHour =
        std::max(metrics.maxSetpointRampWPerHour, rampWPerHour);
    totalRampAccum += rampWPerHour;

    // Native divergence
    double divergenceW =
        std::abs(eval.recommendation.batteryPowerW - rec.nativeInverterPowerW);
    metrics.nativeDivergenceMaxW =
        std::max(metrics.nativeDivergenceMaxW, divergenceW);
    totalDivAccum += divergenceW;

    // Total energy recommended (charging only, kWh)
    if (eval.recommendation.batteryPowerW > 0.0) {
      metrics.totalEnergyRecommendedKWh +=
          eval.recommendation.batteryPowerW * dtHours / 1000.0;
    }

    // Record decision reason
    metrics.reasonCounts[eval.recommendation.reason]++;

    // Target tracking at 15:30 (targetMinuteOfDay)
    int diffToTarget = std::abs(static_cast<int>(rec.minuteOfDay) -
                                static_cast<int>(config_.targetMinuteOfDay));
    if (diffToTarget < targetMinuteDiff) {
      targetMinuteDiff = diffToTarget;
      metrics.targetTimeSocPct = rec.soc;
    }

    // Milestone: target reached
    if (metrics.targetReachedMinute < 0 && rec.soc >= config_.targetSocPct) {
      metrics.targetReachedMinute = rec.minuteOfDay;
    }
    // Milestone: 100% full reached
    if (metrics.fullSocReachedMinute < 0 && rec.soc >= 99.9) {
      metrics.fullSocReachedMinute = rec.minuteOfDay;
    }

    ReplayStepResult stepResult;
    stepResult.record = rec;
    stepResult.evaluation = eval;
    stepResult.setpointRampWPerHour = rampWPerHour;
    stepResult.nativeDivergenceW = divergenceW;
    metrics.stepResults.push_back(stepResult);

    previousSetpointW = eval.recommendation.batteryPowerW;
    previousMinute = rec.minuteOfDay;
    state = eval.nextState;
  }

  if (metrics.totalSteps > 0) {
    metrics.avgSetpointRampWPerHour = totalRampAccum / metrics.totalSteps;
    metrics.nativeDivergenceMAE = totalDivAccum / metrics.totalSteps;
  }

  return metrics;
}

std::string
HAReplayEngine::formatMarkdownReport(const DailyReplayMetrics &metrics) {
  std::ostringstream ss;
  ss << "### Replay Report: `" << metrics.scenarioName << "`\n\n";
  ss << "| Metrica | Valore |\n";
  ss << "|---|---|\n";
  ss << "| Campioni totali | " << metrics.totalSteps << " |\n";
  ss << "| SOC alle 15:30 (target time) | " << std::fixed
     << std::setprecision(1) << metrics.targetTimeSocPct << "% |\n";

  if (metrics.targetReachedMinute >= 0) {
    int h = metrics.targetReachedMinute / 60;
    int m = metrics.targetReachedMinute % 60;
    ss << "| Raggiungimento target | " << std::setfill('0') << std::setw(2) << h
       << ":" << std::setw(2) << m << " (minuto " << metrics.targetReachedMinute
       << ") |\n";
  } else {
    ss << "| Raggiungimento target | Non raggiunto |\n";
  }

  if (metrics.fullSocReachedMinute >= 0) {
    int h = metrics.fullSocReachedMinute / 60;
    int m = metrics.fullSocReachedMinute % 60;
    ss << "| Raggiungimento 100% | " << std::setfill('0') << std::setw(2) << h
       << ":" << std::setw(2) << m << " (minuto "
       << metrics.fullSocReachedMinute << ") |\n";
  } else {
    ss << "| Raggiungimento 100% | Non raggiunto |\n";
  }

  ss << "| Variazione setpoint max | " << std::fixed << std::setprecision(1)
     << metrics.maxSetpointRampWPerHour << " W/h |\n";
  ss << "| Variazione setpoint media | " << std::fixed << std::setprecision(1)
     << metrics.avgSetpointRampWPerHour << " W/h |\n";
  ss << "| Divergenza nativa MAE | " << std::fixed << std::setprecision(1)
     << metrics.nativeDivergenceMAE << " W |\n";
  ss << "| Divergenza nativa Max | " << std::fixed << std::setprecision(1)
     << metrics.nativeDivergenceMaxW << " W |\n";
  ss << "| Energia raccomandata totale | " << std::fixed << std::setprecision(2)
     << metrics.totalEnergyRecommendedKWh << " kWh |\n\n";

  ss << "#### Decision Breakdown (Reason Codes)\n\n";
  ss << "| Reason Code | Conteggio | Percentuale |\n";
  ss << "|---|---|---|\n";
  for (const auto &pair : metrics.reasonCounts) {
    double pct = (metrics.totalSteps > 0)
                     ? (100.0 * pair.second / metrics.totalSteps)
                     : 0.0;
    ss << "| `" << reasonToString(pair.first) << "` | " << pair.second << " | "
       << std::fixed << std::setprecision(1) << pct << "% |\n";
  }
  ss << "\n";

  return ss.str();
}

} // namespace heliograph::ems::replay
