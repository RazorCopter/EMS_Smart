// SPDX-License-Identifier: MIT
//
// POSIX serial transport for Linux host using termios.
//
// Implements ITransport for native Linux environments, enabling the daemon to run on x86_64
// for Phase 0 Shadow mode. No Arduino, no ESP-IDF: pure POSIX <termios.h>, open(), read(),
// write(), close().

#pragma once

#include <mutex>
#include <string>

#include "transport.h"

#if !defined(ESP32)

namespace heliograph {

/// Serial transport for Linux/POSIX using termios.
///
/// Phase 0 Shadow requirement: read-only monitoring of an inverter from a Linux host before
/// any write capability is enabled. This transport speaks RS232/RS485 serial via standard
/// POSIX interfaces, with no hardware dependency on ESP32.
class PosixSerialTransport : public Transport {
public:
    /// Constructs an unconfigured transport. Call configure() with a port path like
    /// "/dev/ttyUSB0" before use.
    PosixSerialTransport();
    ~PosixSerialTransport() override;

    TransportType type() const override { return TransportType::Rs485; }

    /// Opens the serial port and applies line settings. `profile.baudRate`, parity, data bits,
    /// and stop bits are applied via tcsetattr(). Safe to call again to reconfigure or switch
    /// ports.
    ///
    /// The port path is passed as the FIRST component of the profile (if extended), or defaults
    /// to the member `portPath_` if set separately. This maintains compatibility with existing
    /// SerialProfile usage while allowing host-specific port specification.
    bool configure(const SerialProfile& profile) override;

    void   flushInput() override;
    size_t write(const uint8_t* data, size_t len) override;
    size_t read(uint8_t* buf, size_t len, uint32_t timeoutMs) override;
    uint64_t nowMs() const override;

    bool lock(uint32_t timeoutMs) override;
    void unlock() override;

    /// Sets the device path (e.g., "/dev/ttyUSB0") to use on next configure().
    void setPortPath(const std::string& path) { portPath_ = path; }
    const std::string& portPath() const { return portPath_; }

private:
    void closePort();

    std::timed_mutex busMutex_;
    int              fd_ = -1;
    bool             configured_ = false;
    std::string      portPath_ = "/dev/ttyUSB0";  // default for Linux
    SerialProfile    profile_{};
};

}  // namespace heliograph

#endif  // !defined(ESP32)
