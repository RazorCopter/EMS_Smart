// SPDX-License-Identifier: MIT
#include <Arduino.h>
#include <Wire.h>
#include <driver/gpio.h>
#include <esp_task_wdt.h>

#include "boards/waveshare_esp32_s3_relay_1ch.h"
#include "clock_service.h"
#include "remote.h"
#include "ems_runtime.h"
#include "m5.h"
#include "drivers/afore/v51_reader.h"
#include <esp_ota_ops.h>

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
v51::AforeReader reader;
bool readsStarted = false;

const char* sampleValue(const v51::Sample& sample, uint32_t now, char (&out)[20]) {
    const auto value = sample.usable(now);
    if (!value) return "null";
    snprintf(out, sizeof(out), "%.5g", *value);
    return out;
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
    if (!readsStarted && millis() >= 11000) {
        esp_ota_img_states_t otaState{};
        const auto* running = esp_ota_get_running_partition();
        if (running && esp_ota_get_state_partition(running, &otaState) == ESP_OK &&
            otaState == ESP_OTA_IMG_VALID) {
            readsStarted = true;
            reader.start(millis());
        }
    }
    reader.tick(millis());
    v51::serviceAux(readsStarted && watchdogReady && !reader.state().readHalted);
    if (millis() - lastHeartbeat >= 1000) {
        lastHeartbeat = millis();
        const auto clock=v51::clockStamp();
        char row[240]{};
        snprintf(row, sizeof(row), "SHADOW heartbeat=%lu reset=%d heap=%u min_heap=%u "
                 "psram=%u psram_free=%u time_source=%s watchdog=%s AUX1=%s gpio47=%d "
                 "modbus_write_count=0",
                 static_cast<unsigned long>(++sequence), int(esp_reset_reason()), ESP.getFreeHeap(),
                 ESP.getMinFreeHeap(), ESP.getPsramSize(), ESP.getFreePsram(), v51::timeSourceName(clock.source), watchdogReady ? "on" : "error", v51::auxOn() ? "ON" : "OFF", gpio_get_level(kAux));
        v51::recordLog(row);
        const auto& state = reader.state();
        char soc[20], grid[20], pv[20], battery[20], load[20], temperature[20], voltage[20], frequency[20];
        snprintf(row, sizeof(row), "EnergyState reads=%u errors=%u halted=%d soc=%s grid_w=%s pv_w=%s battery_w=%s load_w=%s temp_c=%s grid_v=%s hz=%s ages_ms=%u/%u/%u/%u",
                 state.goodReads, state.errors, state.readHalted,
                 sampleValue(state.soc, lastHeartbeat, soc), sampleValue(state.grid, lastHeartbeat, grid),
                 sampleValue(state.pv, lastHeartbeat, pv), sampleValue(state.battery, lastHeartbeat, battery),
                 sampleValue(state.load, lastHeartbeat, load), sampleValue(state.temperature, lastHeartbeat, temperature),
                 sampleValue(state.voltage, lastHeartbeat, voltage), sampleValue(state.frequency, lastHeartbeat, frequency),
                 state.soc.age(lastHeartbeat).value_or(0), state.grid.age(lastHeartbeat).value_or(0),
                 state.pv.age(lastHeartbeat).value_or(0), state.battery.age(lastHeartbeat).value_or(0));
        v51::recordLog(row);
        v51::tickEms(state,lastHeartbeat,sequence);
    }
    v51::tickRemote(remoteReady && watchdogReady && gpio_get_level(kAux) == 0 &&
                    ESP.getPsramSize() == 8 * 1024 * 1024);
    if (watchdogReady) esp_task_wdt_reset();
    delay(20);
}
