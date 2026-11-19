// SPDX-License-Identifier: MIT
#include "ems_runtime.h"
#include "clock_service.h"
#include "remote.h"
#include "m4.h"
#include "m5.h"
#include <array>
#include <Arduino.h>
#include <cstring>
#include <mutex>
#include <sstream>
#include <charconv>

namespace heliograph::v51 {
namespace {
struct Field { const char* name; int Policy::* member; };
constexpr Field fields[]={
    {"charge_a",&Policy::chargeA},{"discharge_a",&Policy::dischargeA},
    {"voltage_min",&Policy::minVolt},{"voltage_max",&Policy::maxVolt},{"soc_resume",&Policy::resumeSoc},
    {"grid_target_w",&Policy::gridTarget},{"export_limit_w",&Policy::exportLimit},
    {"strategy",&Policy::strategy},{"fallback",&Policy::fallback},{"soc_min",&Policy::floor},
    {"soc_max",&Policy::ceiling},{"soc_target",&Policy::target},{"charge_w",&Policy::charge},
    {"discharge_w",&Policy::discharge},{"deadband_w",&Policy::deadband},{"ramp_w_s",&Policy::ramp},
    {"stale_ms",&Policy::staleMs},{"charge_first_w",&Policy::chargeFirstW},
    {"surplus_only",&Policy::surplusOnly},{"grid_charge",&Policy::gridCharge},
    {"soc_hysteresis",&Policy::hysteresis},{"charge_first_s",&Policy::chargeFirstSec},
    {"target_deadline_s",&Policy::deadlineSec},{"capacity_wh",&Policy::capacityWh},
    {"efficiency_pct",&Policy::efficiencyPct},{"target_w",&Policy::targetW},
    {"maintenance_soc",&Policy::maintenanceSoc},{"hold_s",&Policy::maintenanceHoldSec},
    {"maintenance_timeout_s",&Policy::maintenanceSec},
    {"nominal_mv",&Policy::nominalMv},{"daily_deadline",&Policy::daily},{"deadline_minute",&Policy::deadlineMinute},
    {"predictive",&Policy::predictive},{"auto_season",&Policy::autoSeason},
    {"summer_soc_min",&Policy::summerFloor},{"winter_soc_min",&Policy::winterFloor},
    {"full_hold_s",&Policy::fullHoldSec},{"weekday_load_w",&Policy::weekdayLoadW},
    {"weekend_load_w",&Policy::weekendLoadW},{"winter_start",&Policy::winterStart},
    {"winter_end",&Policy::winterEnd},{"low_sun_kwh",&Policy::lowSunKwh},
    {"anticipation_min",&Policy::anticipationMin},{"safety_margin_pct",&Policy::safetyMarginPct},
};
std::mutex lock;
Policy current;
uint32_t revision=1;
std::optional<Policy> overridePolicy;
uint32_t leaseStart=0, leaseDuration=0;
Preferences* storage=nullptr;
bool (*checkAuth)(httpd_req_t*)=nullptr;

template<typename T> void number(std::ostream& out,std::optional<T> value) {
    if (value) out<<*value;
    else out<<"null";
}
std::string audit(const EnergyState& e,const Policy& p,const Decision& d,
                  uint32_t now,uint32_t seq,const char* provenance) {
    std::ostringstream out;
    auto stamp=clockStamp();
    stamp.uptime=now;
    out<<"{\"schema\":1,\"source\":\""<<provenance<<"\",\"seq\":"<<seq;
    appendClock(out,stamp);
    out<<",\"strategy\":\""<<strategyName(d.strategy)
       <<"\",\"configured\":\""<<strategyName(d.configured)<<"\",\"previous\":\""<<strategyName(d.previous)
       <<"\",\"state\":\""<<d.state<<"\",\"transition\":\""<<d.transition<<"\",\"soc\":";
    number(out,essential(e.soc,now,p.staleMs));
    out<<",\"grid_w\":"; number(out,essential(e.grid,now,p.staleMs));
    out<<",\"quality\":{\"soc\":\""<<(essential(e.soc,now,p.staleMs)?"valid":"unavailable")<<"\",\"grid\":\""<<(essential(e.grid,now,p.staleMs)?"valid":"unavailable")<<"\"}";
    out<<",\"battery_w\":"; number(out,essential(e.battery,now,p.staleMs));
    out<<",\"load_w\":"; number(out,essential(e.load,now,p.staleMs));
    out<<",\"pv_w\":"; number(out,essential(e.pv,now,p.staleMs));
    out<<",\"safety_reason\":\""<<d.safetyReason<<"\",\"limited_w\":";number(out,d.limited);
    out<<",\"candidate_w\":"; number(out,d.candidate);
    out<<",\"simulated_w\":"; number(out,d.finalWatts);
    out<<",\"safety_min_w\":"; number(out,d.lower);
    out<<",\"safety_max_w\":"; number(out,d.upper);
    out<<",\"missing_wh\":"; number(out,d.missingWh);
    out<<",\"average_w\":"; number(out,d.averageW);
    out<<",\"unreachable\":"<<(d.unreachable ? "true":"false");
    out<<",\"effective_soc_min\":"<<d.effectiveFloor;
    out<<",\"recommended_charge_a\":"<<d.recommendedChargeA;
    out<<",\"season\":\""<<d.seasonName<<"\",\"predictive_phase\":\""<<d.phase<<"\"";
    out<<",\"fog_active\":"<<(d.fogActive ? "true":"false")<<",\"low_sun_mode\":"<<(d.lowSunMode ? "true":"false");
    out<<",\"reason\":\""<<d.reason<<"\",\"executed\":false,\"applied\":null,\"aux1\":\""<<(auxOn()?"ON":"OFF")<<"\",\"modbus_write_count\":0}";
    return out.str();
}

esp_err_t getPolicy(httpd_req_t* r) {
    if (!checkAuth(r)) return ESP_OK;
    std::ostringstream json;
    json<<"{";
    {
        const std::lock_guard guard(lock);
        for (const auto& f:fields) json<<"\""<<f.name<<"\":"<<current.*(f.member)<<",";
        json<<"\"battery_profile_source\":\"HA/Solarman\",\"battery_model_provisional\":false,\"efficiency_provisional\":true,\"revision\":"<<revision<<"}";
    }
    httpd_resp_set_type(r,"application/json");
    return httpd_resp_sendstr(r,json.str().c_str());
}
esp_err_t setPolicy(httpd_req_t* r) {
    if (!checkAuth(r)) return ESP_OK;
    char body[1024]{};
    if (!r->content_len || r->content_len>=sizeof(body))
        return httpd_resp_send_err(r,HTTPD_400_BAD_REQUEST,"Expected bounded form key=value");
    size_t offset=0;
    while (offset<r->content_len) {
        const int n=httpd_req_recv(r,body+offset,r->content_len-offset);
        if (n<=0) return ESP_FAIL;
        offset+=n;
    }
    if (strlen(body)!=offset) return httpd_resp_send_err(r,HTTPD_400_BAD_REQUEST,"Invalid form");
    if (!applyPolicy(body,0,strcmp(body,"rollback=1")==0?3:0)) return httpd_resp_send_err(r,HTTPD_400_BAD_REQUEST,"Invalid policy or persistence failed");
    return getPolicy(r);
}
esp_err_t replay(httpd_req_t* r) {
    if (!checkAuth(r)) return ESP_OK;
    httpd_resp_set_type(r,"application/x-ndjson");
    for (int i=0;i<6;++i) {
        Policy p;
        p.strategy=i;
        p.target=80; p.chargeA=0; p.dischargeA=0;
        p.maintenanceHoldSec=2;
        p.maintenanceSec=60;
        p.chargeFirstSec=30;
        StrategyManager manager;
        EnergyState e;
        e.goodReads=1;
        e.soc={i==5?90.:50.,1000,Quality::Valid};
        e.grid={i==0?500.:-700.,1000,Quality::Valid};
        e.battery={0,1000,Quality::Valid};
        e.load={300,1000,Quality::Valid};
        e.pv={1000,1000,Quality::Valid};
        auto send=[&](uint32_t now) {
            auto line=audit(e,p,manager.tick(e,p,1,now),now,i,"REPLAY")+"\n";
            return httpd_resp_sendstr_chunk(r,line.c_str())==ESP_OK;
        };
        if (!send(1000)) return ESP_FAIL;
        if (i==1 || i==4) {
            e.soc.value=80;
            if (!send(2000)) return ESP_FAIL;
        } else if (i==5 && !send(4000)) return ESP_FAIL;
    }
    return httpd_resp_send_chunk(r,nullptr,0);
}
}
PolicyStatus policyStatus() {
    const std::lock_guard guard(lock);
    return {revision,static_cast<Strategy>(overridePolicy.value_or(current).strategy)};
}
void appendPolicyJson(std::ostream& out) {
    const std::lock_guard guard(lock);
    out<<"{";
    for (const auto& f:fields) out<<"\""<<f.name<<"\":"<<current.*(f.member)<<",";
    out<<"\"strategy_name\":\""<<strategyName(static_cast<Strategy>(current.strategy))
       <<"\",\"revision\":"<<revision<<",\"battery_profile_source\":\"HA/Solarman\",\"battery_model_provisional\":false,\"efficiency_provisional\":true";
    appendClock(out,clockStamp());
    out<<"}";
}
bool applyPolicy(const std::string& form,uint32_t leaseMs,int cancel) {
    const std::lock_guard guard(lock);
    if (cancel) {
        if (cancel==3) {
            Policy previous;
            if (!storage || storage->getBytesLength("ems-v3-prev")!=sizeof(previous) ||
                storage->getBytes("ems-v3-prev",&previous,sizeof(previous))!=sizeof(previous) ||
                !previous.valid() || storage->putBytes("ems-policy-v3",&previous,sizeof(previous))!=sizeof(previous))
                return false;
            current=previous;
        }
        if (cancel==2 && (current.strategy==1 || current.strategy>=4)) {
            Policy next=current;
            next.strategy=current.strategy==1 ? 0 : current.fallback;
            if (!storage || storage->putBytes("ems-policy-v3",&next,sizeof(next))!=sizeof(next))
                return false;
            current=next;
        }
        if (overridePolicy || cancel>=2) {
            overridePolicy.reset();
            ++revision;
        }
        return true;
    }
    if (form.empty() || form.size()>=1024) return false;
    char body[1024];
    memcpy(body,form.c_str(),form.size()+1);
    Policy next;
    next=current;
    const int previousStrategy=next.strategy;
    char* context=nullptr;
    bool changed=false;
    bool explicitFallback=false;
    for (char* token=strtok_r(body,"&",&context); token; token=strtok_r(nullptr,"&",&context)) {
        char* eq=strchr(token,'=');
        if (!eq) return false;
        *eq++=0;
        const auto* end=eq+strlen(eq);
        int value;
        const auto result=std::from_chars(eq,end,value);
        const auto field=std::find_if(std::begin(fields),std::end(fields),
            [token](Field f){ return strcmp(f.name,token)==0; });
        if (result.ec!=std::errc{} || result.ptr!=end || field==std::end(fields))
            return false;
        explicitFallback |= field->member==&Policy::fallback;
        next.*(field->member)=value;
        changed=true;
    }
    if (!explicitFallback && next.strategy!=previousStrategy && next.strategy>=4 &&
        (previousStrategy==0 || previousStrategy==2 || previousStrategy==3)) {
        next.fallback=previousStrategy;
    }
    if (!changed || !next.valid()) return false;
    if (leaseMs) {
        overridePolicy=next;
        leaseStart=millis();
        leaseDuration=leaseMs;
    } else {
        if (!storage || storage->putBytes("ems-v3-prev",&current,sizeof(current))!=sizeof(current) ||
            storage->putBytes("ems-policy-v3",&next,sizeof(next))!=sizeof(next))
            return false;
        current=next;
        overridePolicy.reset();
    }
    ++revision;
    return true;
}
void loadPolicy(Preferences& preferences) {
    storage=&preferences;
    constexpr size_t v2Size = offsetof(Policy, predictive);
    Policy saved;
    if (preferences.getBytesLength("ems-policy-v3") == sizeof(saved) &&
        preferences.getBytes("ems-policy-v3", &saved, sizeof(saved)) == sizeof(saved) && saved.valid()) {
        current = saved;
        return;
    }
    if (preferences.getBytesLength("ems-v3-prev") == sizeof(saved) &&
        preferences.getBytes("ems-v3-prev", &saved, sizeof(saved)) == sizeof(saved) && saved.valid()) {
        current = saved;
        return;
    }
    // Migrate v2 policy into predictive-capable v3 preserving all user parameters
    Policy migrated;
    if (preferences.getBytesLength("ems-policy-v2") == v2Size &&
        preferences.getBytes("ems-policy-v2", &migrated, v2Size) == v2Size && migrated.valid()) {
        migrated.predictive = 1;
        migrated.autoSeason = 1;
        migrated.summerFloor = 10;
        migrated.winterFloor = 20;
        migrated.fullHoldSec = 300;
        migrated.weekdayLoadW = 600;
        migrated.weekendLoadW = 900;
        migrated.lowSunKwh = 8;
        migrated.anticipationMin = 20;
        migrated.safetyMarginPct = 115;
        // Unlock watt ceiling up to chargeA * nominalV when legacy default was stuck at 1000W
        const int ampCapW = migrated.chargeA > 0 ? int(migrated.chargeA * (migrated.nominalMv / 1000.0)) : 2600;
        if (migrated.charge <= 1000 && ampCapW > 1000) migrated.charge = ampCapW;
        if (migrated.chargeFirstW <= 1000 && ampCapW > 1000) migrated.chargeFirstW = ampCapW;
        if (migrated.targetW <= 1000 && ampCapW > 1000) migrated.targetW = ampCapW;
        if (migrated.chargeFirstSec < 43200) migrated.chargeFirstSec = 43200;
        if (migrated.valid()) {
            current = migrated;
            preferences.putBytes("ems-policy-v3", &current, sizeof(current));
            return;
        }
    }
    // Fresh board fallback
    current.predictive = 1;
    const int ampCapW = current.chargeA > 0 ? int(current.chargeA * (current.nominalMv / 1000.0)) : 2600;
    current.charge = std::max(current.charge, ampCapW);
    current.chargeFirstW = std::max(current.chargeFirstW, ampCapW);
    current.targetW = std::max(current.targetW, ampCapW);
    current.chargeFirstSec = 43200;
}
void tickEms(const EnergyState& state,uint32_t now,uint32_t seq) {
    static StrategyManager manager;
    Policy p;
    uint32_t version;
    {
        const std::lock_guard guard(lock);
        if (overridePolicy && now-leaseStart>=leaseDuration) {
            overridePolicy.reset();
            ++revision;
        }
        p=overridePolicy.value_or(current);
        version=revision;
    }
    const auto wall=clockStamp().utc;
    tm local{};
    const auto* calendar=wall ? localtime_r(&wall,&local) : nullptr;
    std::optional<int> dailyRem = calendar && p.daily ? dailySeconds(wall, local, p.deadlineMinute) : std::nullopt;
    PredictiveInputs pred{};
    if (p.predictive && wall) {
        const int effMin = std::max(0, p.deadlineMinute - p.anticipationMin);
        const int remS = dailySeconds(wall, local, effMin).value_or(0);
        pred = readPredictiveInputs(wall, wall + remS);
    }
    const auto result=manager.tick(state,p,version,now,calendar,
        calendar && p.strategy==4 && p.daily ? dailyRem : std::nullopt,
        p.predictive ? &pred : nullptr);
    setHaUsage({result.usedForecast, result.usedRatio, result.usedWeather, result.usedLoadProfile});
    auto line=audit(state,p,result,now,seq,"MEASURED");
    line.pop_back();
    line+=",\"ha_inputs\":"+haSummary(false)+"}";
    recordDecision(state,now,line);
}
void registerEms(httpd_handle_t server,bool (*authorize)(httpd_req_t*)) {
    checkAuth=authorize;
    const httpd_uri_t routes[]={
        {"/api/v1/ems/config",HTTP_GET,getPolicy,nullptr},
        {"/api/v1/ems/config",HTTP_POST,setPolicy,nullptr},
        {"/api/v1/ems/replay",HTTP_GET,replay,nullptr},
    };
    for (const auto& route:routes)
        if (httpd_register_uri_handler(server,&route)!=ESP_OK) recordLog("EMS REST route registration failed");
}
}
