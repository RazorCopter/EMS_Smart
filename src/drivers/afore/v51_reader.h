// SPDX-License-Identifier: MIT
#pragma once

#include "transport/v51_read_only.h"
#include "v51/energy_state.h"

namespace heliograph::v51 {

class AforeReader {
public:
    void start(uint32_t now);
    void tick(uint32_t now);
    const EnergyState& state() const { return state_; }

private:
    ReadOnlyModbusTransport transport_;
    EnergyState state_;
    uint32_t nextPoll_ = 0;
    size_t block_ = 0;
    unsigned failures_ = 0;
    bool started_ = false;
    bool probed_ = false;
};
}
