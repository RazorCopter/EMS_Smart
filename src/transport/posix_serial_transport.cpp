// SPDX-License-Identifier: MIT

#include "posix_serial_transport.h"

#if !defined(ESP32)

#include <chrono>
#include <cstring>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>

#include "diagnostics/logger.h"

namespace heliograph {

PosixSerialTransport::PosixSerialTransport() {}

PosixSerialTransport::~PosixSerialTransport() {
    closePort();
}

void PosixSerialTransport::closePort() {
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
    configured_ = false;
}

bool PosixSerialTransport::configure(const SerialProfile& profile) {
    closePort();

    // Open serial port
    fd_ = ::open(portPath_.c_str(), O_RDWR | O_NOCTTY | O_NDELAY);
    if (fd_ < 0) {
        log::error("posix_serial: failed to open %s: %s", portPath_.c_str(), std::strerror(errno));
        return false;
    }

    // Configure as blocking for read/write operations
    int flags = fcntl(fd_, F_GETFL, 0);
    if (flags == -1) {
        log::error("posix_serial: fcntl F_GETFL failed: %s", std::strerror(errno));
        closePort();
        return false;
    }
    if (fcntl(fd_, F_SETFL, flags & ~O_NONBLOCK) == -1) {
        log::error("posix_serial: fcntl F_SETFL failed: %s", std::strerror(errno));
        closePort();
        return false;
    }

    struct termios tty;
    std::memset(&tty, 0, sizeof(tty));

    if (tcgetattr(fd_, &tty) != 0) {
        log::error("posix_serial: tcgetattr failed: %s", std::strerror(errno));
        closePort();
        return false;
    }

    // Set baud rate
    speed_t speed;
    switch (profile.baudRate) {
        case 1200: speed = B1200; break;
        case 2400: speed = B2400; break;
        case 4800: speed = B4800; break;
        case 9600: speed = B9600; break;
        case 19200: speed = B19200; break;
        case 38400: speed = B38400; break;
        case 57600: speed = B57600; break;
        case 115200: speed = B115200; break;
        default:
            log::error("posix_serial: unsupported baud rate %u", profile.baudRate);
            closePort();
            return false;
    }
    cfsetospeed(&tty, speed);
    cfsetispeed(&tty, speed);

    // Character size (data bits)
    tty.c_cflag &= ~CSIZE;
    switch (profile.dataBits) {
        case 5: tty.c_cflag |= CS5; break;
        case 6: tty.c_cflag |= CS6; break;
        case 7: tty.c_cflag |= CS7; break;
        case 8: tty.c_cflag |= CS8; break;
        default:
            log::error("posix_serial: unsupported data bits %u", profile.dataBits);
            closePort();
            return false;
    }

    // Parity
    switch (profile.parity) {
        case SerialParity::None:
            tty.c_cflag &= ~PARENB;
            break;
        case SerialParity::Even:
            tty.c_cflag |= PARENB;
            tty.c_cflag &= ~PARODD;
            break;
        case SerialParity::Odd:
            tty.c_cflag |= PARENB;
            tty.c_cflag |= PARODD;
            break;
    }

    // Stop bits
    if (profile.stopBits == 1) {
        tty.c_cflag &= ~CSTOPB;
    } else if (profile.stopBits == 2) {
        tty.c_cflag |= CSTOPB;
    } else {
        log::error("posix_serial: unsupported stop bits %u", profile.stopBits);
        closePort();
        return false;
    }

    // Control flags: enable receiver, ignore modem control lines
    tty.c_cflag |= (CLOCAL | CREAD);

    // Disable hardware flow control
    tty.c_cflag &= ~CRTSCTS;

    // Input flags: no software flow control, no special handling
    tty.c_iflag &= ~(IXON | IXOFF | IXANY);
    tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL);

    // Output flags: raw output
    tty.c_oflag &= ~OPOST;

    // Local flags: raw mode, no echo, no signals
    tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ECHONL | ISIG | IEXTEN);

    // Read behavior: VMIN=0, VTIME=0 means non-blocking reads (return immediately)
    // We handle timeouts in read() using select()
    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = 0;

    // Apply settings
    if (tcsetattr(fd_, TCSANOW, &tty) != 0) {
        log::error("posix_serial: tcsetattr failed: %s", std::strerror(errno));
        closePort();
        return false;
    }

    // Flush any stale data
    tcflush(fd_, TCIOFLUSH);

    profile_ = profile;
    configured_ = true;

    log::info("posix_serial: configured %s at %u baud, %u%c%u", portPath_.c_str(),
              profile.baudRate, profile.dataBits,
              profile.parity == SerialParity::None ? 'N'
                                                   : (profile.parity == SerialParity::Even ? 'E' : 'O'),
              profile.stopBits);

    return true;
}

void PosixSerialTransport::flushInput() {
    if (fd_ >= 0) {
        tcflush(fd_, TCIFLUSH);
    }
}

size_t PosixSerialTransport::write(const uint8_t* data, size_t len) {
    if (fd_ < 0 || !configured_) {
        return 0;
    }

    ssize_t written = ::write(fd_, data, len);
    if (written < 0) {
        log::warn("posix_serial: write failed: %s", std::strerror(errno));
        const size_t n = 0;
        tapTx(data, n);
        return n;
    }

    const size_t n = static_cast<size_t>(written);
    tapTx(data, n);
    return n;
}

size_t PosixSerialTransport::read(uint8_t* buf, size_t len, uint32_t timeoutMs) {
    if (fd_ < 0 || !configured_ || len == 0) {
        tapRx(buf, 0);
        return 0;
    }

    // Use select() for timeout
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(fd_, &readfds);

    struct timeval tv;
    tv.tv_sec = timeoutMs / 1000;
    tv.tv_usec = (timeoutMs % 1000) * 1000;

    int ret = select(fd_ + 1, &readfds, nullptr, nullptr, &tv);
    if (ret < 0) {
        log::warn("posix_serial: select failed: %s", std::strerror(errno));
        tapRx(buf, 0);
        return 0;
    }

    if (ret == 0) {
        // Timeout
        tapRx(buf, 0);
        return 0;
    }

    // Data available
    ssize_t n = ::read(fd_, buf, len);
    if (n < 0) {
        log::warn("posix_serial: read failed: %s", std::strerror(errno));
        tapRx(buf, 0);
        return 0;
    }

    const size_t result = static_cast<size_t>(n);
    tapRx(buf, result);
    return result;
}

uint64_t PosixSerialTransport::nowMs() const {
    using namespace std::chrono;
    return static_cast<uint64_t>(
        duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
}

bool PosixSerialTransport::lock(uint32_t timeoutMs) {
    return busMutex_.try_lock_for(std::chrono::milliseconds(timeoutMs));
}

void PosixSerialTransport::unlock() {
    busMutex_.unlock();
}

}  // namespace heliograph

#endif  // !defined(ESP32)
