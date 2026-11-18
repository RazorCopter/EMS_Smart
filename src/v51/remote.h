// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>

namespace heliograph::v51 {
bool beginRemote();
void tickRemote(bool healthy);
void recordLog(const char* message);
const char* managementPassword();
}
