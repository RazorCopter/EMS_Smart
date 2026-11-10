// SPDX-License-Identifier: MIT

#include "readonly_guard.h"

#include "diagnostics/logger.h"
#include "protocols/byte_order.h"

namespace heliograph {

const char* blockReasonToString(BlockReasonCode code) {
    switch (code) {
        case BlockReasonCode::ALLOWED:                  return "ALLOWED";
        case BlockReasonCode::FUNCTION_NOT_ALLOWED:     return "FUNCTION_NOT_ALLOWED";
        case BlockReasonCode::EMPTY_FRAME:              return "EMPTY_FRAME";
        case BlockReasonCode::SHORT_FRAME:              return "SHORT_FRAME";
        case BlockReasonCode::INVALID_LENGTH:           return "INVALID_LENGTH";
        case BlockReasonCode::INVALID_CRC:              return "INVALID_CRC";
        case BlockReasonCode::BROADCAST_NOT_ALLOWED:    return "BROADCAST_NOT_ALLOWED";
        case BlockReasonCode::SLAVE_NOT_ALLOWED:        return "SLAVE_NOT_ALLOWED";
        case BlockReasonCode::QUANTITY_ZERO:            return "QUANTITY_ZERO";
        case BlockReasonCode::QUANTITY_OUT_OF_RANGE:    return "QUANTITY_OUT_OF_RANGE";
        case BlockReasonCode::ADDRESS_RANGE_OVERFLOW:   return "ADDRESS_RANGE_OVERFLOW";
        case BlockReasonCode::TRAILING_BYTES:           return "TRAILING_BYTES";
        default:                                        return "UNKNOWN";
    }
}

ReadOnlyGuard::ReadOnlyGuard(std::unique_ptr<Transport> underlying)
    : underlying_(std::move(underlying)) {}

bool ReadOnlyGuard::configure(const SerialProfile& profile) {
    // Extract slave ID from profile if available (for slave validation)
    // Note: SerialProfile doesn't currently have a slave_id field, so we rely on
    // explicit setConfiguredSlaveId() calls for now
    return underlying_->configure(profile);
}

bool ReadOnlyGuard::isAllowedReadFunction(uint8_t functionCode) const {
    // M2: ONLY FC03 (Read Holding Registers) and FC04 (Read Input Registers) allowed
    return functionCode == 0x03 || functionCode == 0x04;
}

uint16_t ReadOnlyGuard::computeModbusCrc16(const uint8_t* data, size_t len) const {
    // Modbus CRC-16: polynomial 0xA001, initial 0xFFFF, little-endian transmission
    // This is the standard Modbus RTU CRC algorithm
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; ++bit) {
            if (crc & 1) {
                crc = static_cast<uint16_t>((crc >> 1) ^ 0xA001);
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}

BlockReasonCode ReadOnlyGuard::validateModbusRtuFrame(const uint8_t* data, size_t len) {
    // M2 Frame-Aware Validation: Complete Modbus RTU FC03/FC04 request validation
    // Expected frame structure for FC03/FC04 RTU request (8 bytes):
    //   [0]    : slave_id (1 byte)
    //   [1]    : function_code (1 byte) - must be 0x03 or 0x04
    //   [2-3]  : start_address (2 bytes, big-endian)
    //   [4-5]  : quantity (2 bytes, big-endian)
    //   [6-7]  : crc16 (2 bytes, little-endian)

    // Check 1: Empty frame
    if (len == 0) {
        return BlockReasonCode::EMPTY_FRAME;
    }

    // Check 2: Minimum length (must be at least 8 bytes for FC03/FC04 RTU request)
    if (len < 8) {
        return BlockReasonCode::SHORT_FRAME;
    }

    // Check 3: Exact length (FC03/FC04 RTU request must be exactly 8 bytes, no trailing)
    if (len != 8) {
        return BlockReasonCode::INVALID_LENGTH;
    }

    const uint8_t slave_id = data[0];
    const uint8_t function_code = data[1];

    // Check 4: Broadcast not allowed (slave 0 is broadcast address in Modbus)
    if (slave_id == 0) {
        return BlockReasonCode::BROADCAST_NOT_ALLOWED;
    }

    // Check 5: Configured slave validation (if configured)
    if (configuredSlaveId_ != 0 && slave_id != configuredSlaveId_) {
        return BlockReasonCode::SLAVE_NOT_ALLOWED;
    }

    // Check 6: Function code must be FC03 or FC04
    if (!isAllowedReadFunction(function_code)) {
        return BlockReasonCode::FUNCTION_NOT_ALLOWED;
    }

    // Check 7: CRC-16 validation
    // Frame: [slave][fc][addr_h][addr_l][qty_h][qty_l][crc_l][crc_h]
    // CRC covers bytes [0..5], transmitted in bytes [6..7] as little-endian
    const uint16_t transmitted_crc = bytes::le16(data, 6);  // bytes 6-7, little-endian
    const uint16_t computed_crc = computeModbusCrc16(data, 6);  // CRC of first 6 bytes
    if (computed_crc != transmitted_crc) {
        return BlockReasonCode::INVALID_CRC;
    }

    // Check 8: Quantity validation
    const uint16_t start_address = bytes::be16(data, 2);  // bytes 2-3, big-endian
    const uint16_t quantity = bytes::be16(data, 4);       // bytes 4-5, big-endian

    if (quantity == 0) {
        return BlockReasonCode::QUANTITY_ZERO;
    }

    // Modbus RTU limit: max 125 registers per read (byte count field is uint8)
    if (quantity > 125) {
        return BlockReasonCode::QUANTITY_OUT_OF_RANGE;
    }

    // Check 9: Address range overflow (start_address + quantity must not overflow uint16)
    // Note: Modbus addresses are 0-based, so max valid address is 0xFFFF
    // start_address + quantity > 0x10000 would overflow
    if (static_cast<uint32_t>(start_address) + static_cast<uint32_t>(quantity) > 0x10000) {
        return BlockReasonCode::ADDRESS_RANGE_OVERFLOW;
    }

    // All checks passed
    return BlockReasonCode::ALLOWED;
}

size_t ReadOnlyGuard::write(const uint8_t* data, size_t len) {
    if (!forceReadOnly_) {
        // Guard disabled: pass through directly (for testing or non-Shadow builds)
        ++allowedFrameCount_;
        return underlying_->write(data, len);
    }

    // M2 Enforce read-only: frame-aware validation
    BlockReasonCode reason = validateModbusRtuFrame(data, len);

    if (reason != BlockReasonCode::ALLOWED) {
        // SECURITY_BLOCK: frame validation failed
        ++blockedFrameCount_;

        // Record blocked event (M2 audit trail)
        lastBlockedEvent_.timestamp_ms = underlying_->nowMs();
        lastBlockedEvent_.frame_length = len;
        lastBlockedEvent_.slave_id = (len > 0) ? data[0] : 0;
        lastBlockedEvent_.function_code = (len > 1) ? data[1] : 0;
        lastBlockedEvent_.reason = reason;

        // Log with detailed information (M2 observability)
        log::error(
            "readonly_guard: BLOCKED frame [len=%zu slave=0x%02X fc=0x%02X] reason=%s - "
            "Afore Shadow M2 read-only enforcement",
            len,
            lastBlockedEvent_.slave_id,
            lastBlockedEvent_.function_code,
            blockReasonToString(reason)
        );

        // M2: Zero bytes transmitted to underlying transport (physical isolation)
        // Return 0 to signal write failure to caller (no abort, no throw, process alive)
        return 0;
    }

    // Frame ALLOWED: valid FC03 or FC04 read request
    ++allowedFrameCount_;

    if (log::enabled(LogLevel::Trace)) {
        const uint8_t slave_id = data[0];
        const uint8_t function_code = data[1];
        const uint16_t start_address = bytes::be16(data, 2);
        const uint16_t quantity = bytes::be16(data, 4);
        log::trace(
            "readonly_guard: ALLOWED FC%02X slave=%u addr=0x%04X qty=%u (%zu bytes)",
            function_code, slave_id, start_address, quantity, len
        );
    }

    // Transmit to underlying transport
    return underlying_->write(data, len);
}

}  // namespace heliograph
