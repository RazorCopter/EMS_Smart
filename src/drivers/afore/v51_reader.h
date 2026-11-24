// SPDX-License-Identifier: MIT
#pragma once

#include "transport/v51_read_only.h"
#include "v51/energy_state.h"
#include <array>
#ifdef V51_LIVE_TRIAL
#include "external_ems_actuator.h"
#endif

namespace heliograph::v51 {

class AforeReader
#ifdef V51_LIVE_TRIAL
    : public afore::ExternalEmsBus
#endif
{
public:
    void start(uint32_t now);
    void tick(uint32_t now);
    const EnergyState& state() const { return state_; }
#ifdef V51_LIVE_TRIAL
    bool ownsBus() const override { return started_ && probed_ && !state_.readHalted; }
    bool readHolding(uint8_t unit, uint16_t address, uint16_t* words, uint16_t count) override;
    bool writeFrame(const uint8_t* frame, size_t size) override;
    uint8_t unit() const;
#endif

private:
    ReadOnlyModbusTransport transport_;
    EnergyState state_;
    uint32_t nextPoll_ = 0;
    uint32_t haltedAt_ = 0;
    uint8_t phase_ = 0, lowIndex_ = 0;
    std::array<uint16_t, 125> gridWords_{};
    uint32_t gridStamp_ = 0;
    unsigned failures_ = 0;
    bool started_ = false;
    bool probed_ = false;
};
}
