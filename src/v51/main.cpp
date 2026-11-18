// SPDX-License-Identifier: MIT
#include <Arduino.h>
#include <Wire.h>
#include <driver/gpio.h>
#include <esp_task_wdt.h>

#include "boards/waveshare_esp32_s3_relay_1ch.h"
#include "network/rtc_time.h"
#include "remote.h"

namespace {
using namespace heliograph;
static_assert(board::kRelayCount == 1 && board::kRelayPins[0] == 47 && board::kRelayActiveHigh);
constexpr auto kAux = static_cast<gpio_num_t>(board::kRelayPins[0]);

void disableAux() {
    gpio_set_level(kAux, 0);
    gpio_set_direction(kAux, GPIO_MODE_INPUT_OUTPUT);
}

bool watchdogReady = false;
bool remoteReady = false;

const char* readRtc(time_t& utc) {
    Wire.beginTransmission(board::kRtcI2cAddress);
    Wire.write(0x04);
    if (Wire.endTransmission(false) != 0 || Wire.requestFrom(board::kRtcI2cAddress, 7) != 7) {
        return "i2c_unavailable";
    }
    rtc::Registers bytes{};
    for (auto& value : bytes) {
        value = static_cast<uint8_t>(Wire.read());
    }
    if (bytes[0] & 0x80) return "oscillator_stopped";
    return rtc::decodeRegisters(bytes, utc) ? "valid" : "invalid_time";
}

void serialSetup() {
    static char command[20]{};
    static size_t length = 0;
    for (int bytes = 0; bytes < 32 && Serial.available(); ++bytes) {
        const int input = Serial.read();
        if (input == '\n') {
            if (length == 9 && memcmp(command, "SETUP KEY", 9) == 0) {
                Serial.printf("setup_admin_password=%s\n", v51::managementPassword());
            }
            length = 0;
        } else if (input != '\r' && length < sizeof(command)) {
            command[length++] = static_cast<char>(input);
        }
    }
}
}

// Arduino invokes this before setup and before any application service is created.
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
    remoteReady = v51::beginRemote();
    v51::recordLog(remoteReady ? "HTTP service started" : "HTTP service unavailable");
}

void loop() {
    static uint32_t sequence = 0;
    static uint32_t lastHeartbeat = millis() - 1000;
    serialSetup();
    if (millis() - lastHeartbeat >= 1000) {
        lastHeartbeat = millis();
        time_t utc = 0;
        const char* rtcStatus = readRtc(utc);
        char row[240]{};
        snprintf(row, sizeof(row), "SHADOW heartbeat=%lu reset=%d heap=%u min_heap=%u "
                 "psram=%u psram_free=%u rtc=%s utc=%lld watchdog=%s AUX1=DISABLED gpio47=%d "
                 "modbus_write_count=0",
                 static_cast<unsigned long>(++sequence), int(esp_reset_reason()), ESP.getFreeHeap(),
                 ESP.getMinFreeHeap(), ESP.getPsramSize(), ESP.getFreePsram(), rtcStatus,
                 static_cast<long long>(utc), watchdogReady ? "on" : "error", gpio_get_level(kAux));
        v51::recordLog(row);
    }
    v51::tickRemote(remoteReady && watchdogReady && gpio_get_level(kAux) == 0 &&
                    ESP.getPsramSize() == 8 * 1024 * 1024);
    if (watchdogReady) esp_task_wdt_reset();
    delay(20);
}
