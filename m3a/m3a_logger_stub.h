// SPDX-License-Identifier: MIT
// M3A Logger Stub - Minimal logger for safety core testing without dependencies

#pragma once

#include <cstdarg>
#include <cstdio>

namespace heliograph {

enum class LogLevel : uint8_t { Error, Warn, Info, Debug, Trace };

namespace log {

inline void error(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    printf("[M3A ERROR] ");
    vprintf(fmt, args);
    printf("\n");
    va_end(args);
}

inline void trace(const char* fmt, ...) {
    // Suppress trace in M3A tests
    (void)fmt;
}

inline bool enabled(LogLevel level) {
    return level == LogLevel::Error;
}

}  // namespace log
}  // namespace heliograph
