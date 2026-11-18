// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>

namespace heliograph::v51 {

class ReadOnlyModbusTransport {
public:
    enum class Result { Ok, Timeout, Exception, Traffic, BadFrame, Unavailable };

    bool begin(uint32_t baud);
    bool observeBus();
    Result readInputRegisters(uint8_t slave, uint16_t address, uint16_t count, uint16_t* values);

private:
    bool ready_ = false;
};
}
