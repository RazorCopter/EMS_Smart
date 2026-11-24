// SPDX-License-Identifier: MIT
#include "transport/v51_read_only.h"

#include <array>
#ifdef V51_LIVE_TRIAL
#include <atomic>
#endif
#include <driver/uart.h>
#include <esp_timer.h>

#include "boards/waveshare_esp32_s3_relay_1ch.h"
#include "protocols/modbus/modbus_rtu.h"

namespace heliograph::v51 {
namespace {
constexpr auto kPort = static_cast<uart_port_t>(board::kRs485UartNum);
constexpr uint32_t kResponseTimeoutMs = 350;
#ifdef V51_LIVE_TRIAL
std::atomic<unsigned> writeCount{0};
#endif
}

bool ReadOnlyModbusTransport::begin(uint32_t baud) {
    uart_config_t config{};
    config.baud_rate = baud;
    config.data_bits = UART_DATA_8_BITS;
    config.parity = UART_PARITY_DISABLE;
    config.stop_bits = UART_STOP_BITS_1;
    config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    config.source_clk = UART_SCLK_DEFAULT;

    ready_ = uart_driver_install(kPort, 512, 0, 0, nullptr, 0) == ESP_OK;
    if (ready_) {
        ready_ = uart_param_config(kPort, &config) == ESP_OK &&
                 uart_set_pin(kPort, board::kRs485Tx, board::kRs485Rx,
                              board::kRs485De, UART_PIN_NO_CHANGE) == ESP_OK &&
                 uart_set_mode(kPort, UART_MODE_RS485_HALF_DUPLEX) == ESP_OK;
    }
    return ready_;
}

bool ReadOnlyModbusTransport::observeBus() {
    size_t size = 0;
    return ready_ && uart_get_buffered_data_len(kPort, &size) == ESP_OK && size == 0;
}

void ReadOnlyModbusTransport::flushInput() {
    if (ready_) uart_flush_input(kPort);
}

ReadOnlyModbusTransport::Result ReadOnlyModbusTransport::readInputRegisters(
    uint8_t slave, uint16_t address, uint16_t count, uint16_t* values) {
    return readRegisters(slave, modbus::kReadInputRegisters, address, count, values);
}

#ifdef V51_LIVE_TRIAL
unsigned modbusWriteCount() { return writeCount.load(); }

ReadOnlyModbusTransport::Result ReadOnlyModbusTransport::readHoldingRegisters(
    uint8_t slave, uint16_t address, uint16_t count, uint16_t* values) {
    return readRegisters(slave, modbus::kReadHoldingRegisters, address, count, values);
}

bool ReadOnlyModbusTransport::writeMultipleFrame(const uint8_t* frame, size_t size) {
    if (!frame || size < 11 || frame[0] < 1 || frame[0] > 247 ||
        frame[1] != modbus::kWriteMultipleRegisters || !observeBus())
        return false;
    const uint16_t count = uint16_t(frame[4]) << 8 | frame[5];
    if (!count || count > modbus::kMaxWriteRegisters || frame[6] != count * 2 ||
        size != size_t(frame[6]) + 9 ||
        modbus::crc16(frame, size - 2) != (uint16_t(frame[size - 1]) << 8 | frame[size - 2]))
        return false;
    ++writeCount;
    if (uart_write_bytes(kPort, frame, size) != static_cast<int>(size) ||
        uart_wait_tx_done(kPort, pdMS_TO_TICKS(50)) != ESP_OK)
        return false;

    uint8_t response[8]{};
    size_t length = 0;
    const int64_t deadline = esp_timer_get_time() + kResponseTimeoutMs * 1000;
    while (length < sizeof(response) && esp_timer_get_time() < deadline) {
        const int received = uart_read_bytes(kPort, response + length, 1, 1);
        if (received < 0) return false;
        length += received;
        if (length && response[0] != frame[0]) return false;
        if (length >= 2 && response[1] != frame[1]) return false;
    }
    modbus::WriteResponse result{};
    return length == sizeof(response) &&
           modbus::parseWriteResponse(response, length, frame[0], frame[1], result) ==
               modbus::ParseResult::Ok &&
           result.address == (uint16_t(frame[2]) << 8 | frame[3]) && result.value == count;
}
#endif

ReadOnlyModbusTransport::Result ReadOnlyModbusTransport::readRegisters(
    uint8_t slave, uint8_t function, uint16_t address, uint16_t count, uint16_t* values) {
    if (!ready_ || slave < 1 || slave > 247 || !values || count == 0 ||
        count > modbus::kMaxReadRegisters || uint32_t(address) + count > 65536) {
        return Result::Unavailable;
    }
    if (!observeBus()) {
        return Result::Traffic;
    }

    uint8_t request[8];
    size_t requestSize = 0;
    if (modbus::buildReadRequest(slave, function, address, count,
                                request, sizeof(request), requestSize) != modbus::BuildResult::Ok) {
        return Result::Unavailable;
    }
    if (uart_write_bytes(kPort, request, requestSize) != static_cast<int>(requestSize) ||
        uart_wait_tx_done(kPort, pdMS_TO_TICKS(50)) != ESP_OK) {
        return Result::Unavailable;
    }

    std::array<uint8_t, modbus::kMaxAdu> response{};
    size_t length = 0;
    const int64_t deadline = esp_timer_get_time() + kResponseTimeoutMs * 1000;
    while (esp_timer_get_time() < deadline) {
        const int received = uart_read_bytes(kPort, response.data() + length, 1, 1);
        if (received < 0) {
            return Result::Unavailable;
        }
        if (!received) {
            continue;
        }
        ++length;
        if (response[0] != slave) {
            return Result::Traffic;
        }
        if (length < 2) {
            continue;
        }
        const auto receivedFunction = response[1];
        if (receivedFunction != function &&
            receivedFunction != (function | modbus::kExceptionFlag)) {
            return Result::Traffic;
        }
        const size_t expected = receivedFunction == function ? 5 + 2 * count : 5;
        if (length == 3 && receivedFunction == function && response[2] != 2 * count) {
            return Result::Traffic;
        }
        if (length == expected) {
            modbus::ReadResponse result{};
            const auto parsed = modbus::parseReadResponse(response.data(), length, slave,
                                                          function, values,
                                                          count, result);
            if (parsed == modbus::ParseResult::Ok && result.registerCount == count) {
                return Result::Ok;
            }
            return parsed == modbus::ParseResult::Exception ? Result::Exception : Result::BadFrame;
        }
    }
    return length == 0 ? Result::Timeout : Result::BadFrame;
}
}
