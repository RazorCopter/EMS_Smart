// SPDX-License-Identifier: MIT
//
// Telemetry quality model for Phase 0 Shadow Mode (CP3).
//
// Extends the existing Measurement::valid/stale flags with explicit quality states and
// derived/verified metadata, enabling shadow controller to make informed decisions about
// data freshness and trustworthiness.

#pragma once

#include <cstdint>

namespace heliograph {

/// Quality state for a telemetry sample.
///
/// Hierarchical quality model matching Phase 0 requirements:
///   Good      — Fresh reading from verified register, all validation passed
///   Degraded  — Reading present but with warnings (CRC retry, near timeout, unverified register)
///   Stale     — Reading too old to trust (exceeded max age threshold)
///   Invalid   — No usable reading (Modbus error, out-of-range, device offline)
///
/// Quality degrades monotonically without new successful reads: Good → Degraded → Stale → Invalid.
/// Controllers should use only Good/Degraded for actuation; Stale/Invalid trigger fail-safe.
enum class Quality : uint8_t {
    Good     = 0,  ///< Fresh, verified, within tolerance
    Degraded = 1,  ///< Usable but with caveats (retry, unverified register, near-stale)
    Stale    = 2,  ///< Exceeded staleness threshold
    Invalid  = 3,  ///< No usable data (error, offline, never polled)
};

const char* qualityToString(Quality q);

/// Extended telemetry metadata for shadow mode decision-making.
///
/// Augments Measurement with quality state, verification status, and age tracking.
/// Computed per-sample during polling, published alongside value in MQTT payloads.
struct TelemetrySample {
    double value = 0.0;
    Quality quality = Quality::Invalid;
    uint64_t timestampMs = 0;     ///< Monotonic timestamp of last update
    uint64_t ageMs = 0;           ///< Time since last valid update (computed on query)
    bool verified = false;        ///< From VERIFIED register per CP2 profile annotation
    bool derived = false;         ///< Computed value (e.g. power = V × I)
    
    /// Whether this sample is safe to use for control decisions.
    /// Shadow controller should act only on actionable() samples.
    bool actionable() const { 
        return quality == Quality::Good || quality == Quality::Degraded; 
    }
    
    /// Whether this sample should be published as a number.
    /// Invalid/offline samples publish as null to Home Assistant.
    bool publishable() const {
        return quality != Quality::Invalid;
    }
};

/// Computes Quality from Measurement flags and age.
///
/// Rules:
///   - !supported → Invalid (channel does not exist)
///   - !valid → Invalid (Modbus error, never read, out of range)
///   - stale → Stale (exceeded max age)
///   - valid + fresh + verified → Good
///   - valid + fresh + !verified → Degraded (UNVERIFIED register per CP2)
///
/// Additional degradation triggers (future):
///   - CRC retry count > 0
///   - Response time > P95 latency
///   - Value at range boundary (e.g. SOC=0% or 100%)
Quality computeQuality(bool supported, bool valid, bool stale, bool verified);

/// Computes age in milliseconds between nowMs and timestampMs.
/// Returns 0 if timestampMs is in the future (clock skew guard).
uint64_t computeAge(uint64_t nowMs, uint64_t timestampMs);

}  // namespace heliograph
