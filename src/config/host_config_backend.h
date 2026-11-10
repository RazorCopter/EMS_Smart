// SPDX-License-Identifier: MIT
//
// Host configuration backend for Linux daemon.
//
// Phase 0 Shadow configuration: reads from JSON file or environment variables, avoiding any
// ESP32-specific storage (NVS). This keeps the host daemon's config separate from the embedded
// firmware's config model while reusing the core Configuration structure where applicable.

#pragma once

#if !defined(ESP32)

#include <string>

#include "config/configuration.h"

namespace heliograph {

/// Configuration loader for Linux host.
///
/// Reads configuration from:
///   1. JSON file specified by --config or HELIOGRAPH_CONFIG env var
///   2. Environment variables (HELIOGRAPH_* prefix)
///   3. Built-in defaults
///
/// Does NOT persist changes: this is read-only configuration for Phase 0 Shadow.
class HostConfigBackend {
public:
    HostConfigBackend() = default;

    /// Loads configuration from file. Returns false on parse error or missing required fields.
    bool loadFromFile(const std::string& path, Configuration& config);

    /// Loads configuration from environment variables.
    /// Useful for containerized deployments where file-based config is inconvenient.
    void loadFromEnvironment(Configuration& config);

    /// Writes the last load error message (if any).
    const std::string& lastError() const { return lastError_; }

private:
    std::string lastError_;
};

}  // namespace heliograph

#endif  // !defined(ESP32)
