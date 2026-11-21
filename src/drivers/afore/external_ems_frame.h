// SPDX-License-Identifier: MIT
#pragma once

#include "protocols/modbus/modbus_rtu.h"

#include <array>
#include <optional>

namespace heliograph::afore {

// En_240312 specifies high word first. The caller must establish the mapping
// from EMS charge/discharge polarity to signed device watts before using it.
inline std::optional<std::array<uint8_t, 13>> externalPowerFrame(
        uint8_t unit, int32_t deviceWatts) {
    if (unit < 1 || unit > 247) return std::nullopt;
    const auto bits = static_cast<uint32_t>(deviceWatts);
    const uint16_t words[] = {static_cast<uint16_t>(bits >> 16), static_cast<uint16_t>(bits)};
    std::array<uint8_t, 13> frame{};
    size_t length = 0;
    if (modbus::buildWriteMultipleRegisters(unit, 0x0A09, words, 2, frame.data(),
            frame.size(), length) != modbus::BuildResult::Ok || length != frame.size())
        return std::nullopt;
    return frame;
}

}
