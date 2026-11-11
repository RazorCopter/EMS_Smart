// SPDX-License-Identifier: MIT
// Pure target-aware EMS core. This module performs no I/O and only emits
// recommendations.

#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace heliograph::ems::target {

// Canonical battery convention throughout this API: positive charges, negative
// discharges.
enum class DataQuality : uint8_t {
  Valid,
  Stale,
  Invalid,
  OutOfRange,
  CommError,
  Unavailable,
  Unverified,
};

template <typename T> struct Datum {
  std::optional<T> value;
  DataQuality quality = DataQuality::Unavailable;
  uint64_t ageSeconds = 0;

  bool actionable(uint64_t maximumAgeSeconds) const {
    return value.has_value() &&
           (quality == DataQuality::Valid ||
            quality == DataQuality::Unverified) &&
           ageSeconds <= maximumAgeSeconds;
  }
};

struct ClockInput {
  uint16_t minuteOfDay = 0;
  uint8_t dayOfWeek = 1; // 0 Sunday ... 6 Saturday
  uint8_t month = 1;
  uint8_t dayOfMonth = 1;
};

struct ForecastSlot {
  uint16_t startMinuteOfDay = 0;
  double pvEstimateKWh = 0.0;
};

struct ForecastInput {
  std::vector<ForecastSlot> today;
  double tomorrowKWh = 0.0;
  uint64_t ageSeconds = 0;
  bool available = false;
};

struct WeatherInput {
  Datum<double> temperatureC;
  Datum<double> dewPointC;
  Datum<double> radiationWm2;
  Datum<double> actualToForecastRatio;
  std::optional<bool> stableFog;
};

struct OverrideInput {
  std::optional<double> targetSocPct;
  std::optional<double> requestedBatteryPowerW;
};

struct EMSInput {
  ClockInput clock;
  Datum<double> socPct;
  Datum<double> batteryPowerW;
  Datum<double> pvPowerW;
  ForecastInput forecast;
  WeatherInput weather;
  OverrideInput overrideInput;
};

struct EMSConfig {
  double targetSocPct = 100.0;
  uint16_t targetMinuteOfDay = 15 * 60 + 30;
  uint16_t anticipationMinutes = 20;
  uint16_t recoveryMinutes = 60;
  double batteryCapacityKWh = 10.24;
  double chargeEfficiency = 0.90;
  double safetyMargin = 1.20;
  double minimumSocPct = 15.0;
  double longevityMaximumSocPct = 100.0;
  double maximumChargePowerW = 2560.0;
  double maximumDischargePowerW = 2560.0;
  double minimumChargePowerW = 256.0;
  bool allowDischarge = false;
  double rampUpWPerCycle = 256.0;
  double rampDownWPerCycle = 410.0;
  double deadbandW = 30.0;
  double hysteresisW = 100.0;
  double weekdayLoadKW = 0.6;
  double weekendLoadKW = 0.9;
  std::array<bool, 7> weekendDays{
      {true, false, false, false, false, false, true}};
  uint64_t telemetryMaximumAgeSeconds = 120;
  uint64_t forecastMaximumAgeSeconds = 14 * 60 * 60;
  uint16_t winterStartMonthDay = 1025;
  uint16_t winterEndMonthDay = 228;
  uint16_t seasonalEvaluationMinute = 8 * 60 + 30;
  double tomorrowFavorableKWh = 7.0;
  double tomorrowMinimumSocPct = 30.0;
};

enum class Feasibility : uint8_t { Feasible, NotFeasible, Unknown };
enum class RecommendationReason : uint8_t {
  TargetSatisfied,
  TargetCharge,
  RecoveryCharge,
  Override,
  ForecastUnavailable,
  TargetNotFeasible,
  DataInvalid,
  NightIdle,
};

struct EnergyAssessment {
  double deficitKWh = 0.0;
  double requiredChargePowerW = 0.0;
  double projectedSocPct = 0.0;
  bool delayed = false;
  bool critical = false;
};

struct EMSState {
  bool fogActive = false;
  bool priorityChargeActive = false;
  bool calibrationRequested = false;
  bool calibrated = false;
  uint64_t fullSocSinceSeconds = 0;
  bool fullSocTimerActive = false;
  double previousRecommendationW = 0.0;
};

struct Recommendation {
  // Advisory value only. No command, transport address, or actuator is
  // represented.
  double batteryPowerW = 0.0;
  double targetSocPct = 0.0;
  Feasibility feasibility = Feasibility::Unknown;
  RecommendationReason reason = RecommendationReason::DataInvalid;
  EnergyAssessment energy;
  bool conservativeMode = false;
  bool fogActive = false;
  bool priorityChargeActive = false;
  bool calibrationCompleted = false;
  double confidence = 0.0;
  std::string explanation;
};

struct Evaluation {
  Recommendation recommendation;
  EMSState nextState;
};

namespace energy_model {
double deficitKWh(double currentSocPct, double targetSocPct,
                  double capacityKWh);
double requiredPowerW(double deficitKWh, double hours, double efficiency,
                      double safetyMargin);
double projectedSocPct(double socPct, double chargePowerW, double hours,
                       double capacityKWh, double efficiency);
} // namespace energy_model

namespace load_profile {
double estimatedLoadKW(const EMSConfig &config, uint8_t dayOfWeek);
} // namespace load_profile

namespace fog_detector {
bool update(const WeatherInput &weather, bool previousFog);
} // namespace fog_detector

namespace seasonal_strategy {
bool isWinter(const EMSConfig &config, uint8_t month, uint8_t dayOfMonth);
bool priorityCharge(const EMSConfig &config, const EMSInput &input,
                    double todaySurplusKWh, double deficitToFullKWh);
} // namespace seasonal_strategy

namespace setpoint_shaper {
double shape(double requestedW, double previousW, const EMSConfig &config,
             bool critical);
} // namespace setpoint_shaper

class TargetAwareEMS {
public:
  explicit TargetAwareEMS(EMSConfig config = {});
  Evaluation evaluate(const EMSInput &input, const EMSState &state,
                      uint64_t nowSeconds) const;

private:
  EMSConfig config_;
};

} // namespace heliograph::ems::target
