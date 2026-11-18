// SPDX-License-Identifier: MIT
#pragma once
#include "energy_state.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace heliograph::v51 {
enum class Strategy { SelfUse, ChargeFirst, Native, Safe };

struct Decision {
    Strategy strategy;
    std::optional<int> candidate;
    std::optional<int> finalWatts;
    const char* reason;
    static constexpr bool executed = false;
};

inline const char* strategyName(Strategy strategy) {
    switch (strategy) {
        case Strategy::SelfUse: return "SELF_USE";
        case Strategy::ChargeFirst: return "CHARGE_FIRST";
        case Strategy::Native: return "NATIVE_INVERTER";
        case Strategy::Safe: return "SAFE_MODE";
    }
    return "SAFE_MODE";
}

struct ShadowExecutor {
    static Decision simulate(Decision decision) { return decision; }
};

inline Decision arbitrate(const EnergyState& state, Strategy strategy, uint32_t now,
                          std::optional<int> previous, uint32_t elapsedMs) {
    const auto soc = state.soc.usable(now);
    const auto grid = state.grid.usable(now);
    const auto battery = state.battery.usable(now);
    if (strategy == Strategy::Safe || state.readHalted || !state.goodReads || !soc || !grid ||
        !battery || std::abs(int32_t(state.grid.stamp - state.battery.stamp)) > 2000) {
        return ShadowExecutor::simulate({Strategy::Safe, {}, {}, "INPUT_UNAVAILABLE_OR_STALE"});
    }
    if (strategy == Strategy::Native) {
        return ShadowExecutor::simulate({strategy, {}, {}, "NATIVE_OBSERVATION"});
    }

    constexpr int capW = 1000;
    const auto balance = static_cast<int>(std::lround(*battery - *grid));
    const int candidate = strategy == Strategy::ChargeFirst ? std::max(0, balance) : balance;
    const int minimum = *soc <= 20 || strategy == Strategy::ChargeFirst ? 0 : -capW;
    const int maximum = *soc >= 95 ? 0 : std::clamp(balance, 0, capW);
    int result = std::clamp(candidate, minimum, maximum);
    if (previous) {
        const int step = int(250 * std::min(elapsedMs, uint32_t(4000)) / 1000);
        result = std::abs(result - *previous) < 20 ? *previous :
                 std::clamp(result, *previous - step, *previous + step);
    }
    result = std::clamp(result, minimum, maximum);
    return ShadowExecutor::simulate({strategy, candidate, result,
                                     result == candidate ? "BALANCE" : "SAFETY_OR_SMOOTHING"});
}
}
