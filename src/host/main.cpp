// SPDX-License-Identifier: MIT
//
// Heliograph host daemon entry point (Phase 0 Shadow).
//
// Linux-native daemon for read-only inverter monitoring. This is a placeholder main()
// demonstrating the integration of PosixSerialTransport, ReadOnlyGuard, and host configuration.
// Full daemon implementation (polling loop, MQTT, REST API) is scaffolded for future phases.

#if !defined(ESP32)

#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <thread>

#include "config/configuration.h"
#include "config/host_config_backend.h"
#include "diagnostics/logger.h"
#include "transport/posix_serial_transport.h"
#include "transport/readonly_guard.h"

using namespace heliograph;

static volatile bool g_running = true;

static void signalHandler(int sig) {
    (void)sig;
    g_running = false;
}

static void printUsage(const char* prog) {
    printf("Usage: %s [options]\n", prog);
    printf("Options:\n");
    printf("  --config PATH       Load configuration from JSON file\n");
    printf("  --port PATH         Serial port (default: /dev/ttyUSB0)\n");
    printf("  --baud RATE         Baud rate (default: 9600)\n");
    printf("  --log-level LEVEL   Log level: error|warn|info|debug|trace (default: info)\n");
    printf("  --help              Show this help\n");
    printf("\n");
    printf("Environment variables:\n");
    printf("  HELIOGRAPH_SERIAL_PORT      Override serial port\n");
    printf("  HELIOGRAPH_LOG_LEVEL        Override log level\n");
    printf("  HELIOGRAPH_POLL_INTERVAL    Poll interval in seconds\n");
}

int main(int argc, char* argv[]) {
    // Parse command-line arguments
    std::string configPath;
    std::string serialPort = "/dev/ttyUSB0";
    uint32_t baudRate = 9600;
    LogLevel logLevel = LogLevel::Info;

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--help") == 0 || std::strcmp(argv[i], "-h") == 0) {
            printUsage(argv[0]);
            return 0;
        } else if (std::strcmp(argv[i], "--config") == 0 && i + 1 < argc) {
            configPath = argv[++i];
        } else if (std::strcmp(argv[i], "--port") == 0 && i + 1 < argc) {
            serialPort = argv[++i];
        } else if (std::strcmp(argv[i], "--baud") == 0 && i + 1 < argc) {
            baudRate = static_cast<uint32_t>(std::atoi(argv[++i]));
        } else if (std::strcmp(argv[i], "--log-level") == 0 && i + 1 < argc) {
            if (!parseLogLevel(argv[++i], logLevel)) {
                fprintf(stderr, "Invalid log level: %s\n", argv[i]);
                return 1;
            }
        } else {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            printUsage(argv[0]);
            return 1;
        }
    }

    // Override from environment
    if (const char* envPort = std::getenv("HELIOGRAPH_SERIAL_PORT")) {
        serialPort = envPort;
    }
    if (const char* envLevel = std::getenv("HELIOGRAPH_LOG_LEVEL")) {
        parseLogLevel(envLevel, logLevel);
    }

    // Initialize logging
    log::setLevel(logLevel);

    log::info("heliograph-daemon starting (Phase 0 Shadow)");
    log::info("  serial port: %s", serialPort.c_str());
    log::info("  baud rate: %u", baudRate);
    log::info("  log level: %s", logLevelName(logLevel));

    // Load configuration
    Configuration config;
    HostConfigBackend configBackend;

    if (!configPath.empty()) {
        if (!configBackend.loadFromFile(configPath, config)) {
            log::error("failed to load config: %s", configBackend.lastError().c_str());
            return 1;
        }
        // Re-apply log level from config if it was loaded
        log::setLevel(config.logging.level);
    } else {
        log::info("no config file specified, using defaults");
    }

    // Load additional settings from environment
    configBackend.loadFromEnvironment(config);

    // Create transport stack: PosixSerialTransport wrapped by ReadOnlyGuard
    auto posixTransport = std::make_unique<PosixSerialTransport>();
    posixTransport->setPortPath(serialPort);

    SerialProfile profile;
    profile.baudRate = baudRate;
    profile.dataBits = 8;
    profile.parity = SerialParity::None;
    profile.stopBits = 1;
    profile.responseTimeoutMs = 1000;

    if (!posixTransport->configure(profile)) {
        log::error("failed to configure serial transport");
        return 1;
    }

    // Wrap with read-only guard (default enabled for Phase 0)
    auto transport = std::make_unique<ReadOnlyGuard>(std::move(posixTransport));
    log::info("read-only guard enabled (Phase 0 Shadow mode)");

    // Install signal handlers
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    log::info("daemon ready, press Ctrl+C to stop");

    // Main loop placeholder
    // TODO: Integrate discovery, polling, MQTT publishing
    while (g_running) {
        std::this_thread::sleep_for(std::chrono::seconds(1));

        // Placeholder: log heartbeat every 10 seconds
        static uint64_t lastHeartbeat = 0;
        uint64_t now = transport->nowMs();
        if (now - lastHeartbeat > 10000) {
            log::debug("daemon heartbeat (blocked writes: %u, allowed: %u)",
                       transport->blockedWrites(), transport->allowedWrites());
            lastHeartbeat = now;
        }
    }

    log::info("daemon shutting down");
    return 0;
}

#else

int main() {
    fprintf(stderr, "This daemon is for Linux host only (not ESP32)\n");
    return 1;
}

#endif  // !defined(ESP32)
