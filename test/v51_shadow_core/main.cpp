// SPDX-License-Identifier: MIT
#include "v51/shadow_core.h"
#include <cassert>
#include <string_view>
using namespace heliograph::v51;
EnergyState sample(double soc, double grid, int ms) {
  EnergyState e;
  e.goodReads = 1;
  e.batteryVoltage = {51.2, uint32_t(ms), Quality::Valid};
  e.soc = {soc, uint32_t(ms), Quality::Valid};
  e.grid = {grid, uint32_t(ms), Quality::Valid};
  e.battery = {0, uint32_t(ms), Quality::Valid};
  return e;
}
int main() {
  Policy delegation;
  assert(delegation.valid() && delegation.delegateNativeSelfUse == 0);
  delegation.delegateNativeSelfUse = 2;
  assert(!delegation.valid());
  delegation.delegateNativeSelfUse = 1;
  for (int invalid : {299, 7201}) {
    delegation.nativeSwitchCooldownSec = invalid;
    assert(!delegation.valid());
  }
  delegation.nativeSwitchCooldownSec = 1800;
  for (int invalid : {1, 25}) {
    delegation.maxModeSwitchesPerDay = invalid;
    assert(!delegation.valid());
  }
  StrategyManager m;
  Policy p;
  assert(p.target == 100 && p.capacityWh == 10240 && p.nominalMv == 51200 &&
         p.valid());
  p.target = 80;
  assert(m.tick(sample(50, 600, 1000), p, 1, 1000).finalWatts == -600);
  assert(m.tick(sample(20, 600, 2000), p, 1, 2000).finalWatts == 0);
  assert(m.tick(sample(50, 500, 1000), p, 1, 8000).strategy == Strategy::Safe);
  p.strategy = 1;
  assert(m.tick(sample(50, -1200, 9000), p, 2, 9000).finalWatts == 1000);
  assert(m.tick(sample(79, -1200, 10000), p, 2, 10000).candidate == 250);
  assert(m.tick(sample(77, -1200, 10500), p, 2, 10500).candidate == 250);
  assert(m.tick(sample(75, -1200, 10700), p, 2, 10700).candidate == 1000);
  assert(m.tick(sample(80, -1200, 11000), p, 2, 11000).strategy ==
         Strategy::SelfUse);
  p.strategy = 4;
  p.fallback = 2;
  p.deadlineSec = 10;
  auto d = m.tick(sample(50, -100, 12000), p, 3, 12000);
  assert(d.unreachable && d.missingWh && d.averageW && d.finalWatts == 100);
  assert(m.tick(sample(50, -100, 23000), p, 3, 23000).strategy ==
         Strategy::Native);
  p.strategy = 5;
  p.maintenanceHoldSec = 2;
  assert(std::string_view(m.tick(sample(90, -100, 24000), p, 4, 24000).state) ==
         "HOLD");
  assert(m.tick(sample(90, -100, 27000), p, 4, 27000).strategy ==
         Strategy::Native);
  auto faulty = sample(85, -100, 30000);
  m.tick(faulty, p, 5, 30000);
  faulty.readHalted = true;
  assert(m.tick(faulty, p, 5, 31000).strategy == Strategy::Safe);
  d = m.tick(sample(85, -100, 32000), p, 5, 32000);
  assert(d.strategy == Strategy::Native &&
         std::string_view(d.transition) == "MAINTENANCE_FAULT_ABORT");
  assert(!d.executed);
  Policy limits;
  limits.discharge = 800;
  limits.dischargeA = 8;
  auto power = sample(50, 600, 33000);
  power.batteryVoltage = {50, 33000, Quality::Valid};
  assert(m.tick(power, limits, 6, 33000).finalWatts == -400);
  power.batteryVoltage.status = Quality::Missing;
  assert(m.tick(power, limits, 6, 33000).finalWatts == 0);
  limits.dischargeA = 0;
  limits.chargeA = 0;
  limits.exportLimit = 200;
  power.grid.value = -500;
  assert(m.tick(power, limits, 7, 33000).lower == 300);
  Policy daily;
  daily.strategy = 4;
  StrategyManager scheduler;
  tm time{};
  time.tm_year = 126;
  time.tm_yday = 320;
  time.tm_hour = 15;
  time.tm_min = 29;
  assert(scheduler.tick(sample(50, -2000, 40000), daily, 1, 40000, &time)
             .strategy == Strategy::Target);
  time.tm_min = 30;
  assert(scheduler.tick(sample(50, -2000, 41000), daily, 1, 41000, &time)
             .strategy == Strategy::SelfUse);
  time.tm_yday = 321;
  time.tm_hour = 8;
  time.tm_min = 0;
  assert(scheduler.tick(sample(50, -2000, 42000), daily, 1, 42000, &time)
             .strategy == Strategy::Target);
  time.tm_yday = 320;
  assert(scheduler.tick(sample(50, -2000, 43000), daily, 1, 43000, &time)
             .strategy == Strategy::SelfUse);
  time.tm_yday = 321;
  assert(scheduler.tick(sample(50, -2000, 44000), daily, 1, 44000, &time)
             .strategy == Strategy::SelfUse);
  time.tm_yday = 322;
  assert(scheduler.tick(sample(50, -2000, 45000), daily, 1, 45000, &time, 3600)
             .strategy == Strategy::Target);

  StrategyManager lateClock;
  daily.deadlineSec = 2;
  assert(lateClock.tick(sample(50, -2000, 1000), daily, 1, 1000).strategy ==
         Strategy::Target);
  assert(lateClock.tick(sample(50, -2000, 3000), daily, 1, 3000).strategy ==
         Strategy::SelfUse);
  assert(lateClock.tick(sample(50, -2000, 4000), daily, 1, 4000, &time, 3600)
             .strategy == Strategy::SelfUse);
  p.ceiling = 10;
  assert(!p.valid());

  // === Predictive & Seasonal EMS tests ===
  Policy pred;
  pred.predictive = 1;
  pred.autoSeason = 1;
  pred.summerFloor = 10;
  pred.winterFloor = 20;
  pred.charge = 3000;
  pred.chargeFirstW = 3000;
  pred.targetW = 3000;
  pred.chargeA = 50;
  pred.dischargeA = 50;
  pred.fullHoldSec = 300;
  pred.ramp = 5000; // fast step in unit tests
  assert(pred.valid());

  tm winterDay{};
  winterDay.tm_year = 126;
  winterDay.tm_mon = 10;
  winterDay.tm_mday = 19; // Nov 19
  winterDay.tm_wday = 4;
  winterDay.tm_hour = 10;
  winterDay.tm_min = 10;

  tm summerDay{};
  summerDay.tm_year = 126;
  summerDay.tm_mon = 6;
  summerDay.tm_mday = 15; // Jul 15
  summerDay.tm_wday = 3;
  summerDay.tm_hour = 10;
  summerDay.tm_min = 10;

  // 1. Sunny day: gentle charging modulates amps below 50A to hit 15:30 target
  StrategyManager gentleMgr;
  PredictiveInputs sunny{};
  sunny.forecastValid = true;
  sunny.remainingForecastKwh = 16.0;
  sunny.todayTotalKwh = 18.0;
  sunny.tomorrowTotalKwh = 16.0;
  sunny.loadProfileValid = true;
  sunny.loadTodayKw = 0.6;
  auto eSunny = sample(40, -2500, 10000);
  eSunny.batteryVoltage = {52.0, 10000, Quality::Valid};
  auto dSunny =
      gentleMgr.tick(eSunny, pred, 1, 10000, &summerDay, std::nullopt, &sunny);
  assert(dSunny.strategy == Strategy::Target);
  assert(dSunny.effectiveFloor == 10);
  assert(!dSunny.winterSeason);
  assert(dSunny.recommendedChargeA > 0 && dSunny.recommendedChargeA < 50);
  assert(dSunny.finalWatts.has_value() && *dSunny.finalWatts < 2500);

  // 2. Rainy/foggy multi-day: captures 100% surplus PV up to 50A and blocks
  // discharge at night
  StrategyManager rainyMgr;
  PredictiveInputs rainy{};
  rainy.forecastValid = true;
  rainy.remainingForecastKwh = 2.5;
  rainy.todayTotalKwh = 4.77;
  rainy.tomorrowTotalKwh = 5.0;
  rainy.weatherValid = true;
  rainy.fog = true;
  auto eRainy = sample(25, -1800, 20000);
  eRainy.batteryVoltage = {52.0, 20000, Quality::Valid};
  auto dRainy =
      rainyMgr.tick(eRainy, pred, 1, 20000, &winterDay, std::nullopt, &rainy);
  assert(dRainy.strategy == Strategy::ChargeFirst);
  assert(dRainy.effectiveFloor == 20);
  assert(dRainy.winterSeason);
  assert(dRainy.lowSunMode);
  assert(dRainy.lower == 0);
  assert(dRainy.recommendedChargeA == 50);
  assert(dRainy.finalWatts == 1800);
  assert(std::string_view(dRainy.safetyReason) == "NONE");

  // Small cloud drop < deadband (1800 -> 1790W) must NOT emit false POWER_LIMIT
  // warning
  auto eCloud = sample(25, -1790, 21000);
  eCloud.batteryVoltage = {52.0, 21000, Quality::Valid};
  auto dCloud =
      rainyMgr.tick(eCloud, pred, 1, 21000, &winterDay, std::nullopt, &rainy);
  assert(dCloud.finalWatts == 1790);
  assert(std::string_view(dCloud.safetyReason) == "NONE");
  assert(std::string_view(dCloud.reason) == "CAPTURE_CHARGE_FIRST");

  // Night cloud/load during rainy ChargeFirst: zero discharge, protects state
  auto eNight = sample(25, 500, 25000);
  eNight.batteryVoltage = {52.0, 25000, Quality::Valid};
  auto dNight =
      rainyMgr.tick(eNight, pred, 1, 25000, &winterDay, std::nullopt, &rainy);
  assert(dNight.strategy == Strategy::ChargeFirst);
  assert(dNight.lower == 0 && dNight.finalWatts == 0);
  assert(std::string_view(dNight.disposition) == "NO_PV_SURPLUS");

  // Decision disposition matrix: cancelled, partially limited and directly
  // applicable candidates.
  Policy noGrid = pred;
  noGrid.predictive = 0;
  noGrid.strategy = 1;
  noGrid.surplusOnly = 0;
  noGrid.gridCharge = 0;
  noGrid.charge = 2560;
  noGrid.chargeFirstW = 2560;
  noGrid.chargeA = 0;
  noGrid.ramp = 20000;
  StrategyManager noGridMgr;
  auto dNoGrid = noGridMgr.tick(sample(50, 500, 30000), noGrid, 1, 30000);
  assert(dNoGrid.candidate == 2560 && dNoGrid.limited == 0 &&
         dNoGrid.finalWatts == 0);
  assert(std::string_view(dNoGrid.disposition) == "GRID_CHARGE_DISABLED");

  Policy partial = noGrid;
  partial.surplusOnly = 1;
  StrategyManager partialMgr;
  auto dPartial = partialMgr.tick(sample(50, -1200, 31000), partial, 1, 31000);
  assert(dPartial.candidate == 2560 && dPartial.limited == 1200 &&
         dPartial.finalWatts == 1200);
  assert(std::string_view(dPartial.disposition) == "PARTIAL_PV_SURPLUS");

  Policy applicable = partial;
  applicable.gridCharge = 1;
  applicable.surplusOnly = 0;
  StrategyManager applicableMgr;
  auto dApplicable =
      applicableMgr.tick(sample(50, 500, 32000), applicable, 1, 32000);
  assert(dApplicable.candidate == 2560 && dApplicable.limited == 2560 &&
         dApplicable.finalWatts == 2560);
  assert(std::string_view(dApplicable.disposition) == "APPLICABLE");

  Policy safety = applicable;
  safety.strategy = 0;
  safety.floor = 20;
  safety.resumeSoc = 25;
  safety.minVolt = 500;
  safety.discharge = 3000;
  StrategyManager safetyMgr;
  auto dSafety = safetyMgr.tick(sample(15, 800, 33000), safety, 1, 33000);
  assert(dSafety.candidate < 0 && dSafety.finalWatts == 0);
  assert(std::string_view(dSafety.disposition) == "SAFETY_LIMIT");

  EnergyState unavailable = sample(50, 500, 34000);
  unavailable.grid.status = Quality::Stale;
  StrategyManager unavailableMgr;
  auto dUnavailable = unavailableMgr.tick(unavailable, applicable, 1, 34000);
  assert(!dUnavailable.candidate.has_value() &&
         !dUnavailable.finalWatts.has_value());
  assert(std::string_view(dUnavailable.disposition) == "INPUT_UNAVAILABLE");

  // 3 & 4. Closed loop: CHARGE_FIRST -> 100% held for 5 min (300s) -> SELF_USE
  // down to 20% -> CHARGE_FIRST
  StrategyManager loopMgr;
  uint32_t t = 100000;
  auto dLoop = loopMgr.tick(sample(25, -2000, t), pred, 1, t, &winterDay,
                            std::nullopt, &rainy);
  assert(dLoop.strategy == Strategy::ChargeFirst);

  // Reach 100%, start 5-minute hold
  t += 4000;
  dLoop = loopMgr.tick(sample(100, -500, t), pred, 1, t, &winterDay,
                       std::nullopt, &rainy);
  assert(dLoop.strategy == Strategy::ChargeFirst);
  assert(std::string_view(dLoop.phase) == "FULL_HOLD_5M");

  // Advance 296s in 4s steps (still < 300s): must remain in ChargeFirst
  for (int i = 0; i < 74; ++i) {
    t += 4000;
    dLoop = loopMgr.tick(sample(100, -200, t), pred, 1, t, &winterDay,
                         std::nullopt, &rainy);
    assert(dLoop.strategy == Strategy::ChargeFirst);
  }
  // At 300s+: transitions to SelfUse
  t += 4000;
  dLoop = loopMgr.tick(sample(100, 600, t), pred, 1, t, &winterDay,
                       std::nullopt, &rainy);
  assert(dLoop.strategy == Strategy::SelfUse);
  assert(std::string_view(dLoop.reason) == "FULL_SOC_5MIN_COMPLETE");

  // Discharging through 60% and 25%: remains SelfUse and allows discharge
  t += 4000;
  dLoop = loopMgr.tick(sample(60, 600, t), pred, 1, t, &winterDay, std::nullopt,
                       &rainy);
  assert(dLoop.strategy == Strategy::SelfUse && dLoop.lower < 0 &&
         dLoop.finalWatts == -600);
  t += 4000;
  dLoop = loopMgr.tick(sample(25, 600, t), pred, 1, t, &winterDay, std::nullopt,
                       &rainy);
  assert(dLoop.strategy == Strategy::SelfUse && dLoop.finalWatts == -600);

  // Hits winter minimum 20%: blocks discharge and loops back to ChargeFirst!
  t += 4000;
  dLoop = loopMgr.tick(sample(20, 600, t), pred, 1, t, &winterDay, std::nullopt,
                       &rainy);
  assert(dLoop.strategy == Strategy::ChargeFirst);
  assert(dLoop.lower == 0);

  // === Regression tests for Audit v5.1-console-7 ===
  // R1: Sunny day 15:15 (between deadlineMinute-anticipationMin and
  // deadlineMinute, SOC=95%) stays in GentleCharge RECOVERY (not ChargeFirst)
  StrategyManager winMgr;
  tm sunnyWin = summerDay;
  sunnyWin.tm_hour = 14;
  sunnyWin.tm_min = 0;
  PredictiveInputs lateSunny = sunny;
  lateSunny.remainingForecastKwh = 3.0;
  auto dR1 = winMgr.tick(sample(85, -1200, 10000), pred, 1, 10000, &sunnyWin,
                         std::nullopt, &lateSunny);
  assert(dR1.strategy == Strategy::Target);
  sunnyWin.tm_hour = 15;
  sunnyWin.tm_min = 15;
  lateSunny.remainingForecastKwh = 0.2;
  dR1 = winMgr.tick(sample(95, -400, 14000), pred, 1, 14000, &sunnyWin,
                    std::nullopt, &lateSunny);
  assert(dR1.strategy == Strategy::Target &&
         std::string_view(dR1.state) == "RECOVERY");

  // R2: Sunny day 15:30 transitions to SelfUse allowing discharge (lower < 0)
  sunnyWin.tm_hour = 15;
  sunnyWin.tm_min = 30;
  auto dR2 = winMgr.tick(sample(95, 500, 18000), pred, 1, 18000, &sunnyWin,
                         std::nullopt, &lateSunny);
  assert(dR2.strategy == Strategy::SelfUse && dR2.lower.has_value() &&
         *dR2.lower < 0);

  // R3: Next sunny morning (day+1, 09:00, SOC=55%) resumes GentleCharge
  // automatically
  sunnyWin.tm_yday += 1;
  sunnyWin.tm_mday += 1;
  sunnyWin.tm_hour = 9;
  sunnyWin.tm_min = 0;
  auto dR3 = winMgr.tick(sample(55, -1800, 22000), pred, 1, 22000, &sunnyWin,
                         std::nullopt, &sunny);
  assert(dR3.strategy == Strategy::Target);

  // R4: Winter sunny morning (Nov 19, SOC=25%, today=16 kWh, tomorrow=15 kWh)
  // enters GentleCharge (< 50A)
  StrategyManager winSunnyMgr;
  PredictiveInputs winSunny = sunny;
  winSunny.todayTotalKwh = 16.0;
  winSunny.tomorrowTotalKwh = 15.0;
  winSunny.remainingForecastKwh = 15.0;
  auto eWin25 = sample(25, -2400, 30000);
  eWin25.batteryVoltage = {52.0, 30000, Quality::Valid};
  auto dR4 = winSunnyMgr.tick(eWin25, pred, 1, 30000, &winterDay, std::nullopt,
                              &winSunny);
  assert(dR4.winterSeason && !dR4.lowSunMode &&
         dR4.strategy == Strategy::Target && dR4.recommendedChargeA > 0 &&
         dR4.recommendedChargeA < 50);

  // R5: Winter sunny today + rainy tomorrow (< lowSunKwh) enters
  // CaptureChargeFirst
  StrategyManager winMixMgr;
  PredictiveInputs winMix = winSunny;
  winMix.tomorrowTotalKwh = 3.0;
  auto dR5 =
      winMixMgr.tick(eWin25, pred, 1, 30000, &winterDay, std::nullopt, &winMix);
  assert(dR5.winterSeason && dR5.lowSunMode &&
         dR5.strategy == Strategy::ChargeFirst);

  // R6: resumeSoc=25 with floor=20 honors 10%/15% in summer and 20%/25% in
  // winter
  Policy predRes = pred;
  predRes.floor = 20;
  predRes.resumeSoc = 25;
  predRes.hysteresis = 2;
  predRes.predictive = 0;
  StrategyManager resMgr;
  assert(resMgr.tick(sample(20, 400, 1000), predRes, 1, 1000).finalWatts == 0);
  assert(resMgr.tick(sample(25, 400, 2000), predRes, 1, 2000).finalWatts == 0);
  assert(resMgr.tick(sample(26, 400, 3000), predRes, 1, 3000).finalWatts ==
         -200);
  predRes.predictive = 1;
  StrategyManager sumResMgr;
  tm sumEvening = summerDay;
  sumEvening.tm_hour = 17;
  sumEvening.tm_min = 0;
  sumResMgr.tick(sample(30, 400, 1000), predRes, 1, 1000, &sumEvening,
                 std::nullopt, &sunny);
  auto dR6s = sumResMgr.tick(sample(14, 400, 2000), predRes, 1, 2000,
                             &sumEvening, std::nullopt, &sunny);
  assert(dR6s.effectiveFloor == 10 && dR6s.strategy == Strategy::SelfUse &&
         *dR6s.lower < 0);
  tm winEvening = winterDay;
  winEvening.tm_hour = 17;
  winEvening.tm_min = 0;
  sumResMgr.tick(sample(30, 400, 3000), predRes, 2, 3000, &winEvening,
                 std::nullopt, &winSunny);
  auto dR6w = sumResMgr.tick(sample(20, 400, 4000), predRes, 2, 4000,
                             &winEvening, std::nullopt, &winSunny);
  assert(dR6w.effectiveFloor == 20 && *dR6w.lower == 0);

  // R7: fogSticky_ clears on midnight rollover AND after 1h of expired weather
  StrategyManager fogMgr;
  PredictiveInputs clearWeather{};
  clearWeather.forecastValid = true;
  clearWeather.todayTotalKwh = 15.0;
  clearWeather.tomorrowTotalKwh = 15.0;
  clearWeather.remainingForecastKwh = 14.0;
  clearWeather.weatherValid = true;
  clearWeather.fog = true;
  auto dFog = fogMgr.tick(sample(35, -1800, 10000), pred, 1, 10000, &winterDay,
                          std::nullopt, &clearWeather);
  assert(dFog.fogActive && dFog.strategy == Strategy::ChargeFirst);
  clearWeather.weatherValid = false;
  dFog = fogMgr.tick(sample(35, -1800, 10000 + 3600000), pred, 1,
                     10000 + 3600000, &winterDay, std::nullopt, &clearWeather);
  assert(!dFog.fogActive && dFog.strategy == Strategy::Target);
  // Rollover test
  clearWeather.weatherValid = true;
  clearWeather.fog = true;
  dFog = fogMgr.tick(sample(35, -1800, 20000), pred, 2, 20000, &winterDay,
                     std::nullopt, &clearWeather);
  assert(dFog.fogActive);
  tm nextWinterDay = winterDay;
  nextWinterDay.tm_yday += 1;
  nextWinterDay.tm_mday += 1;
  clearWeather.fog = false;
  clearWeather.spreadC = 2.0;
  clearWeather.radiationWm2 = 150.0;
  dFog = fogMgr.tick(sample(35, -1800, 25000), pred, 2, 25000, &nextWinterDay,
                     std::nullopt, &clearWeather);
  assert(!dFog.fogActive);

  // R8: Stale telemetry during FullHold resets fullHoldAccumMs_
  StrategyManager holdMgr;
  uint32_t th = 10000;
  holdMgr.tick(sample(100, -200, th), pred, 1, th, &winterDay, std::nullopt,
               &rainy);
  th += 4000;
  holdMgr.tick(sample(100, -200, th), pred, 1, th, &winterDay, std::nullopt,
               &rainy);
  th += 20000; // stale telemetry gap (> staleMs)
  auto eStale = sample(100, -200, 10000);
  auto dStale =
      holdMgr.tick(eStale, pred, 1, th, &winterDay, std::nullopt, &rainy);
  assert(dStale.strategy == Strategy::Safe);
  // Resume 295s more of valid 100% hold: must NOT complete early from prior
  // partial hold
  for (int i = 0; i < 73; ++i) {
    th += 4000;
    auto dHold = holdMgr.tick(sample(100, -200, th), pred, 1, th, &winterDay,
                              std::nullopt, &rainy);
    assert(dHold.strategy == Strategy::ChargeFirst);
  }

  // R9: exportPredictiveState / restorePredictiveState preserves SelfUse
  // discharge cycle across reboot on rainy day
  StrategyManager bootMgr1;
  uint32_t tb = 50000;
  bootMgr1.tick(sample(100, -200, tb), pred, 1, tb, &winterDay, std::nullopt,
                &rainy);
  for (int i = 0; i < 76; ++i) {
    tb += 4000;
    bootMgr1.tick(sample(100, 500, tb), pred, 1, tb, &winterDay, std::nullopt,
                  &rainy);
  }
  const auto savedSt = bootMgr1.exportPredictiveState();
  assert(savedSt.discharging == 1);
  StrategyManager bootMgr2;
  bootMgr2.restorePredictiveState(savedSt);
  auto dReboot = bootMgr2.tick(sample(50, 500, 1000), pred, 1, 1000, &winterDay,
                               std::nullopt, &rainy);
  assert(dReboot.strategy == Strategy::SelfUse && dReboot.finalWatts == -500);
  assert(std::string_view(dReboot.disposition) == "APPLICABLE");

  // R10: exportPredictiveState / restorePredictiveState preserves FullHold across reboot
  StrategyManager holdBoot1;
  holdBoot1.tick(sample(100, -200, 10000), pred, 1, 10000, &winterDay,
                 std::nullopt, &rainy);
  holdBoot1.tick(sample(100, -200, 14000), pred, 1, 14000, &winterDay,
                 std::nullopt, &rainy);
  const auto savedHold = holdBoot1.exportPredictiveState();
  assert(savedHold.phase == static_cast<uint8_t>(StrategyManager::CyclePhase::FullHold));
  StrategyManager holdBoot2;
  holdBoot2.restorePredictiveState(savedHold);
  auto dHoldReboot = holdBoot2.tick(sample(100, -200, 1000), pred, 1, 1000,
                                    &winterDay, std::nullopt, &rainy);
  assert(dHoldReboot.strategy == Strategy::ChargeFirst);
  assert(std::string_view(dHoldReboot.phase) == "FULL_HOLD_5M");
  assert(dHoldReboot.lower == 0);
  assert(std::string_view(dHoldReboot.disposition) == "SAFETY_LIMIT");

  // R11: Secondary threshold edits (same predictive=1 & strategy=0) preserve active phase
  Policy predEdited = pred;
  predEdited.lowSunKwh = 10;
  predEdited.deadband = 30;
  predEdited.anticipationMin = 25;
  auto dSecEdit = bootMgr2.tick(sample(60, 600, 4000), predEdited, 2, 4000,
                                &winterDay, std::nullopt, &rainy);
  assert(dSecEdit.strategy == Strategy::SelfUse && dSecEdit.finalWatts == -600);
  assert(std::string_view(dSecEdit.disposition) == "APPLICABLE");

  // R12: Effective discharge_w = 2560W (exact & capped on 3000W load)
  Policy disPol = pred;
  disPol.predictive = 0;
  disPol.strategy = 0;
  disPol.discharge = 2560;
  disPol.dischargeA = 50;
  disPol.ramp = 20000;
  StrategyManager disMgr1;
  auto eDis2560 = sample(60, 2560, 1000);
  eDis2560.batteryVoltage = {52.0, 1000, Quality::Valid};
  auto dDisExact = disMgr1.tick(eDis2560, disPol, 1, 1000);
  assert(dDisExact.candidate == -2560 && dDisExact.limited == -2560 &&
         dDisExact.finalWatts == -2560);
  assert(std::string_view(dDisExact.disposition) == "APPLICABLE");

  StrategyManager disMgr2;
  auto eDis3000 = sample(60, 3000, 1000);
  eDis3000.batteryVoltage = {52.0, 1000, Quality::Valid};
  auto dDisCap = disMgr2.tick(eDis3000, disPol, 1, 1000);
  assert(dDisCap.candidate == -3000 && dDisCap.limited == -2560 &&
         dDisCap.finalWatts == -2560 && dDisCap.lower == -2560);
  assert(std::string_view(dDisCap.disposition) == "PARTIAL_POWER_LIMIT");

  // Damping K=0.5 + +/-50W deadband in SelfUse and asymmetric ChargeFirst surplusOnly
  Policy dampPol;
  dampPol.strategy = 0;
  dampPol.predictive = 0;
  dampPol.ramp = 2000;
  dampPol.deadband = 20;
  StrategyManager dampSelf;
  assert(dampSelf.tick(sample(50, -1000, 1000), dampPol, 1, 1000).finalWatts == 1000);
  // Load step on: surplus drops to 600 -> d.upper clamps immediately to 600 (no grid charge)
  EnergyState eLoad = sample(50, 400, 2000);
  eLoad.battery = {1000.0, 2000, Quality::Valid};
  assert(dampSelf.tick(eLoad, dampPol, 1, 2000).finalWatts == 600);
  // Within +/-50W deadband (surplus=640 >= 600): holds 600
  EnergyState eSmall = sample(50, -40, 3000);
  eSmall.battery = {600.0, 3000, Quality::Valid};
  assert(dampSelf.tick(eSmall, dampPol, 1, 3000).finalWatts == 600);
  // Surplus rises to 1000: damped K=0.5 -> 600 + (1000-600)/2 = 800
  EnergyState eRise = sample(50, -400, 4000);
  eRise.battery = {600.0, 4000, Quality::Valid};
  assert(dampSelf.tick(eRise, dampPol, 1, 4000).finalWatts == 800);

  // ChargeFirst (ordinary) surplusOnly: immediate drop on load spike, damped rise on load drop
  dampPol.strategy = 1;
  dampPol.surplusOnly = 1;
  dampPol.chargeFirstW = 2560;
  dampPol.charge = 2560;
  StrategyManager dampCf;
  assert(dampCf.tick(sample(50, -1500, 1000), dampPol, 1, 1000).finalWatts == 1500);
  // Surplus drops to 600: immediate drop to 600
  assert(dampCf.tick(sample(50, -600, 2000), dampPol, 1, 2000).finalWatts == 600);
  // Surplus jumps back to 1600: damped K=0.5 rise -> 600 + (1600-600)/2 = 1100
  assert(dampCf.tick(sample(50, -1600, 3000), dampPol, 1, 3000).finalWatts == 1100);

  // ChargeFirst with surplusOnly=0, gridCharge=0: uses full pv_w during day, 0 at night
  Policy cfPvPol;
  cfPvPol.strategy = 1;
  cfPvPol.surplusOnly = 0;
  cfPvPol.gridCharge = 0;
  cfPvPol.charge = 2560;
  cfPvPol.chargeFirstW = 2560;
  StrategyManager cfPvMgr;
  EnergyState eDayPv = sample(50, -1400, 1000);
  eDayPv.pv = {2400.0, 1000, Quality::Valid};
  assert(cfPvMgr.tick(eDayPv, cfPvPol, 1, 1000).finalWatts == 2400);
  EnergyState eNightPv = sample(50, 400, 2000);
  eNightPv.pv = {0.0, 2000, Quality::Valid};
  assert(cfPvMgr.tick(eNightPv, cfPvPol, 1, 2000).finalWatts == 0);

  // Ensure every checked Decision has an explicit non-placeholder disposition
  for (const auto *disp :
       {dSunny.disposition, dRainy.disposition, dNight.disposition,
        dNoGrid.disposition, dPartial.disposition, dApplicable.disposition,
        dSafety.disposition, dUnavailable.disposition, dReboot.disposition,
        dHoldReboot.disposition, dSecEdit.disposition, dDisExact.disposition,
        dDisCap.disposition}) {
    const std::string_view sv(disp);
    assert(!sv.empty() && sv != "NONE" && sv != "NOT_EVALUATED");
  }
}
