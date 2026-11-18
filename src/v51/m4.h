// SPDX-License-Identifier: MIT
#pragma once
#include <esp_http_server.h>
#include <Preferences.h>
#include <string>
#include "energy_state.h"

namespace heliograph::v51 {
using Authorize = bool (*)(httpd_req_t*);
void registerM4(httpd_handle_t server, Authorize auth, Preferences& storage);
void recordDecision(const EnergyState&, uint32_t now, const std::string& record);
void appendPolicyJson(std::ostream&);
bool applyPolicy(const std::string& form, uint32_t leaseMs=0, int cancel=0);
const unsigned char* dashboardPage();
size_t dashboardSize();
}
