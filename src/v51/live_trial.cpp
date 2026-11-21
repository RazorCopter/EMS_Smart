// SPDX-License-Identifier: MIT
#include "live_trial.h"
#include "drivers/afore/v51_reader.h"
#include "live_executor.h"
#include "ems_runtime.h"
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
uint32_t started = 0, lastWrite = 0, confirmedCount = 0;
const char* status = "PREFLIGHT_REQUIRED";

void releaseSession(const char* reason) {
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
    out["live_trial_confirmed_count"] = confirmedCount;
    out["modbus_write_count"] = modbusWriteCount();
    out["live_trial_last_confirmed_uptime_ms"] = lastWrite;
    out["live_trial_w"] = nullptr;
    if (confirmedWatts) out["live_trial_w"] = *confirmedWatts;
    const auto& debug = response.diagnostic();
    out["trainer_sample_state"] = debug.sample;
    out["trainer_deadline"] = debug.deadline;
    out["trainer_command_age_ms"] = debug.commandAgeMs;
    out["trainer_pending_age_ms"] = debug.pendingAgeMs;
    out["trainer_sample_age_ms"] = debug.sampleAgeMs;
    out["trainer_physical_age_ms"] = nullptr;
    if (debug.physicalAgeMs) out["trainer_physical_age_ms"] = *debug.physicalAgeMs;
}

void serviceLiveTrial(AforeReader& reader, Decision decision, uint32_t now, bool healthy) {
    const std::lock_guard guard(trialMutex);
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
        actuator.emplace(reader, reader.unit(), *nativeMode, true);
        executor.emplace(*actuator);
        trialPolicy = savedPolicy();
        durationMs = uint32_t(trialPolicy.trainerSec) * 1000;
        const auto lease = trainer ? std::optional<uint32_t>{durationMs} : std::nullopt;
        if (!actuator->enterExternalBatteryControl() || !executor->arm(now, lease)) {
            releaseSession(actuator->release() ? "LIVE_TRIAL_START_REFUSED" : "LIVE_TRIAL_RELEASE_FAILED");
            return;
        }
        confirmedCount = lastWrite = 0;
        confirmedWatts.reset();
        response = TrainerResponse{trialPolicy};
        started = now;
        active.store(true);
        status = trainer ? "LIVE_TRIAL_RUNNING" : "LIVE_RUNNING";
        recordLog(status);
    }
    if (!executor) return;

    if (trainer && now - started >= durationMs) {
        const auto result = executor->disarm();
        releaseSession(result.status == LiveStatus::Shadow ? "LIVE_TRAINER_COMPLETE_NATIVE_RESTORED"
                                                          : "LIVE_TRIAL_RELEASE_FAILED");
        return;
    }
    if (trainer && confirmedWatts) {
        const auto& sample = reader.state().battery;
        const auto measured = sample.usable(now);
        const auto result = response.observe(measured.value_or(NAN), sample.stamp, now);
        const auto& debug = response.diagnostic();
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
        if (result != TrainerResponse::Result::Waiting) {
            char line[120]{};
            snprintf(line, sizeof(line), "TRAINER %s expected_w=%d measured_w=%.1f",
                     result == TrainerResponse::Result::Matched ? "MATCH" : "MISMATCH",
                     *confirmedWatts, measured.value_or(NAN));
            recordLog(line);
            if (result == TrainerResponse::Result::Mismatch) {
                const auto stop = executor->disarm();
                releaseSession(stop.status == LiveStatus::Shadow ? "LIVE_TRAINER_MISMATCH_NATIVE_RESTORED"
                                                                 : "LIVE_TRIAL_RELEASE_FAILED");
                return;
            }
        }
    }
    if (!healthy) decision.finalWatts.reset();
    if (trainer && decision.finalWatts && decision.lower && decision.upper) {
        const int lower = std::max(*decision.lower, -trialPolicy.trainerDischargeW);
        const int upper = std::min(*decision.upper, trialPolicy.trainerChargeW);
        if (lower > upper) decision.finalWatts.reset();
        else decision.finalWatts = std::clamp(*decision.finalWatts, lower, upper);
    }
    const auto result = executor->tick(decision, now, now, 1000);
    if (result.status == LiveStatus::Holding) return;
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
