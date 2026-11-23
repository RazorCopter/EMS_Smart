// SPDX-License-Identifier: MIT
#pragma once

#include "shadow_core.h"
#include "device/live_actuator.h"

namespace heliograph::v51 {

enum class LiveStatus {
    Shadow, Armed, Applied, Holding, LeaseExpired, DecisionInvalid,
    OwnershipLost, WatchdogExpired, ReadbackFailed, ReleaseFailed, NativeDelegated
};

struct LiveResult {
    LiveStatus status = LiveStatus::Shadow;
    // Present only for a setpoint confirmed by this tick, in EMS watts.
    std::optional<int> confirmedWatts;
};

inline bool nativeDelegationBudgetAllows(uint32_t cooldownRemainingMs, unsigned writesToday,
                                         unsigned maximumWrites) {
    return cooldownRemainingMs == 0 && writesToday < maximumWrites &&
           maximumWrites - writesToday >= 2;
}

// One instance belongs to the task that owns the actuator. SHADOW is the boot
// state. Arming is volatile and always requires an explicit request.
class LiveExecutor {
public:
    explicit LiveExecutor(LiveActuator& actuator) : actuator_(actuator) {}
    LiveExecutor(const LiveExecutor&) = delete;
    LiveExecutor& operator=(const LiveExecutor&) = delete;

    bool arm(uint32_t now, std::optional<uint32_t> leaseMs, bool startNative = false) {
        if (armed_ || status_ == LiveStatus::ReleaseFailed ||
            (leaseMs && (!*leaseMs || *leaseMs > INT32_MAX)) || !actuator_.hasControl())
            return false;
        if (startNative && !actuator_.verifyNativeMode()) return false;
        armed_ = true;
        delegated_ = startNative;
        confirmedAt_ = now;
        leaseStarted_ = now;
        leaseMs_ = leaseMs;
        status_ = LiveStatus::Armed;
        return true;
    }

    LiveResult disarm() {
        if (!armed_) return {status_, {}};
        return stop(LiveStatus::Shadow);
    }

    bool nativeDelegated() const { return armed_ && delegated_; }

    LiveResult delegateNative(uint32_t now) {
        if (!armed_ || delegated_) return {status_, {}};
        if (!actuator_.enterNativeMode()) return stop(LiveStatus::ReleaseFailed, false);
        confirmed_.reset();
        delegated_ = true;
        confirmedAt_ = now;
        return {status_ = LiveStatus::NativeDelegated, {}};
    }

    LiveResult resumeExternal(uint32_t now) {
        if (!nativeDelegated()) return {status_, {}};
        if (!actuator_.verifyNativeMode()) return stop(LiveStatus::OwnershipLost, false);
        delegated_ = false;
        if (!actuator_.enterExternalBatteryControl()) return stop(LiveStatus::ReadbackFailed);
        confirmedAt_ = now;
        return {status_ = LiveStatus::Armed, {}};
    }

    LiveResult tick(const Decision& decision, uint32_t decisionAt, uint32_t now,
                    uint32_t maxDecisionAgeMs) {
        if (!armed_) return {status_, {}};
        if (!actuator_.hasControl()) return stop(LiveStatus::OwnershipLost, false);
        if (leaseMs_ && now - leaseStarted_ >= *leaseMs_) return stop(LiveStatus::LeaseExpired);
        if ((confirmed_ || delegated_) && now - confirmedAt_ >= kWatchdogMs)
            return stop(LiveStatus::WatchdogExpired);
        if (!maxDecisionAgeMs || maxDecisionAgeMs >= kHeartbeatMs ||
            now - decisionAt > maxDecisionAgeMs ||
            decision.strategy == Strategy::Safe || decision.strategy == Strategy::Native ||
            !decision.finalWatts || !decision.lower || !decision.upper ||
            *decision.finalWatts < *decision.lower || *decision.finalWatts > *decision.upper)
            return stop(LiveStatus::DecisionInvalid);

        if (delegated_) {
            if (now - confirmedAt_ >= kHeartbeatMs) {
                if (!actuator_.verifyNativeMode()) return stop(LiveStatus::OwnershipLost, false);
                confirmedAt_ = now;
            }
            return {LiveStatus::NativeDelegated, {}};
        }
        const int watts = *decision.finalWatts;
        if (confirmed_ == watts && now - confirmedAt_ < kHeartbeatMs)
            return {LiveStatus::Holding, {}};

        // A write with no matching readback might still have reached the device.
        const auto readback = actuator_.writeAndReadBack(watts);
        if (readback != watts) return stop(LiveStatus::ReadbackFailed);
        confirmed_ = readback;
        confirmedAt_ = now;
        status_ = LiveStatus::Applied;
        return {status_, readback};
    }

private:
    static constexpr uint32_t kHeartbeatMs = 30000;
    static constexpr uint32_t kWatchdogMs = 120000;

    LiveResult stop(LiveStatus status, bool release = true) {
        armed_ = false;
        confirmed_.reset();
        const bool restored = !release || (delegated_ ? actuator_.verifyNativeMode() : actuator_.release());
        delegated_ = false;
        status_ = restored ? status : LiveStatus::ReleaseFailed;
        return {status_, {}};
    }

    LiveActuator& actuator_;
    bool armed_ = false, delegated_ = false;
    uint32_t leaseStarted_ = 0, confirmedAt_ = 0;
    std::optional<uint32_t> leaseMs_;
    std::optional<int> confirmed_;
    LiveStatus status_ = LiveStatus::Shadow;
};

}
