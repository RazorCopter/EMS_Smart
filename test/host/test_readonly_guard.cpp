// SPDX-License-Identifier: MIT
//
// Unit tests for ReadOnlyGuard transport wrapper.

#include "transport/readonly_guard.h"

#include <cstring>
#include <memory>
#include <vector>

#include "test/support/mock_transport.h"

using namespace heliograph;
using namespace heliograph::test;

// Test helper: creates a minimal Modbus RTU frame
std::vector<uint8_t> makeModbusFrame(uint8_t unitId, uint8_t functionCode) {
    std::vector<uint8_t> frame = {unitId, functionCode, 0x00, 0x01, 0x00, 0x02};
    // CRC placeholder (not validated by the guard)
    frame.push_back(0x00);
    frame.push_back(0x00);
    return frame;
}

static void test_allows_read_function_codes() {
    auto mock = std::make_unique<MockTransport>();
    MockTransport* mockPtr = mock.get();
    ReadOnlyGuard guard(std::move(mock));

    // Read Holding Registers (0x03)
    auto frame = makeModbusFrame(1, 0x03);
    size_t written = guard.write(frame.data(), frame.size());
    if (written != frame.size()) {
        printf("FAIL: 0x03 was blocked (wrote %zu/%zu bytes)\n", written, frame.size());
        exit(1);
    }
    if (mockPtr->writes.size() != 1) {
        printf("FAIL: 0x03 did not reach underlying transport\n");
        exit(1);
    }

    // Read Input Registers (0x04)
    frame = makeModbusFrame(1, 0x04);
    written = guard.write(frame.data(), frame.size());
    if (written != frame.size()) {
        printf("FAIL: 0x04 was blocked\n");
        exit(1);
    }

    if (guard.allowedWrites() != 2 || guard.blockedWrites() != 0) {
        printf("FAIL: counters wrong (allowed=%u, blocked=%u)\n", guard.allowedWrites(),
               guard.blockedWrites());
        exit(1);
    }

    printf("OK\n");
}

static void test_blocks_write_single_register() {
    auto mock = std::make_unique<MockTransport>();
    MockTransport* mockPtr = mock.get();
    ReadOnlyGuard guard(std::move(mock));

    // Write Single Register (0x06)
    auto frame = makeModbusFrame(1, 0x06);
    size_t written = guard.write(frame.data(), frame.size());
    if (written != 0) {
        printf("FAIL: 0x06 was not blocked (wrote %zu bytes)\n", written);
        exit(1);
    }
    if (!mockPtr->writes.empty()) {
        printf("FAIL: 0x06 reached underlying transport\n");
        exit(1);
    }
    if (guard.blockedWrites() != 1) {
        printf("FAIL: blocked counter not incremented\n");
        exit(1);
    }

    printf("OK\n");
}

static void test_blocks_write_multiple_registers() {
    auto mock = std::make_unique<MockTransport>();
    ReadOnlyGuard guard(std::move(mock));

    // Write Multiple Registers (0x10)
    auto frame = makeModbusFrame(1, 0x10);
    size_t written = guard.write(frame.data(), frame.size());
    if (written != 0) {
        printf("FAIL: 0x10 was not blocked\n");
        exit(1);
    }
    if (guard.blockedWrites() != 1) {
        printf("FAIL: blocked counter not incremented\n");
        exit(1);
    }

    printf("OK\n");
}

static void test_blocks_write_single_coil() {
    auto mock = std::make_unique<MockTransport>();
    ReadOnlyGuard guard(std::move(mock));

    // Write Single Coil (0x05)
    auto frame = makeModbusFrame(1, 0x05);
    size_t written = guard.write(frame.data(), frame.size());
    if (written != 0) {
        printf("FAIL: 0x05 was not blocked\n");
        exit(1);
    }

    printf("OK\n");
}

static void test_blocks_write_multiple_coils() {
    auto mock = std::make_unique<MockTransport>();
    ReadOnlyGuard guard(std::move(mock));

    // Write Multiple Coils (0x0F)
    auto frame = makeModbusFrame(1, 0x0F);
    size_t written = guard.write(frame.data(), frame.size());
    if (written != 0) {
        printf("FAIL: 0x0F was not blocked\n");
        exit(1);
    }

    printf("OK\n");
}

static void test_bypass_when_disabled() {
    auto mock = std::make_unique<MockTransport>();
    MockTransport* mockPtr = mock.get();
    ReadOnlyGuard guard(std::move(mock));

    guard.setForceReadOnly(false);

    // Write Single Register (0x06) should now pass through
    auto frame = makeModbusFrame(1, 0x06);
    size_t written = guard.write(frame.data(), frame.size());
    if (written != frame.size()) {
        printf("FAIL: 0x06 was blocked even with guard disabled\n");
        exit(1);
    }
    if (mockPtr->writes.size() != 1) {
        printf("FAIL: 0x06 did not reach underlying transport\n");
        exit(1);
    }
    if (guard.blockedWrites() != 0) {
        printf("FAIL: blocked counter incremented when disabled\n");
        exit(1);
    }

    printf("OK\n");
}

static void test_passthrough_for_short_frames() {
    auto mock = std::make_unique<MockTransport>();
    MockTransport* mockPtr = mock.get();
    ReadOnlyGuard guard(std::move(mock));

    // Single byte (too short to contain function code)
    uint8_t short_frame[] = {0x42};
    size_t written = guard.write(short_frame, 1);
    if (written != 1) {
        printf("FAIL: short frame was blocked\n");
        exit(1);
    }
    if (mockPtr->writes.size() != 1) {
        printf("FAIL: short frame did not pass through\n");
        exit(1);
    }

    printf("OK\n");
}

int main() {
    printf("==> test_allows_read_function_codes: ");
    test_allows_read_function_codes();

    printf("==> test_blocks_write_single_register: ");
    test_blocks_write_single_register();

    printf("==> test_blocks_write_multiple_registers: ");
    test_blocks_write_multiple_registers();

    printf("==> test_blocks_write_single_coil: ");
    test_blocks_write_single_coil();

    printf("==> test_blocks_write_multiple_coils: ");
    test_blocks_write_multiple_coils();

    printf("==> test_bypass_when_disabled: ");
    test_bypass_when_disabled();

    printf("==> test_passthrough_for_short_frames: ");
    test_passthrough_for_short_frames();

    printf("\nAll ReadOnlyGuard tests passed.\n");
    return 0;
}
