// SPDX-License-Identifier: MIT
#pragma once
#include <optional>

namespace heliograph::v51 {
#ifdef V51_LIVE_TRIAL
#undef V51_BUILD_ID
#define V51_BUILD_ID "v5.1-live-trial-18"
bool liveTrialActive();
bool liveTrialExecuted();
std::optional<int> liveTrialAppliedWatts();
#else
inline constexpr bool liveTrialActive() { return false; }
inline constexpr bool liveTrialExecuted() { return false; }
inline std::optional<int> liveTrialAppliedWatts() { return std::nullopt; }
#endif
}

#ifdef V51_LIVE_TRIAL
#include <ArduinoJson.h>
#include "shadow_core.h"

namespace heliograph::v51 {
class AforeReader;

// Requests are queued from HTTP; all bus I/O stays on the reader's task.
bool requestLiveTrial(const char* action);
bool configureLiveTrainer(bool enabled, unsigned sampleSeconds);
void appendLiveTrialStatus(JsonObject out);
void serviceLiveTrial(AforeReader& reader, Decision decision, uint32_t now, bool healthy);
}
#endif
