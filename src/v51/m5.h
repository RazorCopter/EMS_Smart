// SPDX-License-Identifier: MIT
#pragma once
#include <ArduinoJson.h>
#include <esp_http_server.h>
#include <string>
#include <cstdint>
namespace heliograph::v51 {
struct ManualResult { bool accepted; const char* reason; };
void serviceAux(bool healthy);
bool auxOn();
ManualResult manualCommand(const char* action, JsonVariantConst payload, const char* origin,
                           unsigned maximumSeconds = 86400);
bool acceptHaInput(const char* kind, const char* data, size_t size);
std::string haSummary(bool contents, bool dashboard=false);
void rejectRetainedHaInput(const char* kind);
std::string manualState();
void registerM5(httpd_handle_t server, bool(*auth)(httpd_req_t*));
}
