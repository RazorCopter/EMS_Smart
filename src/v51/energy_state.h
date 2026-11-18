// SPDX-License-Identifier: MIT
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>

namespace heliograph::v51 {
enum class Quality { Missing, Valid, ReadError, Invalid, Stale };

struct Sample {
    std::optional<double> value;
    uint32_t stamp = 0;
    Quality status = Quality::Missing;

    Quality quality(uint32_t now) const {
        return status == Quality::Valid && now - stamp > 5000 ? Quality::Stale : status;
    }

    std::optional<double> usable(uint32_t now) const {
        return quality(now) == Quality::Valid ? value : std::nullopt;
    }

    std::optional<uint32_t> age(uint32_t now) const {
        if (!value) {
            return std::nullopt;
        }
        return now - stamp;
    }
};

struct EnergyState {
    Sample soc, pv, grid, battery, inverter, load, temperature, voltage, frequency;
    uint32_t goodReads = 0;
    uint32_t errors = 0;
    bool readHalted = false;
};
}
