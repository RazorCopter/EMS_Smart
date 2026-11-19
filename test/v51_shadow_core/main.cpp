// SPDX-License-Identifier: MIT
#include "v51/shadow_core.h"
#include <cassert>
#include <string_view>
using namespace heliograph::v51;
EnergyState sample(double soc,double grid,int ms) {
    EnergyState e;
    e.goodReads=1;
    e.batteryVoltage={51.2,uint32_t(ms),Quality::Valid};
    e.soc={soc,uint32_t(ms),Quality::Valid};
    e.grid={grid,uint32_t(ms),Quality::Valid};
    e.battery={0,uint32_t(ms),Quality::Valid};
    return e;
}
int main() {
    StrategyManager m;
    Policy p;
    assert(p.target==100 && p.capacityWh==10240 && p.nominalMv==51200 && p.valid());
    p.target=80;
    assert(m.tick(sample(50,600,1000),p,1,1000).finalWatts == -600);
    assert(m.tick(sample(20,600,2000),p,1,2000).finalWatts == 0);
    assert(m.tick(sample(50,500,1000),p,1,8000).strategy==Strategy::Safe);
    p.strategy=1;
    assert(m.tick(sample(50,-1200,9000),p,2,9000).finalWatts==1000);
    assert(m.tick(sample(79,-1200,10000),p,2,10000).candidate==250);
    assert(m.tick(sample(77,-1200,10500),p,2,10500).candidate==250);
    assert(m.tick(sample(75,-1200,10700),p,2,10700).candidate==1000);
    assert(m.tick(sample(80,-1200,11000),p,2,11000).strategy==Strategy::SelfUse);
    p.strategy=4; p.fallback=2; p.deadlineSec=10;
    auto d=m.tick(sample(50,-100,12000),p,3,12000);
    assert(d.unreachable && d.missingWh && d.averageW && d.finalWatts==100);
    assert(m.tick(sample(50,-100,23000),p,3,23000).strategy==Strategy::Native);
    p.strategy=5; p.maintenanceHoldSec=2;
    assert(std::string_view(m.tick(sample(90,-100,24000),p,4,24000).state)=="HOLD");
    assert(m.tick(sample(90,-100,27000),p,4,27000).strategy==Strategy::Native);
    auto faulty=sample(85,-100,30000);
    m.tick(faulty,p,5,30000);
    faulty.readHalted=true;
    assert(m.tick(faulty,p,5,31000).strategy==Strategy::Safe);
    d=m.tick(sample(85,-100,32000),p,5,32000);
    assert(d.strategy==Strategy::Native && std::string_view(d.transition)=="MAINTENANCE_FAULT_ABORT");
    assert(!d.executed);
    Policy limits;
    limits.discharge=800;
    limits.dischargeA=8;
    auto power=sample(50,600,33000);
    power.batteryVoltage={50,33000,Quality::Valid};
    assert(m.tick(power,limits,6,33000).finalWatts==-400);
    power.batteryVoltage.status=Quality::Missing;
    assert(m.tick(power,limits,6,33000).finalWatts==0);
    limits.dischargeA=0;
    limits.chargeA=0;
    limits.exportLimit=200;
    power.grid.value=-500;
    assert(m.tick(power,limits,7,33000).lower==300);
    Policy daily;
    daily.strategy=4;
    StrategyManager scheduler;
    tm time{};
    time.tm_year=126; time.tm_yday=320; time.tm_hour=15; time.tm_min=29;
    assert(scheduler.tick(sample(50,-2000,40000),daily,1,40000,&time).strategy==Strategy::Target);
    time.tm_min=30;
    assert(scheduler.tick(sample(50,-2000,41000),daily,1,41000,&time).strategy==Strategy::SelfUse);
    time.tm_yday=321; time.tm_hour=8; time.tm_min=0;
    assert(scheduler.tick(sample(50,-2000,42000),daily,1,42000,&time).strategy==Strategy::Target);
    time.tm_yday=320;
    assert(scheduler.tick(sample(50,-2000,43000),daily,1,43000,&time).strategy==Strategy::SelfUse);
    time.tm_yday=321;
    assert(scheduler.tick(sample(50,-2000,44000),daily,1,44000,&time).strategy==Strategy::SelfUse);
    time.tm_yday=322;
    assert(scheduler.tick(sample(50,-2000,45000),daily,1,45000,&time,3600).strategy==Strategy::Target);

    StrategyManager lateClock;
    daily.deadlineSec=2;
    assert(lateClock.tick(sample(50,-2000,1000),daily,1,1000).strategy==Strategy::Target);
    assert(lateClock.tick(sample(50,-2000,3000),daily,1,3000).strategy==Strategy::SelfUse);
    assert(lateClock.tick(sample(50,-2000,4000),daily,1,4000,&time,3600).strategy==Strategy::SelfUse);
    p.ceiling=10;
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
    winterDay.tm_year = 126; winterDay.tm_mon = 10; winterDay.tm_mday = 19; // Nov 19
    winterDay.tm_wday = 4; winterDay.tm_hour = 10; winterDay.tm_min = 10;

    tm summerDay{};
    summerDay.tm_year = 126; summerDay.tm_mon = 6; summerDay.tm_mday = 15; // Jul 15
    summerDay.tm_wday = 3; summerDay.tm_hour = 10; summerDay.tm_min = 10;

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
    auto dSunny = gentleMgr.tick(eSunny, pred, 1, 10000, &summerDay, std::nullopt, &sunny);
    assert(dSunny.strategy == Strategy::Target);
    assert(dSunny.effectiveFloor == 10);
    assert(!dSunny.winterSeason);
    assert(dSunny.recommendedChargeA > 0 && dSunny.recommendedChargeA < 50);
    assert(dSunny.finalWatts.has_value() && *dSunny.finalWatts < 2500);

    // 2. Rainy/foggy multi-day: captures 100% surplus PV up to 50A and blocks discharge at night
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
    auto dRainy = rainyMgr.tick(eRainy, pred, 1, 20000, &winterDay, std::nullopt, &rainy);
    assert(dRainy.strategy == Strategy::ChargeFirst);
    assert(dRainy.effectiveFloor == 20);
    assert(dRainy.winterSeason);
    assert(dRainy.lowSunMode);
    assert(dRainy.lower == 0);
    assert(dRainy.recommendedChargeA == 50);
    assert(dRainy.finalWatts == 1800);
    assert(std::string_view(dRainy.safetyReason) == "NONE");

    // Small cloud drop < deadband (1800 -> 1790W) must NOT emit false POWER_LIMIT warning
    auto eCloud = sample(25, -1790, 21000);
    eCloud.batteryVoltage = {52.0, 21000, Quality::Valid};
    auto dCloud = rainyMgr.tick(eCloud, pred, 1, 21000, &winterDay, std::nullopt, &rainy);
    assert(dCloud.finalWatts == 1790);
    assert(std::string_view(dCloud.safetyReason) == "NONE");
    assert(std::string_view(dCloud.reason) == "CAPTURE_CHARGE_FIRST");

    // Night cloud/load during rainy ChargeFirst: zero discharge, protects state
    auto eNight = sample(25, 500, 25000);
    eNight.batteryVoltage = {52.0, 25000, Quality::Valid};
    auto dNight = rainyMgr.tick(eNight, pred, 1, 25000, &winterDay, std::nullopt, &rainy);
    assert(dNight.strategy == Strategy::ChargeFirst);
    assert(dNight.lower == 0 && dNight.finalWatts == 0);

    // 3 & 4. Closed loop: CHARGE_FIRST -> 100% held for 5 min (300s) -> SELF_USE down to 20% -> CHARGE_FIRST
    StrategyManager loopMgr;
    uint32_t t = 100000;
    auto dLoop = loopMgr.tick(sample(25, -2000, t), pred, 1, t, &winterDay, std::nullopt, &rainy);
    assert(dLoop.strategy == Strategy::ChargeFirst);

    // Reach 100%, start 5-minute hold
    t += 4000;
    dLoop = loopMgr.tick(sample(100, -500, t), pred, 1, t, &winterDay, std::nullopt, &rainy);
    assert(dLoop.strategy == Strategy::ChargeFirst);
    assert(std::string_view(dLoop.phase) == "FULL_HOLD_5M");

    // Advance 296s in 4s steps (still < 300s): must remain in ChargeFirst
    for (int i = 0; i < 74; ++i) {
        t += 4000;
        dLoop = loopMgr.tick(sample(100, -200, t), pred, 1, t, &winterDay, std::nullopt, &rainy);
        assert(dLoop.strategy == Strategy::ChargeFirst);
    }
    // At 300s+: transitions to SelfUse
    t += 4000;
    dLoop = loopMgr.tick(sample(100, 600, t), pred, 1, t, &winterDay, std::nullopt, &rainy);
    assert(dLoop.strategy == Strategy::SelfUse);
    assert(std::string_view(dLoop.reason) == "FULL_SOC_5MIN_COMPLETE");

    // Discharging through 60% and 25%: remains SelfUse and allows discharge
    t += 4000;
    dLoop = loopMgr.tick(sample(60, 600, t), pred, 1, t, &winterDay, std::nullopt, &rainy);
    assert(dLoop.strategy == Strategy::SelfUse && dLoop.lower < 0 && dLoop.finalWatts == -600);
    t += 4000;
    dLoop = loopMgr.tick(sample(25, 600, t), pred, 1, t, &winterDay, std::nullopt, &rainy);
    assert(dLoop.strategy == Strategy::SelfUse && dLoop.finalWatts == -600);

    // Hits winter minimum 20%: blocks discharge and loops back to ChargeFirst!
    t += 4000;
    dLoop = loopMgr.tick(sample(20, 600, t), pred, 1, t, &winterDay, std::nullopt, &rainy);
    assert(dLoop.strategy == Strategy::ChargeFirst);
    assert(dLoop.lower == 0);
}
