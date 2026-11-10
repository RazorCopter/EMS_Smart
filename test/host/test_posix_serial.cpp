// SPDX-License-Identifier: MIT
//
// Unit tests for PosixSerialTransport.
//
// These tests verify the configure/read/write/lock API without requiring actual hardware.
// Full integration testing with a real serial port is manual (or requires a pty/loopback).

#include "transport/posix_serial_transport.h"

#if !defined(ESP32)

#include <cstdio>

#include "transport/serial_profile.h"

using namespace heliograph;

static void test_construct_and_destroy() {
    PosixSerialTransport transport;
    // Should not crash
    printf("OK\n");
}

static void test_type_returns_rs485() {
    PosixSerialTransport transport;
    if (transport.type() != TransportType::Rs485) {
        printf("FAIL: type() != Rs485\n");
        exit(1);
    }
    printf("OK\n");
}

static void test_set_port_path() {
    PosixSerialTransport transport;
    transport.setPortPath("/dev/ttyUSB99");
    if (transport.portPath() != "/dev/ttyUSB99") {
        printf("FAIL: portPath() mismatch\n");
        exit(1);
    }
    printf("OK\n");
}

static void test_configure_fails_on_nonexistent_port() {
    PosixSerialTransport transport;
    transport.setPortPath("/dev/this_port_does_not_exist_12345");

    SerialProfile profile;
    profile.baudRate = 9600;
    profile.dataBits = 8;
    profile.parity = SerialParity::None;
    profile.stopBits = 1;

    bool result = transport.configure(profile);
    if (result) {
        printf("FAIL: configure() succeeded on nonexistent port\n");
        exit(1);
    }
    printf("OK\n");
}

static void test_lock_and_unlock() {
    PosixSerialTransport transport;

    bool locked = transport.lock(100);
    if (!locked) {
        printf("FAIL: initial lock failed\n");
        exit(1);
    }

    transport.unlock();

    // Lock again to verify unlock worked
    locked = transport.lock(100);
    if (!locked) {
        printf("FAIL: second lock failed after unlock\n");
        exit(1);
    }
    transport.unlock();

    printf("OK\n");
}

static void test_now_ms_advances() {
    PosixSerialTransport transport;

    uint64_t t1 = transport.nowMs();
    // Small busy-wait to ensure time advances
    for (volatile int i = 0; i < 1000000; ++i) {}
    uint64_t t2 = transport.nowMs();

    if (t2 <= t1) {
        printf("FAIL: nowMs() did not advance (t1=%llu, t2=%llu)\n",
               (unsigned long long)t1, (unsigned long long)t2);
        exit(1);
    }

    printf("OK\n");
}

int main() {
    printf("==> test_construct_and_destroy: ");
    test_construct_and_destroy();

    printf("==> test_type_returns_rs485: ");
    test_type_returns_rs485();

    printf("==> test_set_port_path: ");
    test_set_port_path();

    printf("==> test_configure_fails_on_nonexistent_port: ");
    test_configure_fails_on_nonexistent_port();

    printf("==> test_lock_and_unlock: ");
    test_lock_and_unlock();

    printf("==> test_now_ms_advances: ");
    test_now_ms_advances();

    printf("\nAll PosixSerialTransport tests passed.\n");
    return 0;
}

#else

int main() {
    printf("PosixSerialTransport tests skipped (ESP32 build)\n");
    return 0;
}

#endif  // !defined(ESP32)
