// SPDX-License-Identifier: MIT
//
// Afore AF5K-SL Telemetry Engine with Quality Tracking (CP3).
//
// Orchestrates Modbus polling with differentiated cadences:
//   - Fast block (1s): power, current, voltage — control-critical measurements
//   - Medium block (5s): SOC, grid state — slower-changing state
//   - Slow block (30s): temperature, energy counters — diagnostic/informational
//
// Computes quality metadata per sample (Good/Degraded/Stale/Invalid) based on
// age, register verification status, and Modbus communication health.
//
// Phase 0 Shadow Mode: read-only operation, no External EMS actuation.

#pragma once

#include <cstdint>
#include <map>
#include <string>

#include "device/device_state.h"
#include "device/telemetry_quality.h"
#include "protocols/modbus/modbus_client.h"
#include "transport/transport.h"

namespace heliograph {

/// Polling configuration for one register block.
struct PollingBlock {
  uint16_t startAddress;
  uint16_t count;
  uint32_t intervalMs;     ///< Polling interval in milliseconds
  uint64_t lastPollMs = 0; ///< Timestamp of last successful poll
  bool enabled = true;

  /// Whether this block is due for polling now.
  bool isDue(uint64_t nowMs, uint32_t backoffMultiplier = 1) const {
    uint32_t effectiveInterval = intervalMs * backoffMultiplier;
    return enabled && (nowMs - lastPollMs >= effectiveInterval);
  }
};

/// Telemetry engine for Afore AF5K-SL with quality tracking and differentiated
/// polling.
///
/// Usage:
///   AforeTelemetryEngine engine(transport, slaveId);
///   engine.configureFastBlock(512, 19, 1000);   // 0x0200-0x0212, 1s
///   engine.configureMediumBlock(2000, 9, 5000); // 0x07D0-0x07D8, 5s
///   engine.configureSlowBlock(552, 9, 10000);   // 0x0228-0x0230, 10s
///
///   loop:
///     engine.poll(nowMs);
///     const auto& samples = engine.getTelemetrySamples();
///     // Use samples for MQTT, shadow controller, etc.
class AforeTelemetryEngine {
public:
  /// Creates engine with Modbus transport and slave address.
  /// Transport should be wrapped in ReadOnlyGuard for Phase 0 safety.
  AforeTelemetryEngine(Transport &transport, uint8_t slaveId);

  /// Configure polling blocks with differentiated cadences.
  /// Blocks default to disabled; call these to enable.
  void configureFastBlock(uint16_t start, uint16_t count, uint32_t intervalMs);
  void configureMediumBlock(uint16_t start, uint16_t count,
                            uint32_t intervalMs);
  void configureSlowBlock(uint16_t start, uint16_t count, uint32_t intervalMs);

  /// Sets staleness threshold: samples older than this are marked Stale.
  /// Default: 15 seconds (3× fast block interval for tolerance).
  void setMaxAge(uint32_t maxAgeMs) { maxAgeMs_ = maxAgeMs; }

  /// Real hardware polling gate enforcement (Milestone M9 safety invariant).
  /// Real polling against physical inverters is STRICTLY FORBIDDEN until Gate
  /// G2 approval. When bound to physical hardware (e.g. RS485 or POSIX serial),
  /// poll() unconditionally refuses execution unless hardwarePollingAuthorized_
  /// has been explicitly set to true. Simulated accelerated cadence is allowed
  /// ONLY on Mock/Fake transports during tests.
  void authorizeHardwarePolling(bool authorized) {
    hardwarePollingAuthorized_ = authorized;
  }
  bool isHardwarePollingAuthorized() const {
    return hardwarePollingAuthorized_;
  }

  /// Polls all due blocks and updates telemetry samples with quality metadata.
  /// Non-blocking: returns immediately after polling due blocks.
  /// Returns number of blocks successfully polled this cycle.
  size_t poll(uint64_t nowMs);

  /// Backoff and error handling accessors (Milestone M9)
  uint32_t getConsecutiveErrors() const { return consecutiveErrors_; }
  bool isCircuitOpen() const { return circuitOpen_; }
  uint32_t getBackoffMultiplier() const { return backoffMultiplier_; }
  void setMinInterFrameGapMs(uint32_t gapMs) { minInterFrameGapMs_ = gapMs; }

  /// Retrieves current telemetry samples with quality metadata.
  /// Map keyed by canonical measurement ID (e.g. "battery.soc").
  const std::map<std::string, TelemetrySample> &getTelemetrySamples() const {
    return samples_;
  }

  /// Updates DeviceState measurements from internal telemetry samples.
  /// Applies quality-based filtering: only Good/Degraded samples update state.
  void updateDeviceState(DeviceState &state, uint64_t nowMs);

  /// Communication statistics for diagnostics.
  struct Stats {
    uint32_t totalPolls = 0;
    uint32_t successfulPolls = 0;
    uint32_t crcErrors = 0;
    uint32_t timeouts = 0;
    uint32_t exceptions = 0;
    uint64_t lastSuccessMs = 0;

    double successRate() const {
      return totalPolls > 0 ? (successfulPolls * 100.0 / totalPolls) : 0.0;
    }
  };

  const Stats &getStats() const { return stats_; }
  void resetStats() { stats_ = Stats{}; }

private:
  bool pollBlock(const std::string &blockName, PollingBlock &block,
                 uint64_t nowMs);
  void updateSampleQuality(TelemetrySample &sample, uint64_t nowMs,
                           bool verified);
  void decodePowerBlock(const std::vector<uint16_t> &data, uint64_t nowMs);
  void decodeBatteryBlock(const std::vector<uint16_t> &data, uint64_t nowMs);
  void decodePVBlock(const std::vector<uint16_t> &data, uint64_t nowMs);

  Transport &transport_;
  uint8_t slaveId_;
  uint32_t maxAgeMs_ = 15000; ///< Default 15s staleness threshold

  bool hardwarePollingAuthorized_ =
      false; ///< Gate G2 authorization required for real hardware
  uint32_t consecutiveErrors_ = 0;
  uint32_t backoffMultiplier_ = 1;
  bool circuitOpen_ = false;
  uint64_t circuitOpenUntilMs_ = 0;
  uint64_t lastTransactionMs_ = 0;
  uint32_t minInterFrameGapMs_ = 200;

  std::map<std::string, PollingBlock> blocks_;
  std::map<std::string, TelemetrySample> samples_;
  Stats stats_;
};

} // namespace heliograph
