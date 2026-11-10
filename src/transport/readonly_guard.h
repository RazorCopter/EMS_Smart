// SPDX-License-Identifier: MIT
//
// Read-only transport guard: Afore Shadow M2 hardening (CORRECTED).
//
// Wraps any Transport implementation and enforces a strict read-only policy with multi-level
// defense for Afore Shadow mode. This guard performs frame-aware validation of Modbus RTU
// requests and blocks ANY frame that is not a valid, complete FC03 or FC04 read request.
//
// M2 SAFETY CORRECTIONS:
//   1. IMMUTABLE under AFORE_SHADOW_BUILD: setForceReadOnly() removed, always enforced
//   2. MANDATORY slave ID: constructor requires explicit slave ID, no wildcard mode
//   3. CRC VERIFIED: uses modbus::crc16() with correct polynomial 0xA001
//
// M2 ENFORCEMENT LAYERS (Trust Boundary):
//   1. Modbus function code: ONLY 0x03 (Read Holding) and 0x04 (Read Input) allowed
//   2. Frame structure: slave ID > 0, matches configured slave, valid length (8 bytes)
//   3. CRC-16 validation: computed CRC must match transmitted CRC (Modbus-16, polynomial 0xA001)
//   4. Quantity validation: > 0 and <= 125 registers (Modbus RTU limit)
//   5. Address range: start_address + quantity must not overflow uint16
//   6. Trailing bytes: zero bytes after expected frame length
//
// BLOCKED FRAME BEHAVIOR (SECURITY_BLOCK):
//   - Zero bytes transmitted to underlying transport (lower transport write() NOT called)
//   - Increment blocked_frame_count metric
//   - Log event with timestamp, frame length, slave ID, FC, specific ReasonCode
//   - Process remains alive and operational (no abort, no exit, no throw)
//   - Return 0 to caller to signal write failure
//
// FRAME AUTHORIZATION:
//   Allowed: Modbus RTU FC03 or FC04, slave == configuredSlaveId, valid CRC, valid quantity,
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
    SLAVE_NOT_ALLOWED,              // slave ID != configured slave
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
/// CRC-16, valid slave ID matching configured slave, valid quantity, and exact length.
///
/// M2 SAFETY CORRECTIONS:
///   - IMMUTABLE: under AFORE_SHADOW_BUILD, read-only enforcement is always active, no API to disable
///   - MANDATORY SLAVE: constructor requires explicit configured slave ID (1 for AF5K-SL), no wildcard
///   - VERIFIED CRC: uses modbus::crc16() from modbus_rtu.h (polynomial 0xA001 confirmed correct)
///
/// Defense-in-depth: even if application logic constructs a write command, it cannot reach
/// the physical serial port. Zero bytes transmitted for blocked frames.
class ReadOnlyGuard : public Transport {
public:
    /// Wraps an existing transport with mandatory slave ID enforcement.
    /// 
    /// M2 CORRECTION: Slave ID is MANDATORY, no default value.
    /// For Afore AF5K-SL deployment: configuredSlaveId = 1
    /// 
    /// @param underlying Lower transport (takes ownership via unique_ptr)
    /// @param configuredSlaveId Modbus slave ID to enforce (must be > 0, typically 1 for AF5K-SL)
    explicit ReadOnlyGuard(std::unique_ptr<Transport> underlying, uint8_t configuredSlaveId);
    
    ~ReadOnlyGuard() override = default;

    TransportType type() const override { return underlying_->type(); }
    bool configure(const SerialProfile& profile) override;
    void flushInput() override { underlying_->flushInput(); }

    /// Intercepts writes with frame-aware validation.
    /// 
    /// M2 CORRECTION: Under AFORE_SHADOW_BUILD, always enforces read-only (no disable API).
    /// Standard Heliograph build retains forceReadOnly_ flag for compatibility.
    /// 
    /// Returns 0 and logs BlockedFrameEvent if blocked.
    size_t write(const uint8_t* data, size_t len) override;

    size_t   read(uint8_t* buf, size_t len, uint32_t timeoutMs) override {
        return underlying_->read(buf, len, timeoutMs);
    }
    uint64_t nowMs() const override { return underlying_->nowMs(); }
    bool     lock(uint32_t timeoutMs) override { return underlying_->lock(timeoutMs); }
    void     unlock() override { underlying_->unlock(); }

#if !defined(AFORE_SHADOW_BUILD)
    /// Controls frame validation (standard Heliograph build only).
    /// 
    /// M2 CORRECTION: This API is NOT AVAILABLE in AFORE_SHADOW_BUILD.
    /// Afore Shadow build always enforces read-only, no runtime disable possible.
    /// 
    /// @param enabled true = enforce read-only, false = pass through all frames
    void setForceReadOnly(bool enabled) { forceReadOnly_ = enabled; }
    
    /// Returns current read-only enforcement state (standard build only).
    bool forceReadOnly() const { return forceReadOnly_; }
#endif

    /// Returns configured slave ID (immutable after construction).
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
    /// 
    /// M2 CORRECTION: Uses modbus::crc16() from modbus_rtu.h (polynomial 0xA001 verified correct).
    BlockReasonCode validateModbusRtuFrame(const uint8_t* data, size_t len);

    /// Checks if function code is Read Holding (0x03) or Read Input (0x04)
    bool isAllowedReadFunction(uint8_t functionCode) const;

    std::unique_ptr<Transport> underlying_;
    
#if !defined(AFORE_SHADOW_BUILD)
    bool forceReadOnly_ = true;  // Standard build: configurable (default true)
#endif
    // AFORE_SHADOW_BUILD: no forceReadOnly_ member, always enforced (compile-time immutable)
    
    uint8_t  configuredSlaveId_;     // M2 CORRECTION: mandatory, immutable, no wildcard mode
    uint32_t blockedFrameCount_ = 0;
    uint32_t allowedFrameCount_ = 0;
    BlockedFrameEvent lastBlockedEvent_{};
};

}  // namespace heliograph
