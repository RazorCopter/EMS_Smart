// SPDX-License-Identifier: MIT
#pragma once
#include <ArduinoJson.h>
#include <esp_http_server.h>
#include <string>
#include <cstdint>
#include "shadow_core.h"

namespace heliograph::v51 {
struct ManualResult { bool accepted; const char* reason; };
struct HaUsageFlags {
    bool forecast = false;
    bool ratio = false;
    bool weather = false;
    bool loadProfile = false;
};
void serviceAux(bool healthy);
bool auxOn();
ManualResult manualCommand(const char* action, JsonVariantConst payload, const char* origin,
                           unsigned maximumSeconds = 86400);
bool acceptHaInput(const char* kind, const char* data, size_t size);
void setHaUsage(HaUsageFlags usage);
PredictiveInputs readPredictiveInputs(time_t nowUtc, time_t targetDeadlineUtc);
std::string haSummary(bool contents, bool dashboard=false);
void rejectRetainedHaInput(const char* kind);
std::string manualState();
void registerM5(httpd_handle_t server, bool(*auth)(httpd_req_t*));
}
