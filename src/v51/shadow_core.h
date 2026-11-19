// SPDX-License-Identifier: MIT
#pragma once
#include "energy_state.h"
#include <algorithm>
#include <cmath>
#include <ctime>
#include <string_view>

namespace heliograph::v51 {
enum class Strategy { SelfUse, ChargeFirst, Native, Safe, Target, Maintenance };
inline constexpr const char* names[] = {"SELF_USE","CHARGE_FIRST","NATIVE_INVERTER","SAFE_MODE","TARGET_SOC","MAINTENANCE"};
inline const char* strategyName(Strategy s) { return names[static_cast<int>(s)]; }

struct Policy {
    int strategy=0, fallback=0, floor=20, ceiling=100, target=100;
    int charge=1000, discharge=1000, deadband=20, ramp=250, staleMs=5000;
    int chargeFirstW=1000, surplusOnly=1, gridCharge=0, hysteresis=2, chargeFirstSec=7200;
    int deadlineSec=14400, capacityWh=10240, efficiencyPct=90, targetW=1000;
    int chargeA=50,dischargeA=50,minVolt=0,maxVolt=0,resumeSoc=20,gridTarget=0,exportLimit=3300;
    int maintenanceSoc=90, maintenanceHoldSec=600, maintenanceSec=14400;
    int nominalMv=51200, daily=1, deadlineMinute=930;
    bool valid() const {
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
            maintenanceHoldSec>=1 && maintenanceHoldSec<maintenanceSec && maintenanceSec<=604800;
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
class StrategyManager {
public:
    Decision tick(const EnergyState& e, const Policy& p, uint32_t revision, uint32_t now, const tm* local=nullptr, std::optional<int> dailyRemaining={}) {
        if (revision_ != revision) {
            revision_=revision;
            start_=now;
            day_=-1;
            holdSince_.reset();
            priorW_.reset();
            done_=false;
            nearTarget_=false;
            exit_="UNCHANGED";
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
            const auto stale=[&](const Sample& sample){
                return sample.status==Quality::Valid && now-sample.stamp>uint32_t(p.staleMs);
            };
            d.safetyReason=stale(e.soc)||stale(e.grid)||stale(e.battery)?"STALE_TELEMETRY":"ESSENTIAL_DATA_UNAVAILABLE";
            return complete(d,now);
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
            return complete(d,now);
        }

        const int surplus=int(std::lround(*batt-*grid));
        const int balance=surplus+p.gridTarget;
        const auto volt=essential(e.batteryVoltage,now,p.staleMs);
        const bool haveVoltage=volt && *volt>0 && *volt<=1000;
        const bool voltageSafe=(!p.minVolt && !p.maxVolt) ||
            (haveVoltage && *volt>=p.minVolt && (!p.maxVolt || *volt<=p.maxVolt));
        if (*soc<=p.floor) reserve_=true;
        else if (*soc>std::max(p.floor,p.resumeSoc)) reserve_=false;
        const bool charging=d.strategy!=Strategy::SelfUse;
        const bool holding=d.strategy==Strategy::Maintenance && holdSince_.has_value();
        const bool permitGrid=charging && p.gridCharge && !(d.strategy==Strategy::ChargeFirst && p.surplusOnly);
        const int cap=std::min(p.charge,d.strategy==Strategy::ChargeFirst ? p.chargeFirstW : charging ? p.targetW : p.charge);
        d.lower=reserve_ || charging ? 0 : -p.discharge;
        if(p.dischargeA) d.lower=std::max(*d.lower,haveVoltage ? -int(p.dischargeA**volt):0);
        d.upper=*soc>=p.ceiling ? 0 : std::min(cap,permitGrid ? cap : std::max(0,surplus));
        if(p.chargeA) d.upper=std::min(*d.upper,haveVoltage ? int(p.chargeA**volt):0);
        if(p.exportLimit) d.lower=std::min(*d.upper,std::max(*d.lower,surplus-p.exportLimit));
        if(!voltageSafe) {d.lower=0;d.upper=0;}
        d.state=holding ? "HOLD" : d.strategy==Strategy::ChargeFirst && nearTarget_ ? "TAPER" : "RUN";
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
        int selected=std::clamp(*d.candidate,*d.lower,*d.upper);
        const int bounded=selected;
        d.limited=bounded;
        if(!voltageSafe)d.safetyReason="VOLTAGE_LIMIT";
        else if(*d.candidate<0 && reserve_)d.safetyReason="SOC_RESERVE";
        else if(*d.candidate>0 && *soc>=p.ceiling)d.safetyReason="SOC_CEILING";
        else if(bounded!=*d.candidate)d.safetyReason="POWER_LIMIT";
        if (priorW_ && effective_==d.strategy) {
            const int step=int(p.ramp*std::min(now-last_,uint32_t(4000))/1000);
            selected=std::abs(selected-*priorW_)<p.deadband ? *priorW_
                : std::clamp(selected,*priorW_-step,*priorW_+step);
        }
        d.finalWatts=std::clamp(selected,*d.lower,*d.upper);
        if(selected!=*d.finalWatts && d.safetyReason==std::string_view("NONE"))d.safetyReason="POWER_LIMIT";
        d.reason=d.unreachable ? "UNREACHABLE_AT_CURRENT_LIMIT" : holding ? "MAINTENANCE_HOLD" :
            selected!=*d.finalWatts ? "SAFETY_RECLAMP" : bounded!=*d.candidate ? "SAFETY_LIMIT" :
            bounded!=*d.finalWatts ? "DEADBAND_OR_RAMP" : charging ? "CHARGE_REQUEST" : "GRID_BALANCE";
        return complete(d,now);
    }
private:
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
};
}
