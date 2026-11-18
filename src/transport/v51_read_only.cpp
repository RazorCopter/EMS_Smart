// SPDX-License-Identifier: MIT
#include "transport/v51_read_only.h"

#include <array>
#include <driver/uart.h>
#include <esp_timer.h>

#include "boards/waveshare_esp32_s3_relay_1ch.h"
#include "protocols/modbus/modbus_rtu.h"

namespace heliograph::v51 {
namespace {
constexpr auto kPort = static_cast<uart_port_t>(board::kRs485UartNum);
constexpr uint32_t kResponseTimeoutMs = 350;
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

ReadOnlyModbusTransport::Result ReadOnlyModbusTransport::readInputRegisters(
    uint8_t slave, uint16_t address, uint16_t count, uint16_t* values) {
    if (!ready_ || slave < 1 || slave > 247 || !values || count == 0 ||
        count > modbus::kMaxReadRegisters || uint32_t(address) + count > 65536) {
        return Result::Unavailable;
    }
    if (!observeBus()) {
        return Result::Traffic;
    }

    uint8_t request[8];
    size_t requestSize = 0;
    if (modbus::buildReadRequest(slave, modbus::kReadInputRegisters, address, count,
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
        const auto function = response[1];
        if (function != modbus::kReadInputRegisters &&
            function != (modbus::kReadInputRegisters | modbus::kExceptionFlag)) {
            return Result::Traffic;
        }
        const size_t expected = function == modbus::kReadInputRegisters ? 5 + 2 * count : 5;
        if (length == 3 && function == modbus::kReadInputRegisters && response[2] != 2 * count) {
            return Result::Traffic;
        }
        if (length == expected) {
            modbus::ReadResponse result{};
            const auto parsed = modbus::parseReadResponse(response.data(), length, slave,
                                                          modbus::kReadInputRegisters, values,
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
