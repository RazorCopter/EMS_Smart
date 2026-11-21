// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <cstddef>

namespace heliograph::v51 {

#ifdef V51_LIVE_TRIAL
unsigned modbusWriteCount();
#else
inline constexpr unsigned modbusWriteCount() { return 0; }
#endif

class ReadOnlyModbusTransport {
public:
    enum class Result { Ok, Timeout, Exception, Traffic, BadFrame, Unavailable };

    bool begin(uint32_t baud);
    bool observeBus();
    Result readInputRegisters(uint8_t slave, uint16_t address, uint16_t count, uint16_t* values);
#ifdef V51_LIVE_TRIAL
    Result readHoldingRegisters(uint8_t slave, uint16_t address, uint16_t count, uint16_t* values);
    bool writeMultipleFrame(const uint8_t* frame, size_t size);
#endif

private:
    Result readRegisters(uint8_t slave, uint8_t function, uint16_t address, uint16_t count,
                         uint16_t* values);
    bool ready_ = false;
};
}
