// SPDX-License-Identifier: MIT
//
// Generic ESP32-C3 board definition for development and bench testing.

#pragma once

#include <cstdint>

namespace heliograph::board {

inline constexpr const char *kName = "Generic ESP32-C3 Dev";
inline constexpr const char *kId = "esp32-c3-dev";

// RS-485 pins (set to standard defaults, de < 0 if not connected)
inline constexpr int kRs485Tx = 21;
inline constexpr int kRs485Rx = 20;
inline constexpr int kRs485De = -1;
inline constexpr int kRs485UartNum = 1;

// Relays
inline constexpr int kRelayCount = 0;
inline constexpr int kRelayPins[1] = {-1};
inline constexpr bool kRelayActiveHigh = true;

// RTC (not present on generic devkit)
inline constexpr bool kHasRtc = false;
inline constexpr int kRtcScl = -1;
inline constexpr int kRtcSda = -1;
inline constexpr uint8_t kRtcI2cAddress = 0x00;

// Boot button / Recovery
inline constexpr bool kHasBootButton = true;
inline constexpr int kBootPin = 9;

// Status LED (GPIO 8 or internal WS2812)
inline constexpr bool kHasStatusLed = false;
inline constexpr int kStatusLedPin = -1;

// Buzzer
inline constexpr bool kBuzzerActiveHigh = false;
inline constexpr bool kHasBuzzer = false;
inline constexpr int kBuzzerPin = -1;

} // namespace heliograph::board
