// SPDX-License-Identifier: MIT

#include "afore_telemetry_engine.h"

#include <cmath>

#include "device/measurement.h"

namespace heliograph {
namespace {

/// Helper to convert uint16 to signed int16 for Modbus registers
inline int16_t toSigned(uint16_t raw) { return static_cast<int16_t>(raw); }

/// Helper to extract register at offset from data block
inline uint16_t reg(const std::vector<uint16_t> &data, size_t offset) {
  return offset < data.size() ? data[offset] : 0;
}

} // namespace

AforeTelemetryEngine::AforeTelemetryEngine(Transport &transport,
                                           uint8_t slaveId)
    : transport_(transport), slaveId_(slaveId) {
  // Initialize all canonical Afore measurements as Invalid
  samples_[measurement_id::kDcPowerTotal] = TelemetrySample{};
  samples_[measurement_id::kDcMppt1Power] = TelemetrySample{};
  samples_[measurement_id::kDcMppt2Power] = TelemetrySample{};
  samples_[measurement_id::kDcMppt1Voltage] = TelemetrySample{};
  samples_[measurement_id::kDcMppt1Current] = TelemetrySample{};
  samples_[measurement_id::kDcMppt2Voltage] = TelemetrySample{};
  samples_[measurement_id::kDcMppt2Current] = TelemetrySample{};
  samples_[measurement_id::kBatterySoc] = TelemetrySample{};
  samples_[measurement_id::kBatteryVoltage] = TelemetrySample{};
  samples_[measurement_id::kBatteryCurrent] = TelemetrySample{};
  samples_[measurement_id::kBatteryPower] = TelemetrySample{};
  samples_[measurement_id::kGridPower] = TelemetrySample{};
  samples_[measurement_id::kAcPowerTotal] = TelemetrySample{};
  samples_[measurement_id::kAcL1Voltage] = TelemetrySample{};
  samples_[measurement_id::kAcFrequency] = TelemetrySample{};
  samples_[measurement_id::kTemperature] = TelemetrySample{};
}

void AforeTelemetryEngine::configureFastBlock(uint16_t start, uint16_t count,
                                              uint32_t intervalMs) {
  blocks_["fast"] = PollingBlock{start, count, intervalMs, 0, true};
}

void AforeTelemetryEngine::configureMediumBlock(uint16_t start, uint16_t count,
                                                uint32_t intervalMs) {
  blocks_["medium"] = PollingBlock{start, count, intervalMs, 0, true};
}

void AforeTelemetryEngine::configureSlowBlock(uint16_t start, uint16_t count,
                                              uint32_t intervalMs) {
  blocks_["slow"] = PollingBlock{start, count, intervalMs, 0, true};
}

size_t AforeTelemetryEngine::poll(uint64_t nowMs) {
  size_t polled = 0;

  // Milestone M9 Safety Invariant: Hardware polling is strictly forbidden until
  // Gate G2. Polling against physical hardware (RS485 or POSIX serial)
  // unconditionally refuses execution unless explicit Gate G2 authorization has
  // been granted.
  if (transport_.type() != TransportType::Mock && !hardwarePollingAuthorized_) {
    return 0;
  }

  // Check if circuit breaker is open (Milestone M9)
  if (circuitOpen_) {
    if (nowMs < circuitOpenUntilMs_) {
      // Still in cooldown period, avoid polluting the bus
      return 0;
    }
    // Cooldown elapsed: try probing again
    circuitOpen_ = false;
  }

  // Poll all due blocks respecting backoff and minInterFrameGapMs_
  for (auto &[name, block] : blocks_) {
    if (block.isDue(nowMs, backoffMultiplier_)) {
      if (lastTransactionMs_ > 0 &&
          (nowMs - lastTransactionMs_ < minInterFrameGapMs_)) {
        // Enforce conservative inter-frame gap: defer to next cycle
        continue;
      }
      if (pollBlock(name, block, nowMs)) {
        polled++;
      }
      lastTransactionMs_ = nowMs;
    }
  }

  // Update age and quality for all samples
  for (auto &[id, sample] : samples_) {
    sample.ageMs = computeAge(nowMs, sample.timestampMs);

    // Recompute quality based on current age
    bool stale = sample.ageMs > maxAgeMs_;
    sample.quality = computeQuality(
        sample.quality != Quality::Invalid, // supported if ever updated
        sample.quality != Quality::Invalid, // valid if not invalid
        stale, sample.verified);
  }

  return polled;
}

bool AforeTelemetryEngine::pollBlock(const std::string &blockName,
                                     PollingBlock &block, uint64_t nowMs) {
  (void)blockName; // Reserved for future logging/diagnostics
  stats_.totalPolls++;

  // Allocate buffer for Modbus read
  std::vector<uint16_t> data(block.count);
  const auto outcome = modbus::readRegisters(
      transport_, slaveId_, modbus::kReadHoldingRegisters, block.startAddress,
      block.count, data.data(), block.count);

  if (outcome.status == modbus::ReadStatus::Ok) {
    stats_.successfulPolls++;
    stats_.lastSuccessMs = nowMs;
    block.lastPollMs = nowMs;
    consecutiveErrors_ = 0;
    backoffMultiplier_ = 1;

    // Decode based on block address range
    if (block.startAddress == 512) { // 0x0200: Power/Grid block
      decodePowerBlock(data, nowMs);
    } else if (block.startAddress == 552) { // 0x0228: PV block
      decodePVBlock(data, nowMs);
    } else if (block.startAddress == 2000) { // 0x07D0: Battery block
      decodeBatteryBlock(data, nowMs);
    }
    return true;

  } else {
    consecutiveErrors_++;
    // Backoff logic: at 3 consecutive errors increase cadence; at 5 open
    // circuit
    if (consecutiveErrors_ >= 3 && backoffMultiplier_ < 8) {
      backoffMultiplier_ *= 2;
    }
    if (consecutiveErrors_ >= 5) {
      circuitOpen_ = true;
      circuitOpenUntilMs_ = nowMs + 30000; // 30s silence window
    }

    // Classify error type
    switch (outcome.status) {
    case modbus::ReadStatus::Crc:
      stats_.crcErrors++;
      break;
    case modbus::ReadStatus::Timeout:
      stats_.timeouts++;
      break;
    case modbus::ReadStatus::Exception:
      stats_.exceptions++;
      break;
    default:
      break;
    }
    return false;
  }
}

void AforeTelemetryEngine::decodePowerBlock(const std::vector<uint16_t> &data,
                                            uint64_t nowMs) {
  // Block 0x0200-0x0212 (19 registers)
  // Offsets relative to start address 512

  // 0x0203 (offset 3): Grid voltage, scale 0.1, UNVERIFIED
  auto &gridV = samples_[measurement_id::kAcL1Voltage];
  gridV.value = reg(data, 3) * 0.1;
  gridV.timestampMs = nowMs;
  gridV.verified = false; // UNVERIFIED per profile
  gridV.derived = false;
  updateSampleQuality(gridV, nowMs, false);

  // 0x0205 (offset 5): Grid frequency, scale 0.01, UNVERIFIED
  auto &freq = samples_[measurement_id::kAcFrequency];
  freq.value = reg(data, 5) * 0.01;
  freq.timestampMs = nowMs;
  freq.verified = false;
  freq.derived = false;
  updateSampleQuality(freq, nowMs, false);

  // 0x020B (offset 11): AC output power, scale 1, signed, VERIFIED
  auto &acPower = samples_[measurement_id::kAcPowerTotal];
  acPower.value = toSigned(reg(data, 11)) * 1.0;
  acPower.timestampMs = nowMs;
  acPower.verified = true; // VERIFIED per profile
  acPower.derived = false;
  updateSampleQuality(acPower, nowMs, true);

  // 0x020C (offset 12): Inverter temperature, scale 0.1, signed, UNVERIFIED
  auto &temp = samples_[measurement_id::kTemperature];
  temp.value = toSigned(reg(data, 12)) * 0.1;
  temp.timestampMs = nowMs;
  temp.verified = false;
  temp.derived = false;
  updateSampleQuality(temp, nowMs, false);

  // 0x0212 (offset 18): Grid power, scale 1, signed, VERIFIED
  // Sign convention: positive = import, negative = export (TO VERIFY in Phase
  // 0)
  auto &gridP = samples_[measurement_id::kGridPower];
  gridP.value = toSigned(reg(data, 18)) * 1.0;
  gridP.timestampMs = nowMs;
  gridP.verified = true; // VERIFIED but sign TBD
  gridP.derived = false;
  updateSampleQuality(gridP, nowMs, true);
}

void AforeTelemetryEngine::decodeBatteryBlock(const std::vector<uint16_t> &data,
                                              uint64_t nowMs) {
  // Block 0x07D0-0x07D8 (9 registers)
  // Offsets relative to start address 2000

  // 0x07D2 (offset 2): Battery SOC, scale 1, VERIFIED
  auto &soc = samples_[measurement_id::kBatterySoc];
  soc.value = reg(data, 2) * 1.0;
  soc.timestampMs = nowMs;
  soc.verified = true;
  soc.derived = false;
  updateSampleQuality(soc, nowMs, true);

  // 0x07D4 (offset 4): Battery voltage, scale 0.1, VERIFIED
  auto &voltage = samples_[measurement_id::kBatteryVoltage];
  voltage.value = reg(data, 4) * 0.1;
  voltage.timestampMs = nowMs;
  voltage.verified = true;
  voltage.derived = false;
  updateSampleQuality(voltage, nowMs, true);

  // 0x07D5 (offset 5): Battery current, scale 0.01, signed, VERIFIED
  auto &current = samples_[measurement_id::kBatteryCurrent];
  current.value = toSigned(reg(data, 5)) * 0.01;
  current.timestampMs = nowMs;
  current.verified = true;
  current.derived = false;
  updateSampleQuality(current, nowMs, true);

  // 0x07D8 (offset 8): Battery power, scale -1 (SIGN FLIP), signed, VERIFIED
  // Afore convention: negative during charge → flip to canonical positive =
  // charging
  auto &power = samples_[measurement_id::kBatteryPower];
  power.value = toSigned(reg(data, 8)) * (-1.0); // Critical sign flip
  power.timestampMs = nowMs;
  power.verified = true;
  power.derived = false;
  updateSampleQuality(power, nowMs, true);
}

void AforeTelemetryEngine::decodePVBlock(const std::vector<uint16_t> &data,
                                         uint64_t nowMs) {
  // Block 0x0228-0x0230 (9 registers)
  // Offsets relative to start address 552

  // 0x0228 (offset 0): PV1 voltage, scale 0.1, UNVERIFIED
  auto &pv1V = samples_[measurement_id::kDcMppt1Voltage];
  pv1V.value = reg(data, 0) * 0.1;
  pv1V.timestampMs = nowMs;
  pv1V.verified = false;
  pv1V.derived = false;
  updateSampleQuality(pv1V, nowMs, false);

  // 0x0229 (offset 1): PV1 current, scale 0.1, UNVERIFIED
  auto &pv1I = samples_[measurement_id::kDcMppt1Current];
  pv1I.value = reg(data, 1) * 0.1;
  pv1I.timestampMs = nowMs;
  pv1I.verified = false;
  pv1I.derived = false;
  updateSampleQuality(pv1I, nowMs, false);

  // 0x022A (offset 2): Total PV power, scale 1, VERIFIED
  auto &pvTotal = samples_[measurement_id::kDcPowerTotal];
  pvTotal.value = reg(data, 2) * 1.0;
  pvTotal.timestampMs = nowMs;
  pvTotal.verified = true;
  pvTotal.derived = false;
  updateSampleQuality(pvTotal, nowMs, true);

  // 0x022D (offset 5): PV1 power, scale 1, VERIFIED
  auto &pv1P = samples_[measurement_id::kDcMppt1Power];
  pv1P.value = reg(data, 5) * 1.0;
  pv1P.timestampMs = nowMs;
  pv1P.verified = true;
  pv1P.derived = false;
  updateSampleQuality(pv1P, nowMs, true);

  // 0x022E (offset 6): PV2 voltage, scale 0.1, UNVERIFIED
  auto &pv2V = samples_[measurement_id::kDcMppt2Voltage];
  pv2V.value = reg(data, 6) * 0.1;
  pv2V.timestampMs = nowMs;
  pv2V.verified = false;
  pv2V.derived = false;
  updateSampleQuality(pv2V, nowMs, false);

  // 0x022F (offset 7): PV2 current, scale 0.1, UNVERIFIED
  auto &pv2I = samples_[measurement_id::kDcMppt2Current];
  pv2I.value = reg(data, 7) * 0.1;
  pv2I.timestampMs = nowMs;
  pv2I.verified = false;
  pv2I.derived = false;
  updateSampleQuality(pv2I, nowMs, false);

  // 0x0230 (offset 8): PV2 power, scale 1, VERIFIED
  auto &pv2P = samples_[measurement_id::kDcMppt2Power];
  pv2P.value = reg(data, 8) * 1.0;
  pv2P.timestampMs = nowMs;
  pv2P.verified = true;
  pv2P.derived = false;
  updateSampleQuality(pv2P, nowMs, true);
}

void AforeTelemetryEngine::updateSampleQuality(TelemetrySample &sample,
                                               uint64_t nowMs, bool verified) {
  sample.timestampMs = nowMs; // Record fresh reading timestamp
  sample.ageMs = 0;           // Fresh reading has zero age
  sample.verified = verified;

  // Fresh reading from successful poll: Good if verified, Degraded if
  // unverified
  sample.quality = verified ? Quality::Good : Quality::Degraded;
}

void AforeTelemetryEngine::updateDeviceState(DeviceState &state,
                                             uint64_t nowMs) {
  // Only update DeviceState with actionable samples (Good/Degraded)
  // Invalid/Stale samples should not overwrite state

  for (const auto &[id, sample] : samples_) {
    if (sample.actionable()) {
      // Find or declare measurement in state
      auto *m = state.measurements.find(id.c_str());
      if (m == nullptr) {
        // Measurement not yet declared, skip (would need type/unit info)
        continue;
      }

      // Update with fresh value
      state.measurements.set(id.c_str(), sample.value, sample.timestampMs);
    } else if (sample.quality == Quality::Stale) {
      // Mark as stale in DeviceState but keep last value
      auto *m = state.measurements.find(id.c_str());
      if (m != nullptr && m->valid) {
        // Keep value, just mark stale
        state.measurements.updateStaleness(nowMs, maxAgeMs_);
      }
    }
  }
}

} // namespace heliograph
