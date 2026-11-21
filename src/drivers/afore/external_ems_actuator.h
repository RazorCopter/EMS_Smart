// SPDX-License-Identifier: MIT
#pragma once

#include "external_ems_frame.h"
#include "device/live_actuator.h"

#include <limits>

namespace heliograph::afore {

class ExternalEmsBus {
public:
    virtual ~ExternalEmsBus() = default;
    virtual bool ownsBus() const = 0;
    virtual bool readHolding(uint8_t unit, uint16_t address, uint16_t* words, uint16_t count) = 0;
    // Success requires validated FC10 echo, including unit, address and quantity.
    // The bus owner must reject a transaction if ownership has lapsed.
    virtual bool writeFrame(const uint8_t* frame, size_t size) = 0;
};

// Its settings remain fixed for the lifetime of the session.
class ExternalEmsActuator final : public v51::LiveActuator {
public:
    ExternalEmsActuator(ExternalEmsBus& bus, uint8_t unit, uint16_t nativeMode, bool invertSign)
        : bus_(bus), unit_(unit), nativeMode_(nativeMode), invertSign_(invertSign) {}

    bool hasControl() const override {
        return unit_ >= 1 && unit_ <= 247 && nativeMode_ != kExternalMode && bus_.ownsBus();
    }

    static std::optional<uint16_t> observedNativeMode(ExternalEmsBus& bus, uint8_t unit) {
        uint16_t mode = 0;
        if (unit < 1 || unit > 247 || !bus.ownsBus() ||
            !bus.readHolding(unit, kMode, &mode, 1) || mode == kExternalMode)
            return std::nullopt;
        return mode;
    }

    bool enterExternalBatteryControl() {
        uint16_t type = 0;
        const auto current = observedNativeMode(bus_, unit_);
        return hasControl() && current == nativeMode_ &&
               bus_.readHolding(unit_, kControlType, &type, 1) && type == 0 &&
               writeMode(kExternalMode) && isExternalBatteryControl();
    }

    const char* lastError() const { return lastError_; }

    std::optional<int> writeAndReadBack(int watts) override {
        lastError_ = "NONE";
        const int64_t wireWatts = invertSign_ ? -int64_t(watts) : int64_t(watts);
        if (!isExternalBatteryControl()) {
            lastError_ = "MODE_OR_CONTROL_LOST";
            return std::nullopt;
        }
        if (wireWatts < std::numeric_limits<int32_t>::min() ||
            wireWatts > std::numeric_limits<int32_t>::max()) {
            lastError_ = "RANGE_ERROR";
            return std::nullopt;
        }
        const auto frame = externalPowerFrame(unit_, static_cast<int32_t>(wireWatts));
        if (!frame || !bus_.writeFrame(frame->data(), frame->size())) {
            lastError_ = "WRITE_FAILED";
            return std::nullopt;
        }
        uint16_t words[2]{};
        if (!bus_.readHolding(unit_, kPower, words, 2) &&
            !bus_.readHolding(unit_, kPower, words, 2)) {
            lastError_ = "READBACK_IO_FAILED";
            return std::nullopt;
        }
        const auto bits = (uint32_t(words[0]) << 16) | words[1];
        if (bits != static_cast<uint32_t>(wireWatts)) {
            lastError_ = "READBACK_MISMATCH";
            return std::nullopt;
        }
        return watts;
    }

    bool release() override {
        uint16_t mode = 0;
        if (!hasControl() || !bus_.readHolding(unit_, kMode, &mode, 1)) return false;
        if (mode != kExternalMode) return mode == nativeMode_;

        writeMode(nativeMode_);
        return bus_.readHolding(unit_, kMode, &mode, 1) && mode == nativeMode_;
    }

private:
    bool writeMode(uint16_t mode) {
        std::array<uint8_t, 11> frame{};
        size_t length = 0;
        return modbus::buildWriteMultipleRegisters(unit_, kMode, &mode, 1,
                   frame.data(), frame.size(), length) == modbus::BuildResult::Ok &&
               bus_.writeFrame(frame.data(), length);
    }

    static constexpr uint16_t kMode = 0x09C4;
    static constexpr uint16_t kControlType = 0x0A08;
    static constexpr uint16_t kPower = 0x0A09;
    static constexpr uint16_t kExternalMode = 5;

    bool isExternalBatteryControl() {
        uint16_t mode = 0, type = 0;
        return hasControl() && bus_.readHolding(unit_, kMode, &mode, 1) &&
               mode == kExternalMode && bus_.readHolding(unit_, kControlType, &type, 1) && type == 0;
    }

    ExternalEmsBus& bus_;
    uint8_t unit_;
    uint16_t nativeMode_;
    bool invertSign_;
    const char* lastError_ = "NONE";
};

}
