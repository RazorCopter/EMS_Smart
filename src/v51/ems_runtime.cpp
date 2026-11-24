// SPDX-License-Identifier: MIT
#include "ems_runtime.h"
#include "clock_service.h"
#include "remote.h"
#include "m4.h"
#include "m5.h"
#include "live_trial.h"
#include "transport/v51_read_only.h"
#include <array>
#include <Arduino.h>
#include <cstring>
#include <memory>
#include <mutex>
#include <new>
#include <sstream>
#include <charconv>

namespace heliograph::v51 {
namespace {
struct Field { const char* name; int Policy::* member; };
constexpr Field fields[]={
    {"charge_a",&Policy::chargeA},{"discharge_a",&Policy::dischargeA},
    {"voltage_min",&Policy::minVolt},{"voltage_max",&Policy::maxVolt},{"soc_resume",&Policy::resumeSoc},
    {"damping_w",&Policy::dampingW},{"damping_pct",&Policy::dampingPct},
    {"trainer_settle_ms", &Policy::trainerSettleMs},
    {"trainer_abs_w", &Policy::trainerAbsW},
    {"trainer_pct", &Policy::trainerPct},
    {"trainer_timeout_ms", &Policy::trainerTimeoutMs},
    {"trainer_gap_ms", &Policy::trainerGapMs},
    {"trainer_s", &Policy::trainerSec},
    {"trainer_critical_w", &Policy::trainerCriticalW},
    {"trainer_critical_pct", &Policy::trainerCriticalPct},
    {"trainer_recovery_ms", &Policy::trainerRecoveryMs},
    {"trainer_recovery_pct", &Policy::trainerRecoveryPct},
    {"trainer_charge_w", &Policy::trainerChargeW},
    {"trainer_discharge_w", &Policy::trainerDischargeW},
    {"delegate_native_self_use", &Policy::delegateNativeSelfUse},
    {"native_switch_cooldown_s", &Policy::nativeSwitchCooldownSec},
    {"max_mode_switches_per_day", &Policy::maxModeSwitchesPerDay},
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
PredictiveCycleState savedCycleState{};
bool hasSavedCycleState=false;
bool cycleStateRestored=false;

template<typename T> void number(std::ostream& out,std::optional<T> value) {
    if (value) out<<*value;
    else out<<"null";
}
std::string audit(const EnergyState& e,const Policy& p,const Decision& d,
                  uint32_t now,uint32_t seq,const char* provenance,
                  bool executed=false,std::optional<int> applied=std::nullopt) {
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
    out<<",\"reason\":\""<<d.reason<<"\",\"disposition\":\""<<d.disposition
       <<"\",\"executed\":"<<(executed?"true":"false")<<",\"applied\":";
    number(out,applied);
    out<<",\"aux1\":\""<<(auxOn()?"ON":"OFF")
       <<"\",\"modbus_write_count\":"<<(strcmp(provenance,"MEASURED")==0?modbusWriteCount():0)<<"}";
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
const char* cyclePhaseLabel(uint8_t phase) {
    switch (phase) {
    case 0: return "SELF_USE";
    case 1: return "GENTLE_TARGET_CHARGE";
    case 2: return "CAPTURE_CHARGE_FIRST";
    case 3: return "FULL_HOLD_5M";
    default: return "UNKNOWN";
    }
}
esp_err_t replayM9b(httpd_req_t* r) {
    if (!checkAuth(r)) return ESP_OK;
    struct ReplayCtx {
        StrategyManager mgr;
        StrategyManager mgr2;
        Policy p;
        Policy p2;
        EnergyState e;
        PredictiveInputs pred;
        tm cal{};
    };
    std::unique_ptr<ReplayCtx> ctx(new (std::nothrow) ReplayCtx());
    if (!ctx) return httpd_resp_send_err(r, HTTPD_500_INTERNAL_SERVER_ERROR, "OOM");
    httpd_resp_set_type(r, "application/x-ndjson");
    httpd_resp_set_hdr(r, "Cache-Control", "no-store");

    auto basePredPolicy = []() {
        Policy p;
        p.predictive = 1;
        p.autoSeason = 1;
        p.summerFloor = 10;
        p.winterFloor = 20;
        p.fullHoldSec = 300;
        p.weekdayLoadW = 600;
        p.weekendLoadW = 900;
        p.lowSunKwh = 8;
        p.anticipationMin = 20;
        p.safetyMarginPct = 115;
        p.charge = 2560;
        p.discharge = 2560;
        p.chargeFirstW = 2560;
        p.targetW = 2560;
        p.chargeA = 50;
        p.dischargeA = 50;
        p.nominalMv = 51200;
        p.ramp = 20000;
        return p;
    };
    auto makeState = [](double soc, double gridW, uint32_t stampMs, double pvW = 0.0, double loadW = 0.0) {
        EnergyState e;
        e.goodReads = 1;
        e.soc = {soc, stampMs, Quality::Valid};
        e.grid = {gridW, stampMs, Quality::Valid};
        e.battery = {0.0, stampMs, Quality::Valid};
        e.batteryVoltage = {52.0, stampMs, Quality::Valid};
        e.pv = {pvW > 0.0 ? pvW : (gridW < 0 ? -gridW + 300.0 : 0.0), stampMs, Quality::Valid};
        e.load = {loadW > 0.0 ? loadW : (gridW >= 0 ? gridW : 300.0), stampMs, Quality::Valid};
        return e;
    };
    auto makeDay = [](int monZeroBased, int mday, int yday, int wday, int hour, int min) {
        tm t{};
        t.tm_year = 2026 - 1900;
        t.tm_mon = monZeroBased;
        t.tm_mday = mday;
        t.tm_yday = yday;
        t.tm_wday = wday;
        t.tm_hour = hour;
        t.tm_min = min;
        t.tm_isdst = -1;
        return t;
    };
    uint32_t seqId = 1;
    auto emit = [&](const char* scenario, const char* step, const char* initialPhase,
                    StrategyManager& m, const EnergyState& e, const Policy& p, uint32_t rev,
                    uint32_t nowMs, const tm* cal, const PredictiveInputs* pi) -> bool {
        const Decision d = m.tick(e, p, rev, nowMs, cal, std::nullopt, pi);
        const auto baseJson = audit(e, p, d, nowMs, seqId++, "REPLAY_M9B");
        std::ostringstream out;
        out << "{\"scenario\":\"" << scenario << "\",\"step\":\"" << step
            << "\",\"initial_phase\":\"" << initialPhase << "\",\"input\":{\"soc\":"
            << e.soc.value.value_or(0.0) << ",\"grid_w\":" << e.grid.value.value_or(0.0)
            << ",\"predictive\":" << p.predictive << ",\"strategy\":\""
            << strategyName(static_cast<Strategy>(p.strategy)) << "\",\"grid_charge\":"
            << p.gridCharge << ",\"surplus_only\":" << p.surplusOnly;
        if (cal) {
            out << ",\"time\":\"" << (cal->tm_mon + 1) << "/" << cal->tm_mday << " ";
            if (cal->tm_hour < 10) out << "0";
            out << cal->tm_hour << ":";
            if (cal->tm_min < 10) out << "0";
            out << cal->tm_min << "\"";
        }
        if (pi) {
            out << ",\"today_kwh\":" << pi->todayTotalKwh << ",\"tomorrow_kwh\":" << pi->tomorrowTotalKwh
                << ",\"rem_kwh\":" << pi->remainingForecastKwh << ",\"fog\":"
                << (pi->fog ? "true" : "false") << ",\"weather_valid\":"
                << (pi->weatherValid ? "true" : "false");
        }
        out << "}," << baseJson.substr(1) << "\n";
        const auto line = out.str();
        return httpd_resp_sendstr_chunk(r, line.c_str()) == ESP_OK;
    };

    // 1. Summer 15:15, SOC 95%
    ctx->mgr = StrategyManager{};
    ctx->p = basePredPolicy();
    ctx->cal = makeDay(6, 15, 195, 3, 14, 0);
    ctx->pred = PredictiveInputs{};
    ctx->pred.forecastValid = true;
    ctx->pred.todayTotalKwh = 18.0;
    ctx->pred.tomorrowTotalKwh = 16.0;
    ctx->pred.remainingForecastKwh = 3.0;
    ctx->pred.loadProfileValid = true;
    ctx->pred.loadTodayKw = 0.6;
    ctx->e = makeState(85, -1200, 10000);
    if (!emit("summer_1515_soc95", "init_1400_soc85", "INIT", ctx->mgr, ctx->e, ctx->p, 1, 10000, &ctx->cal, &ctx->pred)) return ESP_FAIL;
    ctx->cal.tm_hour = 15; ctx->cal.tm_min = 15;
    ctx->pred.remainingForecastKwh = 0.2;
    ctx->e = makeState(95, -400, 14000);
    if (!emit("summer_1515_soc95", "at_1515_soc95", cyclePhaseLabel(ctx->mgr.exportPredictiveState().phase), ctx->mgr, ctx->e, ctx->p, 1, 14000, &ctx->cal, &ctx->pred)) return ESP_FAIL;

    // 2. Winter SOC 35%, today and tomorrow favorable
    ctx->mgr = StrategyManager{};
    ctx->p = basePredPolicy();
    ctx->cal = makeDay(10, 19, 322, 4, 10, 0);
    ctx->pred = PredictiveInputs{};
    ctx->pred.forecastValid = true;
    ctx->pred.todayTotalKwh = 16.0;
    ctx->pred.tomorrowTotalKwh = 15.0;
    ctx->pred.remainingForecastKwh = 14.0;
    ctx->e = makeState(35, -2000, 10000);
    if (!emit("winter_soc35_favorable", "evaluate", "INIT", ctx->mgr, ctx->e, ctx->p, 1, 10000, &ctx->cal, &ctx->pred)) return ESP_FAIL;

    // 3. Fog active then weather stale > 1h
    ctx->mgr = StrategyManager{};
    ctx->p = basePredPolicy();
    ctx->cal = makeDay(10, 19, 322, 4, 10, 0);
    ctx->pred = PredictiveInputs{};
    ctx->pred.forecastValid = true;
    ctx->pred.todayTotalKwh = 15.0;
    ctx->pred.tomorrowTotalKwh = 15.0;
    ctx->pred.remainingForecastKwh = 14.0;
    ctx->pred.weatherValid = true;
    ctx->pred.fog = true;
    ctx->e = makeState(35, -1800, 10000);
    if (!emit("fog_active_then_stale_1h", "fog_active", "INIT", ctx->mgr, ctx->e, ctx->p, 1, 10000, &ctx->cal, &ctx->pred)) return ESP_FAIL;
    ctx->pred.weatherValid = false;
    ctx->e = makeState(35, -1800, 10000 + 3600000u);
    if (!emit("fog_active_then_stale_1h", "weather_stale_1h", cyclePhaseLabel(ctx->mgr.exportPredictiveState().phase), ctx->mgr, ctx->e, ctx->p, 1, 10000 + 3600000u, &ctx->cal, &ctx->pred)) return ESP_FAIL;

    // 4. Day rollover (deadline -> SelfUse -> next morning GentleCharge)
    ctx->mgr = StrategyManager{};
    ctx->p = basePredPolicy();
    ctx->cal = makeDay(6, 15, 195, 3, 15, 15);
    ctx->pred = PredictiveInputs{};
    ctx->pred.forecastValid = true;
    ctx->pred.todayTotalKwh = 18.0;
    ctx->pred.tomorrowTotalKwh = 16.0;
    ctx->pred.remainingForecastKwh = 0.2;
    ctx->e = makeState(95, -400, 10000);
    ctx->mgr.tick(ctx->e, ctx->p, 1, 10000, &ctx->cal, std::nullopt, &ctx->pred);
    ctx->cal.tm_hour = 15; ctx->cal.tm_min = 30;
    ctx->pred.remainingForecastKwh = 0.0;
    ctx->e = makeState(95, 500, 14000);
    if (!emit("day_rollover", "day1_deadline_1530", cyclePhaseLabel(ctx->mgr.exportPredictiveState().phase), ctx->mgr, ctx->e, ctx->p, 1, 14000, &ctx->cal, &ctx->pred)) return ESP_FAIL;
    ctx->cal.tm_mday += 1; ctx->cal.tm_yday += 1; ctx->cal.tm_wday = 4;
    ctx->cal.tm_hour = 9; ctx->cal.tm_min = 0;
    ctx->pred.remainingForecastKwh = 15.0;
    ctx->e = makeState(55, -1800, 18000);
    if (!emit("day_rollover", "day2_0900_soc55", cyclePhaseLabel(ctx->mgr.exportPredictiveState().phase), ctx->mgr, ctx->e, ctx->p, 1, 18000, &ctx->cal, &ctx->pred)) return ESP_FAIL;

    // 5. Reboot during SelfUse of cycle
    ctx->mgr = StrategyManager{};
    ctx->p = basePredPolicy();
    ctx->cal = makeDay(10, 19, 322, 4, 10, 0);
    ctx->pred = PredictiveInputs{};
    ctx->pred.forecastValid = true;
    ctx->pred.todayTotalKwh = 4.77;
    ctx->pred.tomorrowTotalKwh = 5.0;
    ctx->pred.remainingForecastKwh = 2.5;
    ctx->pred.weatherValid = true;
    ctx->pred.fog = true;
    uint32_t tb = 50000;
    ctx->mgr.tick(makeState(100, -200, tb), ctx->p, 1, tb, &ctx->cal, std::nullopt, &ctx->pred);
    for (int i = 0; i < 76; ++i) {
        tb += 4000;
        ctx->mgr.tick(makeState(100, 500, tb), ctx->p, 1, tb, &ctx->cal, std::nullopt, &ctx->pred);
    }
    const auto savedSelfUse = ctx->mgr.exportPredictiveState();
    ctx->mgr2 = StrategyManager{};
    ctx->mgr2.restorePredictiveState(savedSelfUse);
    ctx->e = makeState(50, 500, 1000, 0.0, 500.0);
    if (!emit("reboot_during_cycle_self_use", "post_reboot_soc50", cyclePhaseLabel(savedSelfUse.phase), ctx->mgr2, ctx->e, ctx->p, 1, 1000, &ctx->cal, &ctx->pred)) return ESP_FAIL;

    // 6. Reboot during FullHold
    ctx->mgr = StrategyManager{};
    ctx->mgr.tick(makeState(100, -200, 10000), ctx->p, 1, 10000, &ctx->cal, std::nullopt, &ctx->pred);
    ctx->mgr.tick(makeState(100, -200, 14000), ctx->p, 1, 14000, &ctx->cal, std::nullopt, &ctx->pred);
    const auto savedHold = ctx->mgr.exportPredictiveState();
    ctx->mgr2 = StrategyManager{};
    ctx->mgr2.restorePredictiveState(savedHold);
    ctx->e = makeState(100, -200, 1000);
    if (!emit("reboot_during_full_hold", "post_reboot_soc100", cyclePhaseLabel(savedHold.phase), ctx->mgr2, ctx->e, ctx->p, 1, 1000, &ctx->cal, &ctx->pred)) return ESP_FAIL;

    // 7. Stale telemetry during FullHold (resets hold timer, does not finish early)
    ctx->mgr = StrategyManager{};
    uint32_t th = 10000;
    if (!emit("stale_telemetry_during_full_hold", "hold_start", "INIT", ctx->mgr, makeState(100, -200, th), ctx->p, 1, th, &ctx->cal, &ctx->pred)) return ESP_FAIL;
    th += 4000; ctx->mgr.tick(makeState(100, -200, th), ctx->p, 1, th, &ctx->cal, std::nullopt, &ctx->pred);
    th += 4000; ctx->mgr.tick(makeState(100, -200, th), ctx->p, 1, th, &ctx->cal, std::nullopt, &ctx->pred);
    th += 20000;
    if (!emit("stale_telemetry_during_full_hold", "stale_gap", cyclePhaseLabel(ctx->mgr.exportPredictiveState().phase), ctx->mgr, makeState(100, -200, 10000), ctx->p, 1, th, &ctx->cal, &ctx->pred)) return ESP_FAIL;
    for (int i = 0; i < 74; ++i) {
        th += 4000;
        ctx->mgr.tick(makeState(100, -200, th), ctx->p, 1, th, &ctx->cal, std::nullopt, &ctx->pred);
    }
    if (!emit("stale_telemetry_during_full_hold", "resumed_297s_still_holding", cyclePhaseLabel(ctx->mgr.exportPredictiveState().phase), ctx->mgr, makeState(100, -200, th), ctx->p, 1, th, &ctx->cal, &ctx->pred)) return ESP_FAIL;
    th += 4000;
    if (!emit("stale_telemetry_during_full_hold", "completed_full_300s", cyclePhaseLabel(ctx->mgr.exportPredictiveState().phase), ctx->mgr, makeState(100, 500, th), ctx->p, 1, th, &ctx->cal, &ctx->pred)) return ESP_FAIL;

    // 8. Secondary threshold modification without losing predictive phase
    ctx->mgr = StrategyManager{};
    ctx->mgr.restorePredictiveState(savedSelfUse);
    ctx->e = makeState(60, 600, 400000, 0.0, 600.0);
    if (!emit("secondary_threshold_change_keeps_phase", "before_edit", cyclePhaseLabel(savedSelfUse.phase), ctx->mgr, ctx->e, ctx->p, 1, 400000, &ctx->cal, &ctx->pred)) return ESP_FAIL;
    ctx->p2 = ctx->p;
    ctx->p2.lowSunKwh = 10;
    ctx->p2.deadband = 30;
    ctx->p2.anticipationMin = 25;
    ctx->e = makeState(60, 600, 404000, 0.0, 600.0);
    if (!emit("secondary_threshold_change_keeps_phase", "after_edit_rev2", cyclePhaseLabel(ctx->mgr.exportPredictiveState().phase), ctx->mgr, ctx->e, ctx->p2, 2, 404000, &ctx->cal, &ctx->pred)) return ESP_FAIL;

    // 9. resumeSoc distinct from floor+hysteresis (manual + seasonal predictive)
    ctx->mgr = StrategyManager{};
    ctx->p = basePredPolicy();
    ctx->p.predictive = 0; ctx->p.strategy = 0; ctx->p.floor = 20; ctx->p.hysteresis = 2; ctx->p.resumeSoc = 25;
    if (!emit("resume_soc_distinct_from_floor_hyst", "manual_soc20_reserve", "MANUAL", ctx->mgr, makeState(20, 400, 1000, 0, 400), ctx->p, 1, 1000, nullptr, nullptr)) return ESP_FAIL;
    if (!emit("resume_soc_distinct_from_floor_hyst", "manual_soc25_still_locked", "MANUAL", ctx->mgr, makeState(25, 400, 2000, 0, 400), ctx->p, 1, 2000, nullptr, nullptr)) return ESP_FAIL;
    if (!emit("resume_soc_distinct_from_floor_hyst", "manual_soc26_unlocked", "MANUAL", ctx->mgr, makeState(26, 400, 3000, 0, 400), ctx->p, 1, 3000, nullptr, nullptr)) return ESP_FAIL;
    ctx->mgr2 = StrategyManager{};
    ctx->p2 = ctx->p; ctx->p2.predictive = 1;
    ctx->cal = makeDay(6, 15, 195, 3, 17, 0);
    ctx->pred = PredictiveInputs{}; ctx->pred.forecastValid = true; ctx->pred.todayTotalKwh = 18.0; ctx->pred.tomorrowTotalKwh = 16.0;
    ctx->mgr2.tick(makeState(30, 400, 1000, 0, 400), ctx->p2, 1, 1000, &ctx->cal, std::nullopt, &ctx->pred);
    if (!emit("resume_soc_distinct_from_floor_hyst", "pred_summer_soc14_allows_discharge", cyclePhaseLabel(ctx->mgr2.exportPredictiveState().phase), ctx->mgr2, makeState(14, 400, 2000, 0, 400), ctx->p2, 1, 2000, &ctx->cal, &ctx->pred)) return ESP_FAIL;
    ctx->cal = makeDay(10, 19, 322, 4, 17, 0);
    ctx->mgr2.tick(makeState(30, 400, 3000, 0, 400), ctx->p2, 2, 3000, &ctx->cal, std::nullopt, &ctx->pred);
    if (!emit("resume_soc_distinct_from_floor_hyst", "pred_winter_soc20_blocked", cyclePhaseLabel(ctx->mgr2.exportPredictiveState().phase), ctx->mgr2, makeState(20, 400, 4000, 0, 400), ctx->p2, 2, 4000, &ctx->cal, &ctx->pred)) return ESP_FAIL;

    // 10. Effective discharge_w = 2560 W
    ctx->p = basePredPolicy();
    ctx->p.predictive = 0; ctx->p.strategy = 0; ctx->p.discharge = 2560; ctx->p.dischargeA = 50;
    ctx->mgr = StrategyManager{};
    if (!emit("effective_discharge_2560w", "exact_2560w", "MANUAL", ctx->mgr, makeState(60, 2560, 1000, 0, 2560), ctx->p, 1, 1000, nullptr, nullptr)) return ESP_FAIL;
    ctx->mgr2 = StrategyManager{};
    if (!emit("effective_discharge_2560w", "capped_at_2560w", "MANUAL", ctx->mgr2, makeState(60, 3000, 1000, 0, 3000), ctx->p, 1, 1000, nullptr, nullptr)) return ESP_FAIL;

    // 11-15. Decision disposition cases (M9B #2)
    ctx->p = basePredPolicy();
    ctx->p.predictive = 0; ctx->p.chargeA = 0;
    ctx->p.strategy = 1; ctx->p.surplusOnly = 1; ctx->p.gridCharge = 0;
    ctx->mgr = StrategyManager{};
    if (!emit("disp_charge_first_no_surplus", "eval", "MANUAL", ctx->mgr, makeState(50, 500, 1000, 0, 500), ctx->p, 1, 1000, nullptr, nullptr)) return ESP_FAIL;

    ctx->p.surplusOnly = 0; ctx->p.gridCharge = 0;
    ctx->mgr = StrategyManager{};
    if (!emit("disp_grid_charge_disabled", "eval", "MANUAL", ctx->mgr, makeState(50, 500, 1000, 0, 500), ctx->p, 1, 1000, nullptr, nullptr)) return ESP_FAIL;

    ctx->p.strategy = 0; ctx->p.floor = 20; ctx->p.resumeSoc = 25; ctx->p.dischargeA = 0;
    ctx->mgr = StrategyManager{};
    if (!emit("disp_candidate_zero_safety", "eval", "MANUAL", ctx->mgr, makeState(15, 800, 1000, 0, 800), ctx->p, 1, 1000, nullptr, nullptr)) return ESP_FAIL;

    ctx->p.strategy = 1; ctx->p.surplusOnly = 1; ctx->p.gridCharge = 0;
    ctx->mgr = StrategyManager{};
    if (!emit("disp_candidate_partial_surplus", "eval", "MANUAL", ctx->mgr, makeState(50, -1200, 1000, 1500, 300), ctx->p, 1, 1000, nullptr, nullptr)) return ESP_FAIL;

    ctx->p.strategy = 1; ctx->p.surplusOnly = 0; ctx->p.gridCharge = 1;
    ctx->mgr = StrategyManager{};
    if (!emit("disp_candidate_applicable", "eval", "MANUAL", ctx->mgr, makeState(50, 500, 1000, 0, 500), ctx->p, 1, 1000, nullptr, nullptr)) return ESP_FAIL;

    return httpd_resp_send_chunk(r, nullptr, 0);
}
}
Policy savedPolicy() {
    const std::lock_guard guard(lock);
    return current;
}

Policy activePolicy() {
    const std::lock_guard guard(lock);
    return overridePolicy.value_or(current);
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
            if (!storage || storage->getBytesLength("ems-v5-prev")!=sizeof(previous) ||
                storage->getBytes("ems-v5-prev",&previous,sizeof(previous))!=sizeof(previous) ||
                !previous.valid() || storage->putBytes("ems-policy-v5",&previous,sizeof(previous))!=sizeof(previous))
                return false;
            current=previous;
        }
        if (cancel==2 && (current.strategy==1 || current.strategy>=4)) {
            Policy next=current;
            next.strategy=current.strategy==1 ? 0 : current.fallback;
            if (!storage || storage->putBytes("ems-policy-v5",&next,sizeof(next))!=sizeof(next))
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
        if (!storage || storage->putBytes("ems-v5-prev",&current,sizeof(current))!=sizeof(current) ||
            storage->putBytes("ems-policy-v5",&next,sizeof(next))!=sizeof(next))
            return false;
        current=next;
        overridePolicy.reset();
    }
    ++revision;
    return true;
}
void loadPolicy(Preferences& preferences) {
    storage=&preferences;
    PredictiveCycleState cycleSaved{};
    if (preferences.getBytesLength("ems-pred-st") == sizeof(cycleSaved) &&
        preferences.getBytes("ems-pred-st", &cycleSaved, sizeof(cycleSaved)) == sizeof(cycleSaved) &&
        cycleSaved.magic == 0xA5 && cycleSaved.phase <= 3) {
        savedCycleState = cycleSaved;
        hasSavedCycleState = true;
    }

    auto unlockPowerCaps = [](Policy& p) {
        const int chgCapW = p.chargeA > 0 ? int(p.chargeA * (p.nominalMv / 1000.0)) : 2600;
        const int disCapW = p.dischargeA > 0 ? int(p.dischargeA * (p.nominalMv / 1000.0)) : 2600;
        if (p.charge <= 1000 && chgCapW > 1000) p.charge = chgCapW;
        if (p.chargeFirstW <= 1000 && chgCapW > 1000) p.chargeFirstW = chgCapW;
        if (p.targetW <= 1000 && chgCapW > 1000) p.targetW = chgCapW;
        if (p.discharge <= 1000 && disCapW > 1000) p.discharge = disCapW;
        if (p.chargeFirstSec < 43200) p.chargeFirstSec = 43200;
    };

    constexpr size_t v2Size = offsetof(Policy, predictive);
    Policy saved;
    for (const char* key : {"ems-policy-v5", "ems-v5-prev"}) {
        if (preferences.getBytesLength(key) == sizeof(saved) &&
            preferences.getBytes(key, &saved, sizeof(saved)) == sizeof(saved) && saved.valid()) {
            current = saved;
            return;
        }
    }
    for (const char* key : {"ems-policy-v4", "ems-v4-prev", "ems-policy-v3"}) {
        const auto size = preferences.getBytesLength(key);
        if (size != offsetof(Policy, dampingW) && size != offsetof(Policy, delegateNativeSelfUse) &&
            size != offsetof(Policy, trainerCriticalW)) continue;
        Policy previous;
        if (preferences.getBytes(key, &previous, size) != size) continue;
        previous.trainerCriticalW = std::max(previous.trainerCriticalW, previous.trainerAbsW);
        if (strcmp(key, "ems-policy-v3") == 0) unlockPowerCaps(previous);
        if (!previous.valid()) continue;
        current = previous;
        preferences.putBytes("ems-policy-v5", &current, sizeof(current));
        return;
    }
    // Migrate v2 policy into predictive-capable v4 preserving all user parameters
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
        unlockPowerCaps(migrated);
        if (migrated.valid()) {
            current = migrated;
            preferences.putBytes("ems-policy-v5", &current, sizeof(current));
            return;
        }
    }
    // Fresh board fallback
    current.predictive = 1;
    unlockPowerCaps(current);
}
Decision tickEms(const EnergyState& state,uint32_t now,uint32_t seq,bool recordNow) {
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
    if (hasSavedCycleState && !cycleStateRestored && p.predictive == 1 && (p.strategy == 0 || p.strategy == 1)) {
        manager.restorePredictiveState(savedCycleState);
        cycleStateRestored = true;
    }
    const auto wall=clockStamp().utc;
    tm local{};
    const auto* calendar=wall ? localtime_r(&wall,&local) : nullptr;
    std::optional<int> dailyRem = calendar && p.daily ? dailySeconds(wall, local, p.deadlineMinute) : std::nullopt;
    PredictiveInputs pred{};
    if (p.predictive && wall) {
        const int remS = dailySeconds(wall, local, p.deadlineMinute).value_or(0);
        pred = readPredictiveInputs(wall, wall + remS);
    }
    const auto result=manager.tick(state,p,version,now,calendar,
        calendar && p.strategy==4 && p.daily ? dailyRem : std::nullopt,
        p.predictive ? &pred : nullptr);
    if (p.predictive == 1 && (p.strategy == 0 || p.strategy == 1)) {
        const auto curSt = manager.exportPredictiveState();
        if (!hasSavedCycleState || curSt.phase != savedCycleState.phase ||
            curSt.discharging != savedCycleState.discharging || curSt.cycleDay != savedCycleState.cycleDay) {
            const std::lock_guard guard(lock);
            if (storage && storage->putBytes("ems-pred-st", &curSt, sizeof(curSt)) == sizeof(curSt)) {
                savedCycleState = curSt;
                hasSavedCycleState = true;
            }
        }
    }
    setHaUsage({result.usedForecast, result.usedRatio, result.usedWeather, result.usedLoadProfile});
    if (recordNow) recordEmsDecision(state,result,now,seq,false,std::nullopt);
    return result;
}
void recordEmsDecision(const EnergyState& state,const Decision& result,uint32_t now,uint32_t seq,
                       bool executed,std::optional<int> applied) {
    const auto p = activePolicy();
    auto line=audit(state,p,result,now,seq,"MEASURED",executed,applied);
    line.pop_back();
    line+=liveTrialActive()?",\"live_trial_active\":true":",\"live_trial_active\":false";
    line+=",\"ha_inputs\":"+haSummary(false)+"}";
    recordDecision(state,now,line);
}
void registerEms(httpd_handle_t server,bool (*authorize)(httpd_req_t*)) {
    checkAuth=authorize;
    const httpd_uri_t routes[]={
        {"/api/v1/ems/config",HTTP_GET,getPolicy,nullptr},
        {"/api/v1/ems/config",HTTP_POST,setPolicy,nullptr},
        {"/api/v1/ems/policy",HTTP_GET,getPolicy,nullptr},
        {"/api/v1/ems/policy",HTTP_POST,setPolicy,nullptr},
        {"/api/v1/ems/replay",HTTP_GET,replay,nullptr},
        {"/api/v1/ems/replay/m9b",HTTP_GET,replayM9b,nullptr},
    };
    for (const auto& route:routes)
        if (httpd_register_uri_handler(server,&route)!=ESP_OK) recordLog("EMS REST route registration failed");
}
}
