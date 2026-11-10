// SPDX-License-Identifier: MIT
//
// Read-only transport guard: Phase 0 Shadow safety enforcement.
//
// Wraps any Transport implementation and enforces a strict read-only policy by intercepting
// write() calls and rejecting Modbus function codes that perform writes. This is the
// multi-level defense for Phase 0 Shadow mode: even if application logic attempts a write,
// this guard prevents the packet from reaching the wire.
//
// ENFORCEMENT LEVELS:
//   1. Modbus function code detection (0x05, 0x06, 0x0F, 0x10, etc.)
//   2. Runtime flag check (forceReadOnly, default true)
//   3. Detailed logging of all rejected writes
//
// The guard is transparent for reads: configure(), read(), lock/unlock all pass through.
// Only write() is intercepted and gated.

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>

#include "transport.h"

namespace heliograph {

/// Transport wrapper that enforces read-only operation by blocking Modbus write function codes.
///
/// Phase 0 Shadow requirement: the bridge must observe inverter state without any possibility
/// of modifying it. This guard provides defense-in-depth: even if a bug or misconfiguration
/// causes a write command to be constructed, it cannot reach the physical device.
///
/// RECOGNIZED WRITE FUNCTION CODES (Modbus standard):
///   0x05  Write Single Coil
///   0x06  Write Single Register
///   0x0F  Write Multiple Coils
///   0x10  Write Multiple Registers
///   0x16  Mask Write Register
///   0x17  Read/Write Multiple Registers
///
/// Any packet starting with unit_id + write_function_code is rejected before transmission.
/// Read function codes (0x01-0x04) pass through unchanged.
class ReadOnlyGuard : public Transport {
public:
    /// Wraps an existing transport. The guard takes ownership via unique_ptr; the underlying
    /// transport is destroyed when the guard is destroyed.
    explicit ReadOnlyGuard(std::unique_ptr<Transport> underlying);
    ~ReadOnlyGuard() override = default;

    TransportType type() const override { return underlying_->type(); }
    bool configure(const SerialProfile& profile) override { return underlying_->configure(profile); }
    void flushInput() override { underlying_->flushInput(); }

    /// Intercepts writes. If forceReadOnly is true (default), scans the payload for Modbus
    /// write function codes and rejects them. Returns 0 and logs an error if blocked.
    size_t write(const uint8_t* data, size_t len) override;

    size_t   read(uint8_t* buf, size_t len, uint32_t timeoutMs) override {
        return underlying_->read(buf, len, timeoutMs);
    }
    uint64_t nowMs() const override { return underlying_->nowMs(); }
    bool     lock(uint32_t timeoutMs) override { return underlying_->lock(timeoutMs); }
    void     unlock() override { underlying_->unlock(); }

    /// Controls write blocking. Default true for Phase 0 Shadow.
    /// Setting this to false bypasses the guard (for testing or future write-enabled phases).
    void setForceReadOnly(bool enabled) { forceReadOnly_ = enabled; }
    bool forceReadOnly() const { return forceReadOnly_; }

    /// Counters for observability
    uint32_t blockedWrites() const { return blockedWrites_; }
    uint32_t allowedWrites() const { return allowedWrites_; }

private:
    bool isModbusWriteFunction(uint8_t functionCode) const;

    std::unique_ptr<Transport> underlying_;
    bool                       forceReadOnly_ = true;
    uint32_t                   blockedWrites_ = 0;
    uint32_t                   allowedWrites_ = 0;
};

}  // namespace heliograph
