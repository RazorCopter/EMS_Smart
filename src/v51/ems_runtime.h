// SPDX-License-Identifier: MIT
#pragma once
#include "shadow_core.h"
#include <esp_http_server.h>
#include <Preferences.h>

namespace heliograph::v51 {
void loadPolicy(Preferences& preferences);
void tickEms(const EnergyState& state,uint32_t now,uint32_t sequence);
void registerEms(httpd_handle_t server,bool (*authorize)(httpd_req_t*));
}
