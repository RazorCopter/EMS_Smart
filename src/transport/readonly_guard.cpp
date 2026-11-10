// SPDX-License-Identifier: MIT

#include "readonly_guard.h"

#include "diagnostics/logger.h"

namespace heliograph {

ReadOnlyGuard::ReadOnlyGuard(std::unique_ptr<Transport> underlying)
    : underlying_(std::move(underlying)) {}

bool ReadOnlyGuard::isModbusWriteFunction(uint8_t functionCode) const {
    // Modbus write function codes as defined in the Modbus Application Protocol Specification
    switch (functionCode) {
        case 0x05:  // Write Single Coil
        case 0x06:  // Write Single Register
        case 0x0F:  // Write Multiple Coils (15 decimal)
        case 0x10:  // Write Multiple Registers (16 decimal)
        case 0x16:  // Mask Write Register (22 decimal)
        case 0x17:  // Read/Write Multiple Registers (23 decimal)
            return true;
        default:
            return false;
    }
}

size_t ReadOnlyGuard::write(const uint8_t* data, size_t len) {
    if (!forceReadOnly_) {
        // Guard disabled: pass through directly
        ++allowedWrites_;
        return underlying_->write(data, len);
    }

    // Enforce read-only: inspect Modbus frame structure
    // Modbus RTU frame: [unit_id] [function_code] [data...] [crc_low] [crc_high]
    // Minimum valid frame is 4 bytes (unit + function + 2-byte CRC), but we only need 2 to
    // detect the function code.
    if (len < 2) {
        // Too short to be a valid Modbus frame, but also too short to contain a function code.
        // Pass through: this might be a fragment or a non-Modbus protocol. The underlying
        // transport or protocol handler will deal with it.
        log::trace("readonly_guard: frame too short (%zu bytes), passing through", len);
        ++allowedWrites_;
        return underlying_->write(data, len);
    }

    const uint8_t unitId       = data[0];
    const uint8_t functionCode = data[1];

    if (isModbusWriteFunction(functionCode)) {
        // BLOCKED: this is a Modbus write function code
        ++blockedWrites_;
        log::error(
            "readonly_guard: BLOCKED Modbus write function 0x%02X to unit %u (%zu bytes) - "
            "Phase 0 Shadow mode is read-only",
            functionCode, unitId, len);

        // Return 0 to signal write failure. The caller (typically a Modbus client) will treat
        // this as a bus error.
        return 0;
    }

    // Allowed: read function code (0x01-0x04) or other non-write operation
    if (log::enabled(LogLevel::Trace)) {
        log::trace("readonly_guard: allowing function 0x%02X to unit %u (%zu bytes)", functionCode,
                   unitId, len);
    }
    ++allowedWrites_;
    return underlying_->write(data, len);
}

}  // namespace heliograph
