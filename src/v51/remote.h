// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>

namespace heliograph::v51 {
struct SystemResources {
    bool cpuValid = false;
    float cpuPct = 0.0f;
    float cpu0Pct = 0.0f;
    float cpu1Pct = 0.0f;
    uint32_t cpuMhz = 0;
    uint32_t taskCount = 0;
    uint32_t minTaskStackFreeB = 0;
    char minTaskStackName[16]{};
    uint32_t heapTotal = 0;
    uint32_t heapFree = 0;
    uint32_t heapMin = 0;
    uint32_t heapMaxAlloc = 0;
    float heapUsedPct = 0.0f;
    uint32_t psramTotal = 0;
    uint32_t psramFree = 0;
    uint32_t psramMin = 0;
    float psramUsedPct = 0.0f;
    uint32_t nvsUsedEntries = 0;
    uint32_t nvsFreeEntries = 0;
    uint32_t nvsTotalEntries = 0;
    float nvsUsedPct = 0.0f;
    uint32_t flashSize = 0;
    uint32_t sketchSize = 0;
    uint32_t sketchFree = 0;
};

bool beginRemote();
void tickRemote(bool healthy);
void recordLog(const char* message);
void copyManagementPassword(char (&output)[65]);
SystemResources sampleSystemResources();
}
