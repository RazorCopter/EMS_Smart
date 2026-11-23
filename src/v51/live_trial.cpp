// SPDX-License-Identifier: MIT
#include "live_trial.h"
#include "drivers/afore/v51_reader.h"
#include "live_executor.h"
#include "ems_runtime.h"
#include "clock_service.h"
#include "trainer_response.h"
#include "remote.h"

#include <cstdio>
#include <atomic>
#include <mutex>

namespace heliograph::v51 {
namespace {
Policy trialPolicy;
uint32_t durationMs = 1800000;
enum class Request { None, Preflight, Start, Stop };
std::mutex trialMutex;
std::atomic<bool> active{false};
Request pending = Request::None;
std::optional<uint16_t> nativeMode;
std::optional<afore::ExternalEmsActuator> actuator;
std::optional<LiveExecutor> executor;
std::optional<int> confirmedWatts;
bool trainer = true;
TrainerResponse response;
unsigned sampleSeconds = 2;
uint32_t started = 0, lastWrite = 0, confirmedCount = 0, lastTrace = 0;
uint32_t lastModeSwitch = 0;
unsigned dailyModeSwitches = 0, accountedModeWrites = 0;
int switchDay = -1;
const char* status = "PREFLIGHT_REQUIRED";

void accountModeWrites(uint32_t now) {
    const auto utc = clockStamp().utc;
    std::tm local{};
    const int day = utc && localtime_r(&utc, &local)
        ? (local.tm_year + 1900) * 366 + local.tm_yday
        : int(now / 86400000u);
    if (switchDay != day) {
        if (switchDay >= 0 && (switchDay < 366) == (day < 366) && day > switchDay)
            dailyModeSwitches = 0;
        switchDay = day;
    }
    if (actuator && actuator->modeWriteAttempts() > accountedModeWrites) {
        dailyModeSwitches += actuator->modeWriteAttempts() - accountedModeWrites;
        accountedModeWrites = actuator->modeWriteAttempts();
        lastModeSwitch = now;
    }
}

uint32_t cooldownRemaining(uint32_t now) {
    const auto cooldown = uint32_t(trialPolicy.nativeSwitchCooldownSec) * 1000;
    return cooldown - std::min(cooldown, now - lastModeSwitch);
}

void releaseSession(const char* reason) {
    accountModeWrites(clockStamp().uptime);
    status = reason;
    active.store(false);
    executor.reset();
    actuator.reset();
    nativeMode.reset();
    recordLog(reason);
}
}

bool liveTrialActive() { return active.load(); }

bool configureLiveTrainer(bool enabled, unsigned interval) {
    const std::lock_guard guard(trialMutex);
    if (executor || pending != Request::None || interval < 1 || interval > 30) return false;
    trainer = enabled;
    sampleSeconds = interval;
    return true;
}

bool requestLiveTrial(const char* action) {
    const std::lock_guard guard(trialMutex);
    if (strcmp(action, "SHADOW") == 0) {
        pending = Request::Stop;
        return true;
    }
    if (pending != Request::None || executor) return false;
    if (strcmp(action, "PREFLIGHT") == 0) {
        pending = Request::Preflight;
        return true;
    }
    if (strcmp(action, "LIVE") != 0) return false;
    pending = Request::Start;
    return true;
}

void appendLiveTrialStatus(JsonObject out) {
    const std::lock_guard guard(trialMutex);
    out["operating_mode"] = executor ? "LIVE" : "SHADOW";
    out["live_lock"] = status;
    out["live_trial_available"] = true;
    out["live_trainer"] = trainer;
    out["live_trainer_sample_s"] = sampleSeconds;
    out["live_trial_max_w"] = nullptr;
    out["live_trial_duration_s"] = nullptr;
    if (trainer) {
        out["live_trial_duration_s"] = durationMs / 1000;
    }
    out["live_trial_pending"] = pending != Request::None;
    out["live_delegated_native"] = executor && executor->nativeDelegated();
    out["mode_switches_today"] = dailyModeSwitches;
    out["mode_switch_cooldown_remaining_s"] = (cooldownRemaining(clockStamp().uptime) + 999) / 1000;
    out["live_trial_confirmed_count"] = confirmedCount;
    out["modbus_write_count"] = modbusWriteCount();
    out["live_trial_last_confirmed_uptime_ms"] = lastWrite;
    out["live_trial_w"] = nullptr;
    if (confirmedWatts) out["live_trial_w"] = *confirmedWatts;
    const auto& debug = response.diagnostic();
    out["trainer_sample_state"] = debug.sample;
    out["trainer_deadline"] = debug.deadline;
    out["trainer_verification"] = response.verificationState();
    out["trainer_command_age_ms"] = debug.commandAgeMs;
    out["trainer_pending_age_ms"] = debug.pendingAgeMs;
    out["trainer_sample_age_ms"] = debug.sampleAgeMs;
    out["trainer_physical_age_ms"] = nullptr;
    if (debug.physicalAgeMs) out["trainer_physical_age_ms"] = *debug.physicalAgeMs;
}

void serviceLiveTrial(AforeReader& reader, Decision decision, uint32_t now, bool healthy) {
    const std::lock_guard guard(trialMutex);
    accountModeWrites(now);
    const Request request = pending;
    pending = Request::None;
    if (request == Request::Stop && executor) {
        const auto result = executor->disarm();
        releaseSession(result.status == LiveStatus::Shadow ? "LIVE_TRIAL_ABORTED_NATIVE_RESTORED"
                                                          : "LIVE_TRIAL_RELEASE_FAILED");
        return;
    }
    if (request == Request::Stop && !executor) {
        nativeMode.reset();
        status = "PREFLIGHT_REQUIRED";
        return;
    }
    if (request == Request::Preflight) {
        nativeMode = healthy ? afore::ExternalEmsActuator::observedNativeMode(reader, reader.unit())
                             : std::nullopt;
        status = nativeMode ? "LIVE_PREFLIGHT_OK" : "LIVE_PREFLIGHT_FAILED";
        recordLog(status);
    }
    if (request == Request::Start) {
        nativeMode = healthy ? afore::ExternalEmsActuator::observedNativeMode(reader, reader.unit())
                             : std::nullopt;
        if (!nativeMode || !decision.finalWatts || !decision.lower || !decision.upper) {
            releaseSession("LIVE_TRIAL_START_REFUSED");
            return;
        }
        accountedModeWrites = 0;
        actuator.emplace(reader, reader.unit(), *nativeMode, true, recordLog);
        executor.emplace(*actuator);
        trialPolicy = savedPolicy();
        durationMs = uint32_t(trialPolicy.trainerSec) * 1000;
        const auto lease = trainer ? std::optional<uint32_t>{durationMs} : std::nullopt;
        const bool selfUse = decision.strategy == Strategy::SelfUse;
        if (selfUse && nativeMode != 1) {
            releaseSession("LIVE_NATIVE_SELF_USE_UNAVAILABLE");
            return;
        }
        if ((!selfUse && !actuator->enterExternalBatteryControl()) ||
            !executor->arm(now, lease, selfUse)) {
            releaseSession(actuator->release() ? "LIVE_TRIAL_START_REFUSED" : "LIVE_TRIAL_RELEASE_FAILED");
            return;
        }
        confirmedCount = lastWrite = 0;
        confirmedWatts.reset();
        lastTrace = now - sampleSeconds * 1000;
        response = TrainerResponse{trialPolicy};
        started = now;
        active.store(true);
        accountModeWrites(now);
        status = trainer ? "LIVE_TRIAL_RUNNING" : "LIVE_RUNNING";
        recordLog(status);
        if (selfUse) recordLog("LIVE_NATIVE_DELEGATED");
    }
    if (!executor) return;

    if (trainer && now - started >= durationMs) {
        const auto result = executor->disarm();
        releaseSession(result.status == LiveStatus::Shadow ? "LIVE_TRAINER_COMPLETE_NATIVE_RESTORED"
                                                          : "LIVE_TRIAL_RELEASE_FAILED");
        return;
    }
    if (!healthy) decision.finalWatts.reset();
    const bool usable = decision.finalWatts && decision.lower && decision.upper &&
        *decision.finalWatts >= *decision.lower && *decision.finalWatts <= *decision.upper &&
        decision.strategy != Strategy::Safe && decision.strategy != Strategy::Native;
    const bool selfUse = decision.strategy == Strategy::SelfUse;
    if (usable && selfUse && !executor->nativeDelegated()) {
        if (nativeMode != 1 ||
            !nativeDelegationBudgetAllows(cooldownRemaining(now), dailyModeSwitches,
                                         unsigned(trialPolicy.maxModeSwitchesPerDay))) {
            const auto stopped = executor->disarm();
            releaseSession(stopped.status == LiveStatus::Shadow ? "LIVE_NATIVE_SELF_USE_UNAVAILABLE"
                                                               : "LIVE_TRIAL_RELEASE_FAILED");
            return;
        }
        if (executor->delegateNative(now).status != LiveStatus::NativeDelegated) {
            releaseSession("LIVE_TRIAL_RELEASE_FAILED");
            return;
        }
        accountModeWrites(now);
        confirmedWatts.reset();
        response = TrainerResponse{trialPolicy};
        recordLog("LIVE_NATIVE_DELEGATED");
    } else if (usable && !selfUse && executor->nativeDelegated()) {
        const auto resumed = executor->resumeExternal(now);
        if (resumed.status != LiveStatus::Armed) {
            releaseSession(resumed.status == LiveStatus::ReleaseFailed ? "LIVE_TRIAL_RELEASE_FAILED"
                                                                     : "LIVE_TRIAL_FAULT_STOPPED");
            return;
        }
        accountModeWrites(now);
        recordLog("LIVE_NATIVE_RESUMED_EXTERNAL");
    }
    if (trainer && !executor->nativeDelegated() && confirmedWatts && usable) {
        const auto& sample = reader.state().battery;
        const auto measured = sample.usable(now);
        const auto result = response.observe(measured.value_or(NAN), sample.stamp, now);
        const auto& debug = response.diagnostic();
        if (result != TrainerResponse::Result::Waiting || now - lastTrace >= sampleSeconds * 1000) {
            lastTrace = now;
            char trace[480]{};
            snprintf(trace, sizeof(trace),
                     "TRAINER_DEBUG tick_ms=%u strategy=%s phase=%s disposition=%s safety=%s candidate_w=%d request_w=%d lower_w=%d upper_w=%d expected_w=%d measured_w=%.1f tolerance_w=%.1f sample=%s deadline=%s sample_age_ms=%u command_age_ms=%u pending_age_ms=%u physical_age_ms=%d healthy=%d",
                     unsigned(now), strategyName(decision.strategy), decision.phase, decision.disposition,
                     decision.safetyReason, decision.candidate.value_or(INT32_MIN),
                     decision.finalWatts.value_or(INT32_MIN), decision.lower.value_or(INT32_MIN),
                     decision.upper.value_or(INT32_MIN), *confirmedWatts, measured.value_or(NAN),
                     debug.toleranceW, debug.sample, debug.deadline, unsigned(debug.sampleAgeMs),
                     unsigned(debug.commandAgeMs), unsigned(debug.pendingAgeMs),
                     debug.physicalAgeMs ? int(*debug.physicalAgeMs) : -1, healthy);
            recordLog(trace);
            const auto& energy = reader.state();
            char powerTrace[320]{};
            snprintf(powerTrace, sizeof(powerTrace),
                     "TRAINER_POWER tick_ms=%u grid_w=%.1f grid_ms=%u pv_w=%.1f pv_ms=%u "
                     "inverter_w=%.1f inverter_ms=%u battery_w=%.1f battery_ms=%u "
                     "battery_v=%.2f battery_v_ms=%u battery_a=%.2f battery_a_ms=%u",
                     unsigned(now), energy.grid.usable(now).value_or(NAN), unsigned(energy.grid.stamp),
                     energy.pv.usable(now).value_or(NAN), unsigned(energy.pv.stamp),
                     energy.inverter.usable(now).value_or(NAN), unsigned(energy.inverter.stamp),
                     measured.value_or(NAN), unsigned(sample.stamp),
                     energy.batteryVoltage.usable(now).value_or(NAN), unsigned(energy.batteryVoltage.stamp),
                     energy.batteryCurrent.usable(now).value_or(NAN), unsigned(energy.batteryCurrent.stamp));
            recordLog(powerTrace);
        }
        if (result != TrainerResponse::Result::Waiting) {
            char line[120]{};
            using Result = TrainerResponse::Result;
            const char* event = "MATCH";
            switch (result) {
                case Result::Warning: event = "WARNING"; break;
                case Result::Recover: event = "ERROR"; break;
                case Result::Recovered: event = "RECOVERED"; break;
                case Result::Mismatch: event = "CRITICAL"; break;
                default: break;
            }
            snprintf(line, sizeof(line), "TRAINER %s expected_w=%d measured_w=%.1f reason=%s",
                     event, *confirmedWatts, measured.value_or(NAN), debug.deadline);
            recordLog(line);
            if (result == TrainerResponse::Result::Mismatch) {
                const auto stop = executor->disarm();
                releaseSession(stop.status == LiveStatus::Shadow ? "LIVE_TRAINER_MISMATCH_NATIVE_RESTORED"
                                                                 : "LIVE_TRIAL_RELEASE_FAILED");
                return;
            }
        }
    }
    if (trainer && !executor->nativeDelegated() && usable) {
        const int lower = std::max(*decision.lower, -trialPolicy.trainerDischargeW);
        const int upper = std::min(*decision.upper, trialPolicy.trainerChargeW);
        if (lower > upper) decision.finalWatts.reset();
        else {
            const int requested = std::clamp(*decision.finalWatts, lower, upper);
            const int selected = response.verificationSetpoint(requested, lower, upper, now);
            decision.finalWatts = selected;
            if (selected != requested && now == lastTrace) {
                char line[160]{};
                snprintf(line, sizeof(line),
                         "TRAINER_VERIFY_HOLD tick_ms=%u requested_w=%d held_w=%d lower_w=%d upper_w=%d",
                         unsigned(now), requested, selected, lower, upper);
                recordLog(line);
            }
        }
    }
    const auto result = executor->tick(decision, now, now, 1000);
    if (result.status == LiveStatus::Holding || result.status == LiveStatus::NativeDelegated) return;
    if (result.status != LiveStatus::Applied) {
        char errLine[96]{};
        snprintf(errLine, sizeof(errLine), "LIVE_TRIAL stop status=%d actuator=%s",
                 static_cast<int>(result.status), actuator ? actuator->lastError() : "NONE");
        recordLog(errLine);
        releaseSession(result.status == LiveStatus::ReleaseFailed ? "LIVE_TRIAL_RELEASE_FAILED"
                                                                 : "LIVE_TRIAL_FAULT_STOPPED");
        return;
    }
    confirmedWatts = result.confirmedWatts;
    if (trainer) response.confirmed(*confirmedWatts, now);
    lastWrite = now;
    ++confirmedCount;
    char line[100]{};
    snprintf(line, sizeof(line), "LIVE_TRIAL confirmed=%u ems_w=%d",
             static_cast<unsigned>(confirmedCount), *confirmedWatts);
    recordLog(line);
}
}
