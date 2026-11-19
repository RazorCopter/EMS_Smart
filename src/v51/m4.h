// SPDX-License-Identifier: MIT
#pragma once
#include "energy_state.h"
#include <Preferences.h>
#include <esp_http_server.h>
#include <string>


namespace heliograph::v51 {
using Authorize = bool (*)(httpd_req_t *);
bool mqttConnected();
uint32_t mqttReconnectCount();
void registerM4(httpd_handle_t server, Authorize auth, Preferences &storage);
void recordDecision(const EnergyState &, uint32_t now,
                    const std::string &record);
void appendPolicyJson(std::ostream &);
bool applyPolicy(const std::string &form, uint32_t leaseMs = 0, int cancel = 0);
const unsigned char *dashboardPage();
size_t dashboardSize();
} // namespace heliograph::v51
