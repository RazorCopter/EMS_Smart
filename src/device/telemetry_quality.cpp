// SPDX-License-Identifier: MIT

#include "telemetry_quality.h"

namespace heliograph {

const char* qualityToString(Quality q) {
    switch (q) {
        case Quality::Good:     return "good";
        case Quality::Degraded: return "degraded";
        case Quality::Stale:    return "stale";
        case Quality::Invalid:  return "invalid";
    }
    return "unknown";
}

Quality computeQuality(bool supported, bool valid, bool stale, bool verified) {
    // Priority order: unsupported/invalid first, then stale, then verified status
    if (!supported || !valid) {
        return Quality::Invalid;
    }
    
    if (stale) {
        return Quality::Stale;
    }
    
    // Fresh and valid: quality depends on verification status
    return verified ? Quality::Good : Quality::Degraded;
}

uint64_t computeAge(uint64_t nowMs, uint64_t timestampMs) {
    // Guard against clock skew or uninitialized timestamps
    if (timestampMs == 0 || nowMs < timestampMs) {
        return 0;
    }
    return nowMs - timestampMs;
}

}  // namespace heliograph
