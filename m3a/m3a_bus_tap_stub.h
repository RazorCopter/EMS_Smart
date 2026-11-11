// SPDX-License-Identifier: MIT
// M3A bus_tap stub - minimal stub for transport.cpp linking

#pragma once

#include <cstddef>
#include <cstdint>

namespace heliograph::diag {

class BusTap {
public:
    void recordTx(const uint8_t*, size_t, uint64_t) {}
    void recordRx(const uint8_t*, size_t, uint64_t) {}
};

}  // namespace heliograph::diag
