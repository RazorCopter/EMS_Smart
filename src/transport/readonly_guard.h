// SPDX-License-Identifier: MIT
//
// Read-only transport guard: Afore Shadow M2 hardening.
//
// Wraps any Transport implementation and enforces a strict read-only policy with multi-level
// defense for Afore Shadow mode. This guard performs frame-aware validation of Modbus RTU
// requests and blocks ANY frame that is not a valid, complete FC03 or FC04 read request.
//
// M2 ENFORCEMENT LAYERS (Trust Boundary):
//   1. Modbus function code: ONLY 0x03 (Read Holding) and 0x04 (Read Input) allowed
//   2. Frame structure: slave ID > 0, valid length (8 bytes for FC03/FC04 RTU request)
//   3. CRC-16 validation: computed CRC must match transmitted CRC (Modbus-16, polynomial 0xA001)
//   4. Quantity validation: > 0 and <= 125 registers (Modbus RTU limit)
//   5. Address range: start_address + quantity must not overflow uint16
//   6. Trailing bytes: zero bytes after expected frame length
//   7. Configured slave validation: slave ID must match configured device address
//
// BLOCKED FRAME BEHAVIOR (SECURITY_BLOCK):
//   - Zero bytes transmitted to underlying transport (lower transport write() NOT called)
//   - Increment blocked_frame_count metric
//   - Log event with timestamp, frame length, slave ID, FC, specific ReasonCode
//   - Process remains alive and operational (no abort, no exit, no throw)
//   - Return 0 to caller to signal write failure
//
// FRAME AUTHORIZATION:
//   Allowed: Modbus RTU FC03 or FC04, slave > 0 configured, valid CRC, valid quantity,
//            no address overflow, exact 8-byte length, no trailing bytes
//   Blocked: All other frames including FC01, FC02, FC05, FC06, FC0F, FC10 (0x10), FC16, FC17,
//            unknown FC, broadcast (slave 0), wrong slave, bad CRC, invalid quantity,
//            short/long/empty frames, trailing bytes

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

#include "transport.h"

namespace heliograph {

/// Reason codes for blocked frames (M2 observability and audit trail)
enum class BlockReasonCode : uint8_t {
    ALLOWED = 0,                    // Frame passed all checks and was transmitted
    FUNCTION_NOT_ALLOWED,           // FC not 0x03 or 0x04
    EMPTY_FRAME,                    // len == 0
    SHORT_FRAME,                    // len < 8
    INVALID_LENGTH,                 // len != 8 for FC03/FC04 RTU request
    INVALID_CRC,                    // CRC-16 mismatch
    BROADCAST_NOT_ALLOWED,          // slave ID == 0
    SLAVE_NOT_ALLOWED,              // slave ID not in configured set
    QUANTITY_ZERO,                  // quantity == 0
    QUANTITY_OUT_OF_RANGE,          // quantity > 125
    ADDRESS_RANGE_OVERFLOW,         // start_address + quantity > 0x10000
    TRAILING_BYTES,                 // bytes after expected frame length
};

/// Human-readable reason code names (for logging)
const char* blockReasonToString(BlockReasonCode code);

/// Event logged for each blocked frame (M2 audit trail)
struct BlockedFrameEvent {
    uint64_t        timestamp_ms;
    size_t          frame_length;
    uint8_t         slave_id;
    uint8_t         function_code;
    BlockReasonCode reason;
};

/// Transport wrapper that enforces Afore Shadow read-only operation with frame-aware validation.
///
/// M2 requirement: the Afore Shadow build must be structurally and physically incapable of
/// transmitting ANY Modbus RTU frame different from a valid FC03/FC04 read request with correct
/// CRC-16, valid slave ID, valid quantity, and exact length.
///
/// Defense-in-depth: even if application logic constructs a write command, it cannot reach
/// the physical serial port. Zero bytes transmitted for blocked frames.
class ReadOnlyGuard : public Transport {
public:
    /// Wraps an existing transport. The guard takes ownership via unique_ptr.
    explicit ReadOnlyGuard(std::unique_ptr<Transport> underlying);
    ~ReadOnlyGuard() override = default;

    TransportType type() const override { return underlying_->type(); }
    bool configure(const SerialProfile& profile) override;
    void flushInput() override { underlying_->flushInput(); }

    /// Intercepts writes with frame-aware validation. If forceReadOnly is true (default),
    /// performs complete Modbus RTU FC03/FC04 validation including CRC-16, slave ID, quantity,
    /// address range, and frame structure. Returns 0 and logs BlockedFrameEvent if blocked.
    size_t write(const uint8_t* data, size_t len) override;

    size_t   read(uint8_t* buf, size_t len, uint32_t timeoutMs) override {
        return underlying_->read(buf, len, timeoutMs);
    }
    uint64_t nowMs() const override { return underlying_->nowMs(); }
    bool     lock(uint32_t timeoutMs) override { return underlying_->lock(timeoutMs); }
    void     unlock() override { underlying_->unlock(); }

    /// Controls frame validation. Default true for Afore Shadow M2.
    /// Setting false bypasses all checks (for testing or non-Shadow builds).
    void setForceReadOnly(bool enabled) { forceReadOnly_ = enabled; }
    bool forceReadOnly() const { return forceReadOnly_; }

    /// Configures allowed slave ID (default 0 = accept all non-broadcast).
    /// When set to non-zero, only frames for that slave pass validation.
    void setConfiguredSlaveId(uint8_t slaveId) { configuredSlaveId_ = slaveId; }
    uint8_t configuredSlaveId() const { return configuredSlaveId_; }

    /// Counters for observability (M2 metrics)
    uint32_t blockedFrameCount() const { return blockedFrameCount_; }
    uint32_t allowedFrameCount() const { return allowedFrameCount_; }

    /// Access last blocked event (for testing and diagnostics)
    const BlockedFrameEvent& lastBlockedEvent() const { return lastBlockedEvent_; }

    // Legacy counters (kept for compatibility with existing tests)
    uint32_t blockedWrites() const { return blockedFrameCount_; }
    uint32_t allowedWrites() const { return allowedFrameCount_; }

private:
    /// Frame-aware validation: checks FC, CRC, slave, quantity, length, address range
    BlockReasonCode validateModbusRtuFrame(const uint8_t* data, size_t len);

    /// Checks if function code is Read Holding (0x03) or Read Input (0x04)
    bool isAllowedReadFunction(uint8_t functionCode) const;

    /// Computes Modbus CRC-16 (polynomial 0xA001, initial 0xFFFF, little-endian)
    uint16_t computeModbusCrc16(const uint8_t* data, size_t len) const;

    std::unique_ptr<Transport> underlying_;
    bool                       forceReadOnly_ = true;
    uint8_t                    configuredSlaveId_ = 0;  // 0 = accept all non-broadcast
    uint32_t                   blockedFrameCount_ = 0;
    uint32_t                   allowedFrameCount_ = 0;
    BlockedFrameEvent          lastBlockedEvent_{};
};

}  // namespace heliograph
