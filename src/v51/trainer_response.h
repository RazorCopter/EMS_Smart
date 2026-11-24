// SPDX-License-Identifier: MIT
#pragma once
#include "shadow_core.h"
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace heliograph::v51 {

class TrainerResponse {
public:
    TrainerResponse() = default;
    explicit TrainerResponse(const Policy& p) : settings_(p) {}
    void updatePolicy(const Policy& p) { settings_ = p; }
    void confirmed(int watts, uint32_t now) {
        if (!pending_) {
            pending_ = true;
            pendingSince_ = now;
            started_ = now;
        }
        if (watts != watts_ || !initialized_) {
            watts_ = watts;
            severeStamp_.reset();
            changed_ = now;
            started_ = now;
            initialized_ = true;
        }
    }

    enum class Result { Waiting, Matched, Mismatch, Warning, Recover, Recovered };
    struct Diagnostic {
        const char* sample = "UNINITIALIZED";
        const char* deadline = "NONE";
        uint32_t commandAgeMs = 0, pendingAgeMs = 0, sampleAgeMs = 0;
        std::optional<uint32_t> physicalAgeMs;
        double toleranceW = 0;
    };
    const Diagnostic& diagnostic() const { return diagnostic_; }

    const char* verificationState() const {
        return recoveryTarget_ ? "RECOVERY" : warning_ ? "WARNING" : "NORMAL";
    }

    int verificationSetpoint(int requested, int lower, int upper, uint32_t now) {
        // Leave time for the inverter to settle and for a fresh poll and EMS tick.
        constexpr uint32_t observationMarginMs = 3000;
        const auto gapMs = uint32_t(settings_.trainerGapMs);
        const auto reserveMs = uint32_t(settings_.trainerSettleMs) + observationMarginMs;
        const auto holdAfterMs = gapMs - std::min(gapMs, reserveMs);
        if (recoveryTarget_) return std::clamp(*recoveryTarget_, lower, upper);
        if (warning_ && watts_ >= lower && watts_ <= upper) return watts_;
        if (resuming_) {
            const auto elapsed = std::min(now - changed_, uint32_t(1000));
            const int step = int(uint32_t(settings_.ramp) * elapsed / 1000);
            const int next = std::clamp(requested, watts_ - step, watts_ + step);
            resuming_ = next != requested;
            return std::clamp(next, lower, upper);
        }
        if (!initialized_ || !pending_ || now - pendingSince_ < holdAfterMs)
            return requested;
        if (watts_ >= lower && watts_ <= upper) return watts_;
        if (watts_ > upper && requested > 0 && upper > 0) {
            // Leave headroom for another drop in available PV during settling.
            const int marginW = std::max(50, upper / 20);
            return std::clamp(std::min(requested, upper - marginW), std::max(0, lower), upper);
        }
        return requested;
    }

    Result observe(double measured, uint32_t stamp, uint32_t now, bool physicalChargeLimited = false) {
        if (!initialized_) return Result::Waiting;
        // Use only a post-command sample and allow the inverter to settle.
        const double tolerance = std::max(double(settings_.trainerAbsW), std::abs(double(watts_)) * settings_.trainerPct / 100);
        const bool settled = stamp - changed_ >= uint32_t(settings_.trainerSettleMs) &&
                             stamp - changed_ <= now - changed_;
        const bool valid = std::isfinite(measured);
        const bool bmsLimitedOk = physicalChargeLimited && watts_ > 0 && valid &&
                                  measured >= -double(settings_.trainerAbsW) &&
                                  measured <= double(watts_) + tolerance;
        const bool within = valid && (std::abs(measured - watts_) <= tolerance || bmsLimitedOk);
        diagnostic_.sample = !valid ? "INVALID_SAMPLE" : !settled ? "NO_SETTLED_SAMPLE" :
                             within ? "WITHIN_TOLERANCE" : "OUTSIDE_TOLERANCE";
        diagnostic_.deadline = "NONE";
        diagnostic_.commandAgeMs = now - changed_;
        diagnostic_.sampleAgeMs = now - stamp;
        diagnostic_.toleranceW = tolerance;
        if (recoveryTarget_ && now - recoverySince_ >= uint32_t(settings_.trainerRecoveryMs)) {
            diagnostic_.deadline = "RECOVERY_TIMEOUT";
            return Result::Mismatch;
        }
        if (settled && within && (!recoveryTarget_ || watts_ == *recoveryTarget_)) {
            lastPhysical_ = stamp;
            diagnostic_.physicalAgeMs = now - stamp;
            diagnostic_.pendingAgeMs = 0;
            const bool wasPending = pending_;
            pending_ = false;
            warning_ = false;
            severeStamp_.reset();
            if (recoveryTarget_) {
                recoveryTarget_.reset();
                resuming_ = true;
                return Result::Recovered;
            }
            return wasPending ? Result::Matched : Result::Waiting;
        }
        if (!pending_) {
            pending_ = true;
            pendingSince_ = now;
            started_ = now;
        }
        diagnostic_.pendingAgeMs = now - pendingSince_;
        if (lastPhysical_) diagnostic_.physicalAgeMs = now - *lastPhysical_;
        const bool opposite = (watts_ > 0 && measured < -settings_.trainerAbsW) ||
                              (watts_ < 0 && measured > settings_.trainerAbsW);
        const auto critical = std::max(double(settings_.trainerCriticalW),
                                       std::abs(double(watts_)) * settings_.trainerCriticalPct / 100);
        const bool severe = settled && valid && !within &&
                            (opposite || std::abs(measured - watts_) > critical);
        if (severe && severeStamp_ && stamp != *severeStamp_) {
            diagnostic_.deadline = "SEVERE_RESPONSE";
            return Result::Mismatch;
        }
        if (severe) severeStamp_ = stamp;
        else severeStamp_.reset();

        if (recoveryTarget_) return Result::Waiting;
        if (now - started_ >= uint32_t(settings_.trainerTimeoutMs))
            diagnostic_.deadline = "STABLE_COMMAND_TIMEOUT";
        else if (diagnostic_.pendingAgeMs >= uint32_t(settings_.trainerGapMs))
            diagnostic_.deadline = "VERIFICATION_GAP_TIMEOUT";
        else {
            if (valid && settled && !within && !warning_) {
                warning_ = true;
                return Result::Warning;
            }
            return Result::Waiting;
        }
        const int reduced = watts_ * settings_.trainerRecoveryPct / 100;
        // A near-zero target could make a stalled battery appear to recover.
        if (!valid || !settled || severe || std::abs(reduced) <= settings_.trainerAbsW)
            return Result::Mismatch;
        recoveryTarget_ = reduced;
        recoverySince_ = now;
        return Result::Recover;
    }

private:
    Diagnostic diagnostic_;
    std::optional<uint32_t> lastPhysical_, severeStamp_;
    std::optional<int> recoveryTarget_;
    uint32_t recoverySince_ = 0;
    bool warning_ = false, resuming_ = false;
    Policy settings_;
    bool pending_ = false, initialized_ = false;
    int watts_ = 0;
    uint32_t pendingSince_ = 0, started_ = 0, changed_ = 0;
};
}
