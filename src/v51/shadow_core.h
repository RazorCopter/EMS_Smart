// SPDX-License-Identifier: MIT
#pragma once
#include "energy_state.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <ctime>
#include <string_view>

namespace heliograph::v51 {
enum class Strategy { SelfUse, ChargeFirst, Native, Safe, Target, Maintenance };
inline constexpr const char* names[] = {"SELF_USE","CHARGE_FIRST","NATIVE_INVERTER","SAFE_MODE","TARGET_SOC","MAINTENANCE"};
inline const char* strategyName(Strategy s) { return names[static_cast<int>(s)]; }

struct PredictiveInputs {
    bool forecastValid = false;
    double remainingForecastKwh = 0.0;
    double todayTotalKwh = 0.0;
    double tomorrowTotalKwh = 0.0;
    bool ratioValid = false;
    double ratio = 1.0;
    bool weatherValid = false;
    bool fog = false;
    double spreadC = 99.0;
    double radiationWm2 = 0.0;
    bool loadProfileValid = false;
    double loadTodayKw = 0.0;
    double loadTomorrowKw = 0.0;
};

struct Policy {
    int strategy=0, fallback=0, floor=20, ceiling=100, target=100;
    int charge=1000, discharge=1000, deadband=20, ramp=250, staleMs=5000;
    int chargeFirstW=1000, surplusOnly=1, gridCharge=0, hysteresis=2, chargeFirstSec=7200;
    int deadlineSec=14400, capacityWh=10240, efficiencyPct=90, targetW=1000;
    int chargeA=50,dischargeA=50,minVolt=0,maxVolt=0,resumeSoc=20,gridTarget=0,exportLimit=3300;
    int maintenanceSoc=90, maintenanceHoldSec=600, maintenanceSec=14400;
    int nominalMv=51200, daily=1, deadlineMinute=930;
    // Predictive & seasonal extensions (v5.1-predictive)
    int predictive=0, autoSeason=1, summerFloor=10, winterFloor=20, fullHoldSec=300;
    int weekdayLoadW=600, weekendLoadW=900, winterStart=1025, winterEnd=228;
    int lowSunKwh=8, anticipationMin=20, safetyMarginPct=115;

    int dampingW = 50, dampingPct = 50;

    int trainerSettleMs = 3000;
    int trainerAbsW = 60;
    int trainerPct = 15;
    int trainerTimeoutMs = 10000;
    int trainerGapMs = 20000;
    int trainerSec = 1800;
    int trainerChargeW = 200;
    int trainerDischargeW = 200;

    bool valid() const {
        if (!(trainerSettleMs >= 500 && trainerSettleMs <= 10000 &&
              trainerAbsW >= 1 && trainerAbsW <= 500 &&
              trainerPct >= 1 && trainerPct <= 30 &&
              trainerTimeoutMs >= 1000 && trainerTimeoutMs <= 20000 &&
              trainerGapMs >= 2000 && trainerGapMs <= 30000 &&
              trainerSec >= 10 && trainerSec <= 1800 &&
              trainerChargeW >= 0 && trainerChargeW <= 20000 &&
              trainerDischargeW >= 0 && trainerDischargeW <= 20000 && trainerSettleMs < trainerTimeoutMs && trainerTimeoutMs <= trainerGapMs)) return false;
        if (dampingW < 0 || dampingW > 1000 || dampingPct < 1 || dampingPct > 100) return false;
        auto validMmDd = [](int mmdd) {
            const int m = mmdd / 100, d = mmdd % 100;
            return m >= 1 && m <= 12 && d >= 1 && d <= 31;
        };
        return nominalMv>=1000 && nominalMv<=1000000 && (daily==0 || daily==1) && deadlineMinute>=0 && deadlineMinute<1440 && chargeA>=0 && chargeA<=500 && dischargeA>=0 && dischargeA<=500 &&
            minVolt>=0 && maxVolt>=0 && minVolt<=1000 && maxVolt<=1000 && (!maxVolt || minVolt<maxVolt) &&
            resumeSoc>=0 && resumeSoc<=ceiling && gridTarget>=-20000 && gridTarget<=20000 &&
            exportLimit>=0 && exportLimit<=20000 && strategy>=0 && strategy<=5 && (fallback==0 || fallback==2 || fallback==3) &&
            floor>=0 && floor<ceiling && ceiling<=100 && target>floor && target<=ceiling &&
            maintenanceSoc>floor && maintenanceSoc<=ceiling &&
            charge>=0 && charge<=20000 && discharge>=0 && discharge<=20000 &&
            deadband>=0 && deadband<=1000 && ramp>=1 && ramp<=20000 &&
            staleMs>=500 && staleMs<=30000 && chargeFirstW>=0 && chargeFirstW<=20000 &&
            targetW>=0 && targetW<=20000 && (surplusOnly==0 || surplusOnly==1) &&
            (gridCharge==0 || gridCharge==1) && hysteresis>=1 && hysteresis<=10 &&
            chargeFirstSec>=1 && chargeFirstSec<=604800 && deadlineSec>=1 && deadlineSec<=604800 &&
            capacityWh>=100 && capacityWh<=100000 && efficiencyPct>=1 && efficiencyPct<=100 &&
            maintenanceHoldSec>=1 && maintenanceHoldSec<maintenanceSec && maintenanceSec<=604800 &&
            (predictive==0 || predictive==1) && (autoSeason==0 || autoSeason==1) &&
            summerFloor>=0 && summerFloor<ceiling && winterFloor>=0 && winterFloor<ceiling &&
            fullHoldSec>=1 && fullHoldSec<=86400 && weekdayLoadW>=0 && weekdayLoadW<=20000 &&
            weekendLoadW>=0 && weekendLoadW<=20000 && validMmDd(winterStart) && validMmDd(winterEnd) &&
            lowSunKwh>=0 && lowSunKwh<=100 && anticipationMin>=0 && anticipationMin<=180 &&
            safetyMarginPct>=100 && safetyMarginPct<=200;
    }
};

struct Decision {
    Strategy strategy=Strategy::Safe, configured=Strategy::SelfUse, previous=Strategy::Safe;
    std::optional<int> candidate, limited, finalWatts, lower, upper, averageW;
    std::optional<double> missingWh;
    const char* state="SAFE", *reason="INPUT_INVALID", *transition="UNCHANGED";
    const char* safetyReason="NONE";
    bool unreachable=false;
    static constexpr bool executed=false;
    int effectiveFloor=20;
    int recommendedChargeA=0;
    bool fogActive=false, winterSeason=false, lowSunMode=false;
    bool usedForecast=false, usedRatio=false, usedWeather=false, usedLoadProfile=false;
    const char* phase="MANUAL";
    const char* seasonName="UNKNOWN";
    // Why the final simulated power differs from the candidate. APPLICABLE means unchanged.
    const char* disposition="NOT_EVALUATED";
    int surplusW=0;
    bool gridPermitted=false;
};
struct CommandExecutor {
    virtual ~CommandExecutor() = default;
    virtual Decision resolve(Decision decision) const = 0;
};
struct ShadowCommandExecutor final : CommandExecutor {
    Decision resolve(Decision decision) const override { return decision; }
};

inline std::optional<double> essential(const Sample& s, uint32_t now, uint32_t timeout) {
    if (s.status != Quality::Valid || now-s.stamp > timeout || !s.value || !std::isfinite(*s.value))
        return {};
    return s.value;
}

// Convert the day's civil deadline to an instant. On the autumn transition choose
// the first occurrence of a duplicated hour. A missing spring hour moves forward
// by the size of the gap.
inline std::optional<int> dailySeconds(time_t now,const tm& local,int minute) {
    tm target=local;
    target.tm_hour=minute/60;
    target.tm_min=minute%60;
    target.tm_sec=0;
    target.tm_isdst=-1;
    tm normalized=target;
    auto deadline=mktime(&normalized);
    if(deadline==time_t(-1))return std::nullopt;
    time_t earliest{};
    time_t afterGap{};
    for(int dst:{0,1}) {
        tm candidate=target;
        candidate.tm_isdst=dst;
        const auto instant=mktime(&candidate);
        tm check{};
#ifdef _WIN32
        const bool converted=localtime_s(&check,&instant)==0;
#else
        const bool converted=localtime_r(&instant,&check)!=nullptr;
#endif
        if(instant==time_t(-1) || !converted)continue;
        const int dayOrder=(check.tm_year-target.tm_year)*366+check.tm_yday-target.tm_yday;
        const int minuteOrder=(check.tm_hour-target.tm_hour)*60+check.tm_min-target.tm_min;
        if(dayOrder==0 && minuteOrder==0 && (!earliest || instant<earliest))earliest=instant;
        if((dayOrder>0 || (dayOrder==0 && minuteOrder>0)) &&
           (!afterGap || instant<afterGap))afterGap=instant;
    }
    if(earliest)deadline=earliest;
    else if(afterGap)deadline=afterGap;
    return int(std::clamp(difftime(deadline,now),0.0,172800.0));
}

struct PredictiveCycleState {
    uint8_t magic = 0xA5;
    uint8_t phase = 0;       // static_cast<uint8_t>(CyclePhase)
    uint8_t discharging = 0; // 0 or 1
    uint8_t reserved = 0;
    int32_t cycleDay = -1;
};

class StrategyManager {
public:
    enum class CyclePhase { SelfUse, GentleCharge, CaptureChargeFirst, FullHold };

    void restorePredictiveState(const PredictiveCycleState& st) {
        if (st.magic == 0xA5 && st.phase <= 3) {
            cyclePhase_ = static_cast<CyclePhase>(st.phase);
            dischargingCycle_ = st.discharging != 0;
            cycleDay_ = st.cycleDay;
            predInit_ = true;
        }
    }

    PredictiveCycleState exportPredictiveState() const {
        PredictiveCycleState st;
        st.magic = 0xA5;
        st.phase = static_cast<uint8_t>(cyclePhase_);
        st.discharging = dischargingCycle_ ? 1 : 0;
        st.reserved = 0;
        st.cycleDay = cycleDay_;
        return st;
    }

    Decision tick(const EnergyState& e, const Policy& p, uint32_t revision, uint32_t now,
                  const tm* local=nullptr, std::optional<int> dailyRemaining={},
                  const PredictiveInputs* pred=nullptr) {
        if (revision_ != revision) {
            const uint32_t prevRevision = revision_;
            revision_ = revision;
            start_ = now;
            day_ = -1;
            holdSince_.reset();
            priorW_.reset();
            done_ = false;
            nearTarget_ = false;
            exit_ = "UNCHANGED";
            fullHoldAccumMs_ = 0;
            fullHoldLastTick_ = now;
            const bool initialRestore = (prevRevision == 0) && predInit_ &&
                                        p.predictive == 1 && (p.strategy == 0 || p.strategy == 1);
            if (!initialRestore && (!hasLastPolicy_ || p.predictive != lastPredictive_ || p.strategy != lastStrategy_)) {
                predInit_ = false;
                dischargingCycle_ = false;
                cycleDay_ = -1;
                fogSticky_ = false;
                fogDay_ = -1;
                fogLastWeatherMs_ = now;
            }
            hasLastPolicy_ = true;
            lastPredictive_ = p.predictive;
            lastStrategy_ = p.strategy;
        }
        Decision d;
        d.configured=static_cast<Strategy>(p.strategy);
        d.previous=effective_;
        const auto soc=essential(e.soc,now,p.staleMs);
        const auto grid=essential(e.grid,now,p.staleMs);
        const auto batt=essential(e.battery,now,p.staleMs);
        const bool healthy=!e.readHalted && e.goodReads && soc && grid && batt &&
            *soc>=0 && *soc<=100 && std::abs(*grid)<=30000 && std::abs(*batt)<=30000 &&
            std::abs(double(int32_t(e.grid.stamp-e.battery.stamp)))<=std::min(p.staleMs,2000);
        const bool temporary=p.strategy==1 || p.strategy>=4;
        if (!healthy && temporary) {
            holdSince_.reset();
            const auto fault = [](const Sample& sample) {
                return sample.status==Quality::ReadError || sample.status==Quality::Invalid;
            };
            if (p.strategy==5 && (e.readHalted || fault(e.soc) || fault(e.grid) || fault(e.battery)))
                finish("MAINTENANCE_FAULT_ABORT");
        }
        if (!healthy) {
            fullHoldAccumMs_ = 0;
            const auto stale=[&](const Sample& sample){
                return sample.status==Quality::Valid && now-sample.stamp>uint32_t(p.staleMs);
            };
            d.safetyReason=stale(e.soc)||stale(e.grid)||stale(e.battery)?"STALE_TELEMETRY":"ESSENTIAL_DATA_UNAVAILABLE";
            d.disposition="INPUT_UNAVAILABLE";
            return complete(d,now);
        }

        const auto volt=essential(e.batteryVoltage,now,p.staleMs);
        const bool haveVoltage=volt && *volt>0 && *volt<=1000;
        const bool voltageSafe=(!p.minVolt && !p.maxVolt) ||
            (haveVoltage && *volt>=p.minVolt && (!p.maxVolt || *volt<=p.maxVolt));
        const int surplus=int(std::lround(*batt-*grid));
        const int balance=surplus+p.gridTarget;

        // Predictive & seasonal autonomous layer (ACTIVE in SHADOW when p.predictive==1 and mode is SelfUse or ChargeFirst)
        if (p.predictive && (p.strategy == 0 || p.strategy == 1)) {
            return tickPredictive(*soc, surplus, balance, volt, haveVoltage, voltageSafe, p, now, local, pred, d);
        }

        uint32_t elapsed=(now-start_)/1000;
        int limit=p.strategy==1 ? p.chargeFirstSec : p.strategy==4 ? p.deadlineSec : p.maintenanceSec;
        if (p.strategy==4 && p.daily && local) {
            const int day=local->tm_year*366+local->tm_yday;
            if (day_>=0 && day>day_) {done_=false;exit_="UNCHANGED";}
            day_=std::max(day_,day);
            const int localSeconds=local->tm_hour*3600+local->tm_min*60+local->tm_sec;
            elapsed=0;
            limit=day<day_ ? 0 : dailyRemaining.value_or(std::max(0,p.deadlineMinute*60-localSeconds));
        }
        const int goal=p.strategy==5 ? p.maintenanceSoc : p.target;
        if (temporary && !done_) {
            if (elapsed>=uint32_t(limit)) finish("DEADLINE_EXPIRED");
            else if (p.strategy!=5 && *soc>=goal) finish("TARGET_REACHED");
            else if (p.strategy==1) {
                if (*soc>=goal-p.hysteresis) nearTarget_=true;
                else if (*soc<goal-2*p.hysteresis) nearTarget_=false;
            } else if (p.strategy==5) {
                if (*soc>=goal && !holdSince_) holdSince_=now;
                if (holdSince_ && *soc<goal-p.hysteresis) holdSince_.reset();
                if (holdSince_ && (now-*holdSince_)/1000>=uint32_t(p.maintenanceHoldSec))
                    finish("MAINTENANCE_COMPLETE");
            }
        }
        d.strategy=done_ ? (p.strategy==1 ? Strategy::SelfUse : static_cast<Strategy>(p.fallback)) : d.configured;
        d.transition=done_ ? exit_ : "UNCHANGED";
        if (d.strategy==Strategy::Native || d.strategy==Strategy::Safe) {
            d.state=d.strategy==Strategy::Native ? "OBSERVE" : "SAFE";
            d.safetyReason=d.strategy==Strategy::Native?"OBSERVATION":"SAFE_MODE";
            d.reason=d.strategy==Strategy::Native ? "NATIVE_OBSERVATION" : "SAFE_REQUESTED";
            d.disposition=d.strategy==Strategy::Native ? "OBSERVATION_ONLY" : "SAFE_MODE";
            return complete(d,now);
        }

        d.effectiveFloor = p.floor;
        if (*soc<=p.floor) reserve_=true;
        else if (*soc>std::max(p.floor,p.resumeSoc)) reserve_=false;
        const bool charging=d.strategy!=Strategy::SelfUse;
        const bool holding=d.strategy==Strategy::Maintenance && holdSince_.has_value();
        const bool permitGrid=charging && p.gridCharge && !(d.strategy==Strategy::ChargeFirst && p.surplusOnly);
        const int cap=std::min(p.charge,d.strategy==Strategy::ChargeFirst ? p.chargeFirstW : charging ? p.targetW : p.charge);
        d.lower=reserve_ || charging ? 0 : -p.discharge;
        if(p.dischargeA) d.lower=std::max(*d.lower,haveVoltage ? -int(p.dischargeA**volt):0);
        d.upper=*soc>=p.ceiling ? 0 : std::min(cap,permitGrid ? cap : std::max(0,surplus));
        d.surplusW=surplus; d.gridPermitted=permitGrid;
        if(p.chargeA) d.upper=std::min(*d.upper,haveVoltage ? int(p.chargeA**volt):0);
        if(p.exportLimit) d.lower=std::min(*d.upper,std::max(*d.lower,surplus-p.exportLimit));
        if(!voltageSafe) {d.lower=0;d.upper=0;}
        d.state=holding ? "HOLD" : d.strategy==Strategy::ChargeFirst && nearTarget_ ? "TAPER" : "RUN";
        d.recommendedChargeA = p.chargeA;
        if (d.strategy==Strategy::Target || d.strategy==Strategy::Maintenance) {
            d.missingWh=std::max(0.0,goal-*soc)*p.capacityWh/p.efficiencyPct;
            const uint32_t reserve=d.strategy==Strategy::Maintenance ? p.maintenanceHoldSec : 0;
            const uint32_t remaining=uint32_t(limit)-elapsed;
            const uint32_t available=remaining>reserve ? remaining-reserve : 0;
            if (available) d.averageW=int(std::min(100000000.,std::ceil(*d.missingWh*3600/available)));
            d.unreachable=*d.missingWh>0 && (!d.averageW || *d.averageW>*d.upper);
            d.candidate=holding ? (*soc<goal ? cap : 0) : d.averageW.value_or(cap);
        } else {
            d.candidate=charging ? (nearTarget_ ? std::min(cap,250) : cap) : balance;
        }
        return clampAndFinalize(*soc, d, p, voltageSafe, now);
    }

private:
    static bool isWinterDate(const Policy& p, const tm* local) {
        if (!local) return p.floor >= 20;
        const int mmdd = (local->tm_mon + 1) * 100 + local->tm_mday;
        if (p.winterStart <= p.winterEnd) return mmdd >= p.winterStart && mmdd <= p.winterEnd;
        return mmdd >= p.winterStart || mmdd <= p.winterEnd;
    }

    Decision tickPredictive(double soc, int surplus, int balance,
                            std::optional<double> volt, bool haveVoltage, bool voltageSafe,
                            const Policy& p, uint32_t now, const tm* local,
                            const PredictiveInputs* pred, Decision d) {
        const PredictiveInputs emptyPred{};
        const PredictiveInputs& ha = pred ? *pred : emptyPred;

        // 1. Seasonal minimum SOC: 20% autumn/winter, 10% spring/summer
        const bool isWinter = isWinterDate(p, local);
        const int effectiveFloor = p.autoSeason ? (isWinter ? p.winterFloor : p.summerFloor) : p.floor;
        d.effectiveFloor = effectiveFloor;
        d.winterSeason = isWinter;
        d.seasonName = isWinter ? "WINTER" : "SUMMER";

        const int resumeDelta = std::max(p.hysteresis, std::max(0, p.resumeSoc - p.floor));
        const int resumeThreshold = std::min(
            p.ceiling - 1,
            p.autoSeason ? (effectiveFloor + resumeDelta)
                         : std::max(effectiveFloor + p.hysteresis, p.resumeSoc));
        if (soc <= effectiveFloor) reserve_ = true;
        else if (soc > resumeThreshold) reserve_ = false;

        const int currentDay = local ? (local->tm_year * 366 + local->tm_yday) : -1;

        // 2. Fog / weather / ratio correction (midnight rollover + 1h stale decay)
        if (local && fogDay_ >= 0 && currentDay != fogDay_) {
            fogSticky_ = false;
            fogDay_ = currentDay;
        }
        if (ha.weatherValid) {
            d.usedWeather = true;
            fogLastWeatherMs_ = now;
            fogDay_ = currentDay;
            if (ha.fog) fogSticky_ = true;
            else if (ha.spreadC <= 1.5 && ha.radiationWm2 < 100.0 && (!local || local->tm_hour < 12)) fogSticky_ = true;
            else if (!fogSticky_) fogSticky_ = false;
            else if ((!ha.fog && ha.radiationWm2 >= 250.0) ||
                     (ha.spreadC >= 2.5 && ha.radiationWm2 >= 180.0 && (!ha.ratioValid || ha.ratio >= 0.75))) {
                fogSticky_ = false;
            }
        } else if (fogSticky_ && uint32_t(now - fogLastWeatherMs_) >= 3600u * 1000u) {
            fogSticky_ = false;
        }
        const bool fogActive = fogSticky_;
        d.fogActive = fogActive;

        double factor = 1.0;
        if (ha.ratioValid) {
            d.usedRatio = true;
            factor = std::clamp(ha.ratio, 0.35, 1.0);
        }
        if (fogActive) factor *= 0.30;

        // 3. Weekday vs Weekend load profile
        const bool isWeekend = local ? (local->tm_wday == 0 || local->tm_wday == 6) : false;
        double loadKw = isWeekend ? (p.weekendLoadW / 1000.0) : (p.weekdayLoadW / 1000.0);
        if (ha.loadProfileValid && ha.loadTodayKw > 0.0) {
            loadKw = ha.loadTodayKw;
            d.usedLoadProfile = true;
        }

        // 4. Target deadline window (default 15:30 - 20 min = 15:10, keeping 15:10-15:30 as anticipation recovery)
        const int localMin = local ? (local->tm_hour * 60 + local->tm_min) : 12 * 60;
        const int effTargetMin = std::max(0, p.deadlineMinute - p.anticipationMin);
        const bool pastDeadline = local && localMin >= p.deadlineMinute;
        const bool inAnticipationWindow = local && localMin >= effTargetMin && localMin < p.deadlineMinute;
        const double remainingHours = local && localMin < effTargetMin ? (effTargetMin - localMin) / 60.0 : 0.0;

        double expectedPvKwh = 0.0;
        if (ha.forecastValid) {
            d.usedForecast = true;
            expectedPvKwh = ha.remainingForecastKwh * factor;
        }
        const double forecastWindowHours = local && !pastDeadline
            ? std::max(0.0, (p.deadlineMinute - localMin) / 60.0)
            : 0.0;
        const double expectedSurplusKwh = std::max(0.0, expectedPvKwh - loadKw * forecastWindowHours);
        const double deficitFullKwh = std::max(0.0, (double(p.target) - soc) * p.capacityWh / 100000.0);
        const double requiredInputKwh = deficitFullKwh * (100.0 / p.efficiencyPct) * (p.safetyMarginPct / 100.0);

        const bool tomorrowFavorable = ha.forecastValid && ha.tomorrowTotalKwh >= double(p.lowSunKwh);
        const bool todayLowTotal = (ha.todayTotalKwh > 0.0 && ha.todayTotalKwh < double(p.lowSunKwh));
        const bool forecastDeficit = !pastDeadline && (expectedSurplusKwh < requiredInputKwh);
        const bool poorSolar = !ha.forecastValid || fogActive || todayLowTotal || forecastDeficit;
        const bool lowSunMode = isWinter ? (poorSolar || !tomorrowFavorable) : (poorSolar && !tomorrowFavorable);
        d.lowSunMode = lowSunMode;

        const double refVolt = haveVoltage ? *volt : (p.nominalMv / 1000.0);
        const int maxAmpW = p.chargeA ? (haveVoltage ? int(p.chargeA * *volt) : int(p.chargeA * (p.nominalMv / 1000.0))) : p.charge;
        int fullChargeCap = p.charge;
        if (p.chargeA) fullChargeCap = std::min(fullChargeCap, maxAmpW);

        // 5. Closed-loop phase transitions
        if (!predInit_) {
            predInit_ = true;
            if (soc >= double(p.target) - 0.5) {
                cyclePhase_ = CyclePhase::FullHold;
                fullHoldAccumMs_ = 0;
                fullHoldLastTick_ = now;
            } else if (lowSunMode || soc <= effectiveFloor) {
                cyclePhase_ = CyclePhase::CaptureChargeFirst;
            } else if (remainingHours > 0.0 || inAnticipationWindow) {
                cyclePhase_ = CyclePhase::GentleCharge;
            } else {
                cyclePhase_ = CyclePhase::SelfUse;
            }
        }

        if (soc <= effectiveFloor) {
            dischargingCycle_ = false;
            cycleDay_ = -1;
        }

        bool justCompleted5MinHold = false;
        const uint32_t dtMs = now >= fullHoldLastTick_ ? std::min<uint32_t>(now - fullHoldLastTick_, 5000u) : 0u;
        fullHoldLastTick_ = now;

        switch (cyclePhase_) {
        case CyclePhase::FullHold:
            if (soc < double(p.target - p.hysteresis)) {
                fullHoldAccumMs_ = 0;
                cyclePhase_ = CyclePhase::CaptureChargeFirst;
                d.transition = "HOLD_ABORTED_CHARGE";
            } else {
                if (soc >= double(p.target) - 0.5) {
                    fullHoldAccumMs_ += dtMs;
                }
                if (fullHoldAccumMs_ >= uint32_t(p.fullHoldSec) * 1000u) {
                    fullHoldAccumMs_ = 0;
                    dischargingCycle_ = true;
                    cycleDay_ = currentDay;
                    cyclePhase_ = CyclePhase::SelfUse;
                    d.transition = "CALIBRATION_COMPLETE_SELF_USE";
                    justCompleted5MinHold = true;
                }
            }
            break;

        case CyclePhase::CaptureChargeFirst:
            if (soc >= double(p.target) - 0.5) {
                cyclePhase_ = CyclePhase::FullHold;
                fullHoldAccumMs_ = 0;
                d.transition = "FULL_HOLD_STARTED";
            } else if (!lowSunMode && soc > effectiveFloor + p.hysteresis && remainingHours > 1.0 &&
                       ha.forecastValid && expectedSurplusKwh >= requiredInputKwh * 1.15) {
                cyclePhase_ = CyclePhase::GentleCharge;
                d.transition = "SWITCH_GENTLE_TARGET";
            }
            break;

        case CyclePhase::GentleCharge:
            if (soc >= double(p.target) - 0.5) {
                cyclePhase_ = CyclePhase::FullHold;
                fullHoldAccumMs_ = 0;
                d.transition = "FULL_HOLD_STARTED";
            } else if (pastDeadline) {
                dischargingCycle_ = true;
                cycleDay_ = currentDay;
                cyclePhase_ = CyclePhase::SelfUse;
                d.transition = "DEADLINE_REACHED_SELF_USE";
            } else if (lowSunMode && !inAnticipationWindow) {
                cyclePhase_ = CyclePhase::CaptureChargeFirst;
                d.transition = "CYCLE_CHARGE_FIRST";
            }
            break;

        case CyclePhase::SelfUse:
            if (soc <= effectiveFloor) {
                dischargingCycle_ = false;
                cycleDay_ = -1;
                if (lowSunMode || !ha.forecastValid || remainingHours <= 0.0) {
                    cyclePhase_ = CyclePhase::CaptureChargeFirst;
                    d.transition = "CYCLE_CHARGE_FIRST";
                } else {
                    cyclePhase_ = CyclePhase::GentleCharge;
                    d.transition = "CYCLE_GENTLE_TARGET";
                }
            } else if (!dischargingCycle_) {
                if (lowSunMode && !inAnticipationWindow) {
                    cyclePhase_ = CyclePhase::CaptureChargeFirst;
                    d.transition = "CYCLE_CHARGE_FIRST";
                } else if (soc < double(p.target) && (remainingHours > 0.0 || inAnticipationWindow)) {
                    cyclePhase_ = CyclePhase::GentleCharge;
                    d.transition = "CYCLE_GENTLE_TARGET";
                }
            } else if (!lowSunMode && ha.forecastValid && remainingHours > 0.0 &&
                       soc < double(p.target - p.hysteresis) && local && cycleDay_ >= 0 && currentDay > cycleDay_) {
                dischargingCycle_ = false;
                cycleDay_ = -1;
                cyclePhase_ = CyclePhase::GentleCharge;
                d.transition = "CYCLE_GENTLE_TARGET";
            }
            break;
        }

        // 6. Execute phase policy bounds & candidate
        if (cyclePhase_ == CyclePhase::CaptureChargeFirst || cyclePhase_ == CyclePhase::FullHold) {
            d.strategy = Strategy::ChargeFirst;
            const bool holding100 = (cyclePhase_ == CyclePhase::FullHold);
            d.phase = holding100 ? "FULL_HOLD_5M" : "CAPTURE_CHARGE_FIRST";
            d.state = holding100 ? "HOLD_100" : "CAPTURE";
            const int cap = std::min(fullChargeCap, p.chargeFirstW);
            const bool permitGrid = p.gridCharge && !p.surplusOnly;
            d.lower = 0; // Block battery discharge strictly while capturing solar / holding 100%
            d.upper = soc >= p.ceiling ? 0 : std::min(cap, permitGrid ? cap : std::max(0, surplus));
            d.surplusW = surplus; d.gridPermitted = permitGrid;
            d.candidate = cap;
            d.missingWh = deficitFullKwh * 1000.0;
            d.recommendedChargeA = p.chargeA;
            d.reason = holding100 ? "FULL_SOC_HOLD_5M" : "CAPTURE_CHARGE_FIRST";
        } else if (cyclePhase_ == CyclePhase::GentleCharge) {
            d.strategy = Strategy::Target;
            d.phase = "GENTLE_TARGET";
            const int cap = std::min(fullChargeCap, p.targetW);
            const double effHours = std::max(0.25, remainingHours);
            const int rawGentleW = int(std::ceil(requiredInputKwh * 1000.0 / effHours));
            const int minGentleW = std::min(cap, std::max(200, int(std::lround(refVolt * 4.0))));
            const int gentleW = std::clamp(rawGentleW, minGentleW, cap);
            const bool inRecovery = inAnticipationWindow ||
                                    (local && localMin < p.deadlineMinute && (p.deadlineMinute - localMin) <= 60) ||
                                    rawGentleW >= int(cap * 0.9);
            const int targetCapW = inRecovery ? cap : gentleW;
            const bool permitGrid = p.gridCharge;

            d.state = inRecovery ? "RECOVERY" : "GENTLE";
            d.missingWh = deficitFullKwh * 1000.0;
            d.averageW = gentleW;
            d.unreachable = deficitFullKwh > 0.0 && rawGentleW > cap;
            d.recommendedChargeA = std::max(1, int(std::ceil(targetCapW / std::max(10.0, refVolt))));
            d.lower = 0;
            d.upper = soc >= p.ceiling ? 0 : std::min(targetCapW, permitGrid ? targetCapW : std::max(0, surplus));
            d.surplusW = surplus; d.gridPermitted = permitGrid;
            d.candidate = targetCapW;
            d.reason = "GENTLE_TARGET_CHARGE";
        } else {
            d.strategy = Strategy::SelfUse;
            d.phase = "SELF_USE";
            d.state = "RUN";
            d.lower = reserve_ ? 0 : -p.discharge;
            if (p.dischargeA) d.lower = std::max(*d.lower, haveVoltage ? -int(p.dischargeA * *volt) : 0);
            d.upper = soc >= p.ceiling ? 0 : std::min(fullChargeCap, std::max(0, surplus));
            d.surplusW = surplus; d.gridPermitted = false;
            if (p.exportLimit) d.lower = std::min(*d.upper, std::max(*d.lower, surplus - p.exportLimit));
            d.candidate = balance;
            d.recommendedChargeA = p.chargeA;
            if (justCompleted5MinHold) d.reason = "FULL_SOC_5MIN_COMPLETE";
        }

        return clampAndFinalize(soc, d, p, voltageSafe, now, justCompleted5MinHold || (cyclePhase_ != CyclePhase::SelfUse));
    }

    Decision clampAndFinalize(double soc, Decision d, const Policy& p, bool voltageSafe, uint32_t now, bool keepCustomReason=false) {
        if (!voltageSafe) { d.lower = 0; d.upper = 0; }
        int selected = std::clamp(*d.candidate, *d.lower, *d.upper);
        const int bounded = selected;
        d.limited = bounded;
        if (!voltageSafe) d.safetyReason = "VOLTAGE_LIMIT";
        else if (*d.candidate < 0 && reserve_) d.safetyReason = "SOC_RESERVE";
        else if (*d.candidate > 0 && soc >= p.ceiling) d.safetyReason = "SOC_CEILING";
        else if (!keepCustomReason && bounded != *d.candidate) d.safetyReason = "POWER_LIMIT";

        if (priorW_ && effective_ == d.strategy) {
            if (d.strategy == Strategy::SelfUse ||
                (d.strategy == Strategy::ChargeFirst && !d.gridPermitted && selected > *priorW_ &&
                 std::string_view(state_) != "TAPER")) {
                const int delta = selected - *priorW_;
                selected = *priorW_ + (std::abs(delta) <= p.dampingW ? 0 : delta * p.dampingPct / 100);
            }
            const int step = int(p.ramp * std::min(now - last_, uint32_t(4000)) / 1000);
            selected = std::abs(selected - *priorW_) < p.deadband ? *priorW_
                : std::clamp(selected, *priorW_ - step, *priorW_ + step);
        }
        d.finalWatts = std::clamp(selected, *d.lower, *d.upper);
        if (!keepCustomReason && selected != *d.finalWatts && d.safetyReason == std::string_view("NONE"))
            d.safetyReason = "POWER_LIMIT";

        // Explicit disposition: never leave a modified or cancelled candidate without a cause.
        const int cand = *d.candidate, fin = *d.finalWatts;
        if (fin == cand) {
            d.disposition = "APPLICABLE";
        } else if (!voltageSafe || d.safetyReason == std::string_view("SOC_RESERVE") ||
                   d.safetyReason == std::string_view("SOC_CEILING")) {
            d.disposition = fin == 0 ? "SAFETY_LIMIT" : "PARTIAL_SAFETY_LIMIT";
        } else if (bounded != cand) {
            if (cand > 0 && *d.upper == 0 && d.surplusW <= 0 && !d.gridPermitted) {
                d.disposition = (!p.gridCharge && !p.surplusOnly) ? "GRID_CHARGE_DISABLED" : "NO_PV_SURPLUS";
            } else if (fin == 0) {
                d.disposition = "AVAILABLE_POWER_ZERO";
            } else if (cand > 0 && !d.gridPermitted && *d.upper == std::max(0, d.surplusW)) {
                d.disposition = "PARTIAL_PV_SURPLUS";
            } else {
                d.disposition = "PARTIAL_POWER_LIMIT";
            }
        } else {
            d.disposition = "RAMP_OR_DEADBAND";
        }

        if (!keepCustomReason || d.unreachable) {
            const bool charging = d.strategy != Strategy::SelfUse;
            const bool holding = d.strategy == Strategy::Maintenance && holdSince_.has_value();
            d.reason = d.unreachable ? "UNREACHABLE_AT_CURRENT_LIMIT" : holding ? "MAINTENANCE_HOLD" :
                selected != *d.finalWatts ? "SAFETY_RECLAMP" : bounded != *d.candidate ? "SAFETY_LIMIT" :
                bounded != *d.finalWatts ? "DEADBAND_OR_RAMP" : charging ? "CHARGE_REQUEST" : "GRID_BALANCE";
        }
        return complete(d, now);
    }

    void finish(const char* why) { done_=true; exit_=why; }
    Decision complete(Decision d,uint32_t now) {
        if (d.transition==std::string_view("UNCHANGED") && (d.strategy!=effective_ || d.state!=state_))
            d.transition=d.state==std::string_view("HOLD") ? "HOLD_STARTED" :
                d.strategy==Strategy::Safe ? "ENTER_SAFE" : "STRATEGY_OR_STATE_CHANGED";
        effective_=d.strategy;
        state_=d.state;
        last_=now;
        priorW_=d.finalWatts;
        return ShadowCommandExecutor{}.resolve(d);
    }
    int day_=-1;
    uint32_t revision_=0, start_=0, last_=0;
    std::optional<uint32_t> holdSince_;
    std::optional<int> priorW_;
    Strategy effective_=Strategy::Safe;
    const char* state_="SAFE", *exit_="UNCHANGED";
    bool done_=false, nearTarget_=false, reserve_=true;
    // Predictive state
    bool predInit_=false, dischargingCycle_=false, fogSticky_=false, hasLastPolicy_=false;
    int lastPredictive_=0, lastStrategy_=0, cycleDay_=-1, fogDay_=-1;
    CyclePhase cyclePhase_=CyclePhase::SelfUse;
    uint32_t fullHoldAccumMs_=0, fullHoldLastTick_=0, fogLastWeatherMs_=0;
};
}
