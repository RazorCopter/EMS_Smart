// SPDX-License-Identifier: MIT
#include <Arduino.h>
#include <Wire.h>
#include <driver/gpio.h>
#include <esp_task_wdt.h>

#include "boards/waveshare_esp32_s3_relay_1ch.h"
#include "network/rtc_time.h"

namespace {
using namespace heliograph;
static_assert(board::kRelayCount == 1 && board::kRelayPins[0] == 47 && board::kRelayActiveHigh);
constexpr auto kAux = static_cast<gpio_num_t>(board::kRelayPins[0]);

void disableAux() {
    gpio_set_level(kAux, 0);
    gpio_set_direction(kAux, GPIO_MODE_OUTPUT);
}

bool watchdogReady = false;

bool readRtc(time_t& utc) {
    Wire.beginTransmission(board::kRtcI2cAddress);
    Wire.write(0x04);
    if (Wire.endTransmission(false) != 0 || Wire.requestFrom(board::kRtcI2cAddress, 7) != 7) {
        return false;
    }
    rtc::Registers bytes{};
    for (auto& value : bytes) {
        value = static_cast<uint8_t>(Wire.read());
    }
    return rtc::decodeRegisters(bytes, utc);
}
}

// Arduino calls this before setup and before any application service is created.
extern "C" void initVariant() {
    disableAux();
}

void setup() {
    disableAux();
    Serial.begin(115200);
    Serial.setTxTimeoutMs(0);
    Wire.begin(board::kRtcSda, board::kRtcScl, 100000);
    Wire.setTimeOut(50);

    if (esp_task_wdt_status(nullptr) == ESP_ERR_INVALID_STATE) {
        const esp_task_wdt_config_t config{10000, 0, true};
        esp_task_wdt_init(&config);
    }
    watchdogReady = esp_task_wdt_status(nullptr) == ESP_OK || esp_task_wdt_add(nullptr) == ESP_OK;
}

void loop() {
    static uint32_t heartbeat = 0;
    time_t utc = 0;
    const bool rtcValid = readRtc(utc);
    Serial.printf("v5.1 SHADOW heartbeat=%lu uptime_ms=%lu reset=%d heap=%u min_heap=%u "
                  "psram=%u psram_free=%u rtc=%s utc=%lld watchdog=%s AUX1=DISABLED gpio47=%d "
                  "modbus_write_count=0\n",
                  static_cast<unsigned long>(++heartbeat), static_cast<unsigned long>(millis()),
                  static_cast<int>(esp_reset_reason()), ESP.getFreeHeap(), ESP.getMinFreeHeap(),
                  ESP.getPsramSize(), ESP.getFreePsram(), rtcValid ? "valid" : "unavailable",
                  static_cast<long long>(utc), watchdogReady ? "on" : "error", gpio_get_level(kAux));
    if (watchdogReady) {
        esp_task_wdt_reset();
    }
    delay(1000);
}
