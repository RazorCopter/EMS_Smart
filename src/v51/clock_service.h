// SPDX-License-Identifier: MIT
#pragma once

#include <ArduinoJson.h>
#include <Preferences.h>
#include <esp_http_server.h>
#include <ctime>
#include <ostream>

namespace heliograph::v51 {
enum class TimeSource { Unsynced, Rtc, Ntp };
struct ClockStamp {
    uint32_t uptime;
    time_t utc;
    TimeSource source;
};

void loadClock(Preferences& preferences);
bool registerClock(httpd_handle_t server,bool (*authorize)(httpd_req_t*));
void serviceClock(bool wifiConnected);
ClockStamp clockStamp();
const char* timeSourceName(TimeSource source);
void addClock(JsonObject json, ClockStamp stamp);
void appendClock(std::ostream& out, ClockStamp stamp);
}
