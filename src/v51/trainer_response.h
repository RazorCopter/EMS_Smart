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
    void confirmed(int watts, uint32_t now) {
        if (!pending_) {
            pending_ = true;
            pendingSince_ = now;
            started_ = now;
        }
        if (watts != watts_ || !initialized_) {
            watts_ = watts;
            changed_ = now;
            started_ = now;
            initialized_ = true;
        }
    }

    enum class Result { Waiting, Matched, Mismatch };
    struct Diagnostic {
        const char* sample = "UNINITIALIZED";
        const char* deadline = "NONE";
        uint32_t commandAgeMs = 0, pendingAgeMs = 0, sampleAgeMs = 0;
        std::optional<uint32_t> physicalAgeMs;
        double toleranceW = 0;
    };
    const Diagnostic& diagnostic() const { return diagnostic_; }

    Result observe(double measured, uint32_t stamp, uint32_t now) {
        if (!initialized_) return Result::Waiting;
        // Use only a post-command sample and allow the inverter to settle.
        const double tolerance = std::max(double(settings_.trainerAbsW), std::abs(double(watts_)) * settings_.trainerPct / 100);
        const bool settled = stamp - changed_ >= uint32_t(settings_.trainerSettleMs) &&
                             stamp - changed_ <= now - changed_;
        const bool valid = std::isfinite(measured);
        const bool within = valid && std::abs(measured - watts_) <= tolerance;
        diagnostic_.sample = !valid ? "INVALID_SAMPLE" : !settled ? "NO_SETTLED_SAMPLE" :
                             within ? "WITHIN_TOLERANCE" : "OUTSIDE_TOLERANCE";
        diagnostic_.deadline = "NONE";
        diagnostic_.commandAgeMs = now - changed_;
        diagnostic_.sampleAgeMs = now - stamp;
        diagnostic_.toleranceW = tolerance;
        if (settled && within) {
            lastPhysical_ = stamp;
            diagnostic_.physicalAgeMs = now - stamp;
            diagnostic_.pendingAgeMs = 0;
            const bool wasPending = pending_;
            pending_ = false;
            return wasPending ? Result::Matched : Result::Waiting;
        }
        if (!pending_) {
            pending_ = true;
            pendingSince_ = now;
            started_ = now;
        }
        diagnostic_.pendingAgeMs = now - pendingSince_;
        if (lastPhysical_) diagnostic_.physicalAgeMs = now - *lastPhysical_;
        if (now - started_ >= uint32_t(settings_.trainerTimeoutMs))
            diagnostic_.deadline = "STABLE_COMMAND_TIMEOUT";
        else if (diagnostic_.pendingAgeMs >= uint32_t(settings_.trainerGapMs))
            diagnostic_.deadline = "VERIFICATION_GAP_TIMEOUT";
        else return Result::Waiting;
        return Result::Mismatch;
    }

private:
    Diagnostic diagnostic_;
    std::optional<uint32_t> lastPhysical_;
    Policy settings_;
    bool pending_ = false, initialized_ = false;
    int watts_ = 0;
    uint32_t pendingSince_ = 0, started_ = 0, changed_ = 0;
};
}
