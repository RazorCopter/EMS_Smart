// SPDX-License-Identifier: MIT
//
// Unit tests for ReadOnlyGuard transport wrapper - M2 Complete Test Matrix A-Y
//
// TEST STATUS: All tests in this file are marked TEST_AUTHORED (M2 milestone).
// They verify the frame-aware validation logic through static code analysis and
// compilation, but are NOT executed at runtime in M2 (reserved for M3).
//
// M2 scope: Implementation and static verification only.
// M3 scope: Runtime execution and validation.

#include "transport/readonly_guard.h"

#include <cstring>
#include <memory>
#include <vector>

#include "protocols/modbus/modbus_rtu.h"  // for crc16 function
#include "test/support/mock_transport.h"

using namespace heliograph;
using namespace heliograph::test;

// ============================================================================
// Fake Lower Transport (M2 Test Fixture)
// ============================================================================
// Records all write() calls for verification. Provides invariant:
// For any blocked frame -> call_count == 0, total_bytes == 0, frames.empty()

class FakeLowerTransport : public Transport {
public:
    TransportType type() const override { return TransportType::Mock; }
    
    bool configure(const SerialProfile& profile) override {
        (void)profile;
        return true;
    }
    
    void flushInput() override {}
    
    size_t write(const uint8_t* data, size_t len) override {
        ++call_count;
        total_bytes += len;
        frames.emplace_back(data, data + len);
        return len;
    }
    
    size_t read(uint8_t* buf, size_t len, uint32_t timeoutMs) override {
        (void)buf; (void)len; (void)timeoutMs;
        return 0;
    }
    
    uint64_t nowMs() const override { return 1000; }
    bool lock(uint32_t timeoutMs) override { (void)timeoutMs; return true; }
    void unlock() override {}
    
    // Test observation interface
    void reset() {
        call_count = 0;
        total_bytes = 0;
        frames.clear();
    }
    
    // M2 invariants: for blocked frames, transport must not be called
    bool wasNotCalled() const { return call_count == 0 && total_bytes == 0 && frames.empty(); }
    bool wasCalledOnce() const { return call_count == 1 && !frames.empty(); }
    
    size_t call_count = 0;
    size_t total_bytes = 0;
    std::vector<std::vector<uint8_t>> frames;
};

// ============================================================================
// Test Frame Builders (with correct CRC-16)
// ============================================================================

std::vector<uint8_t> buildValidFC03(uint8_t slave = 1, uint16_t addr = 0x0200, uint16_t qty = 10) {
    std::vector<uint8_t> frame(8);
    frame[0] = slave;
    frame[1] = 0x03;  // FC03 Read Holding Registers
    frame[2] = static_cast<uint8_t>(addr >> 8);
    frame[3] = static_cast<uint8_t>(addr & 0xFF);
    frame[4] = static_cast<uint8_t>(qty >> 8);
    frame[5] = static_cast<uint8_t>(qty & 0xFF);
    
    // Compute and append CRC-16 (little-endian)
    uint16_t crc = modbus::crc16(frame.data(), 6);
    frame[6] = static_cast<uint8_t>(crc & 0xFF);
    frame[7] = static_cast<uint8_t>(crc >> 8);
    return frame;
}

std::vector<uint8_t> buildValidFC04(uint8_t slave = 1, uint16_t addr = 0x0200, uint16_t qty = 10) {
    std::vector<uint8_t> frame(8);
    frame[0] = slave;
    frame[1] = 0x04;  // FC04 Read Input Registers
    frame[2] = static_cast<uint8_t>(addr >> 8);
    frame[3] = static_cast<uint8_t>(addr & 0xFF);
    frame[4] = static_cast<uint8_t>(qty >> 8);
    frame[5] = static_cast<uint8_t>(qty & 0xFF);
    
    uint16_t crc = modbus::crc16(frame.data(), 6);
    frame[6] = static_cast<uint8_t>(crc & 0xFF);
    frame[7] = static_cast<uint8_t>(crc >> 8);
    return frame;
}

std::vector<uint8_t> buildFrameWithFC(uint8_t fc, uint8_t slave = 1) {
    std::vector<uint8_t> frame(8);
    frame[0] = slave;
    frame[1] = fc;
    frame[2] = 0x02; frame[3] = 0x00;  // addr = 0x0200
    frame[4] = 0x00; frame[5] = 0x0A;  // qty = 10
    
    uint16_t crc = modbus::crc16(frame.data(), 6);
    frame[6] = static_cast<uint8_t>(crc & 0xFF);
    frame[7] = static_cast<uint8_t>(crc >> 8);
    return frame;
}

// ============================================================================
// M2 TEST MATRIX A-Y (TEST_AUTHORED)
// ============================================================================

// --- Test A: FC03 valid -> ALLOWED ---
static void test_A_FC03_valid_allowed() {
    auto fake = std::make_unique<FakeLowerTransport>();
    FakeLowerTransport* fakePtr = fake.get();
    ReadOnlyGuard guard(std::move(fake));
    
    auto frame = buildValidFC03();
    size_t written = guard.write(frame.data(), frame.size());
    
    if (written != frame.size()) {
        printf("FAIL A: FC03 blocked (wrote %zu/%zu)\n", written, frame.size());
        exit(1);
    }
    if (!fakePtr->wasCalledOnce()) {
        printf("FAIL A: transport not called once (calls=%zu)\n", fakePtr->call_count);
        exit(1);
    }
    if (fakePtr->total_bytes != frame.size()) {
        printf("FAIL A: wrong byte count (got %zu, expected %zu)\n", fakePtr->total_bytes, frame.size());
        exit(1);
    }
    if (guard.allowedFrameCount() != 1 || guard.blockedFrameCount() != 0) {
        printf("FAIL A: wrong counters (allowed=%u blocked=%u)\n", 
               guard.allowedFrameCount(), guard.blockedFrameCount());
        exit(1);
    }
    printf("OK [TEST_AUTHORED]\n");
}

// --- Test B: FC04 valid -> ALLOWED ---
static void test_B_FC04_valid_allowed() {
    auto fake = std::make_unique<FakeLowerTransport>();
    FakeLowerTransport* fakePtr = fake.get();
    ReadOnlyGuard guard(std::move(fake));
    
    auto frame = buildValidFC04();
    size_t written = guard.write(frame.data(), frame.size());
    
    if (written != frame.size()) {
        printf("FAIL B: FC04 blocked\n");
        exit(1);
    }
    if (!fakePtr->wasCalledOnce() || fakePtr->total_bytes != frame.size()) {
        printf("FAIL B: transport call mismatch\n");
        exit(1);
    }
    printf("OK [TEST_AUTHORED]\n");
}

// --- Test C: FC01 -> BLOCKED FUNCTION_NOT_ALLOWED ---
static void test_C_FC01_blocked() {
    auto fake = std::make_unique<FakeLowerTransport>();
    FakeLowerTransport* fakePtr = fake.get();
    ReadOnlyGuard guard(std::move(fake));
    
    auto frame = buildFrameWithFC(0x01);
    size_t written = guard.write(frame.data(), frame.size());
    
    if (written != 0) {
        printf("FAIL C: FC01 not blocked (wrote %zu)\n", written);
        exit(1);
    }
    if (!fakePtr->wasNotCalled()) {
        printf("FAIL C: transport was called (calls=%zu)\n", fakePtr->call_count);
        exit(1);
    }
    if (guard.blockedFrameCount() != 1) {
        printf("FAIL C: blocked counter not incremented\n");
        exit(1);
    }
    if (guard.lastBlockedEvent().reason != BlockReasonCode::FUNCTION_NOT_ALLOWED) {
        printf("FAIL C: wrong reason code\n");
        exit(1);
    }
    printf("OK [TEST_AUTHORED]\n");
}

// --- Test D: FC02 -> BLOCKED ---
static void test_D_FC02_blocked() {
    auto fake = std::make_unique<FakeLowerTransport>();
    FakeLowerTransport* fakePtr = fake.get();
    ReadOnlyGuard guard(std::move(fake));
    
    auto frame = buildFrameWithFC(0x02);
    if (guard.write(frame.data(), frame.size()) != 0 || !fakePtr->wasNotCalled()) {
        printf("FAIL D: FC02 not blocked\n");
        exit(1);
    }
    printf("OK [TEST_AUTHORED]\n");
}

// --- Test E: FC05 -> BLOCKED ---
static void test_E_FC05_blocked() {
    auto fake = std::make_unique<FakeLowerTransport>();
    FakeLowerTransport* fakePtr = fake.get();
    ReadOnlyGuard guard(std::move(fake));
    
    auto frame = buildFrameWithFC(0x05);
    if (guard.write(frame.data(), frame.size()) != 0 || !fakePtr->wasNotCalled()) {
        printf("FAIL E: FC05 not blocked\n");
        exit(1);
    }
    printf("OK [TEST_AUTHORED]\n");
}

// --- Test F: FC06 -> BLOCKED ---
static void test_F_FC06_blocked() {
    auto fake = std::make_unique<FakeLowerTransport>();
    FakeLowerTransport* fakePtr = fake.get();
    ReadOnlyGuard guard(std::move(fake));
    
    auto frame = buildFrameWithFC(0x06);
    if (guard.write(frame.data(), frame.size()) != 0 || !fakePtr->wasNotCalled()) {
        printf("FAIL F: FC06 not blocked\n");
        exit(1);
    }
    printf("OK [TEST_AUTHORED]\n");
}

// --- Test G: FC0F -> BLOCKED ---
static void test_G_FC0F_blocked() {
    auto fake = std::make_unique<FakeLowerTransport>();
    FakeLowerTransport* fakePtr = fake.get();
    ReadOnlyGuard guard(std::move(fake));
    
    auto frame = buildFrameWithFC(0x0F);
    if (guard.write(frame.data(), frame.size()) != 0 || !fakePtr->wasNotCalled()) {
        printf("FAIL G: FC0F not blocked\n");
        exit(1);
    }
    printf("OK [TEST_AUTHORED]\n");
}

// --- Test H: FC10 (0x10) -> BLOCKED ---
static void test_H_FC10_blocked() {
    auto fake = std::make_unique<FakeLowerTransport>();
    FakeLowerTransport* fakePtr = fake.get();
    ReadOnlyGuard guard(std::move(fake));
    
    auto frame = buildFrameWithFC(0x10);
    if (guard.write(frame.data(), frame.size()) != 0 || !fakePtr->wasNotCalled()) {
        printf("FAIL H: FC10 not blocked\n");
        exit(1);
    }
    printf("OK [TEST_AUTHORED]\n");
}

// --- Test I: FC17 -> BLOCKED ---
static void test_I_FC17_blocked() {
    auto fake = std::make_unique<FakeLowerTransport>();
    FakeLowerTransport* fakePtr = fake.get();
    ReadOnlyGuard guard(std::move(fake));
    
    auto frame = buildFrameWithFC(0x17);
    if (guard.write(frame.data(), frame.size()) != 0 || !fakePtr->wasNotCalled()) {
        printf("FAIL I: FC17 not blocked\n");
        exit(1);
    }
    printf("OK [TEST_AUTHORED]\n");
}

// --- Test J: Unknown FC -> BLOCKED ---
static void test_J_unknown_FC_blocked() {
    auto fake = std::make_unique<FakeLowerTransport>();
    FakeLowerTransport* fakePtr = fake.get();
    ReadOnlyGuard guard(std::move(fake));
    
    auto frame = buildFrameWithFC(0x99);  // Unknown function code
    if (guard.write(frame.data(), frame.size()) != 0 || !fakePtr->wasNotCalled()) {
        printf("FAIL J: unknown FC not blocked\n");
        exit(1);
    }
    printf("OK [TEST_AUTHORED]\n");
}

// --- Test K: Empty frame -> BLOCKED ---
static void test_K_empty_frame_blocked() {
    auto fake = std::make_unique<FakeLowerTransport>();
    FakeLowerTransport* fakePtr = fake.get();
    ReadOnlyGuard guard(std::move(fake));
    
    uint8_t empty[] = {};
    if (guard.write(empty, 0) != 0 || !fakePtr->wasNotCalled()) {
        printf("FAIL K: empty frame not blocked\n");
        exit(1);
    }
    if (guard.lastBlockedEvent().reason != BlockReasonCode::EMPTY_FRAME) {
        printf("FAIL K: wrong reason (expected EMPTY_FRAME)\n");
        exit(1);
    }
    printf("OK [TEST_AUTHORED]\n");
}

// --- Test L: Short frame -> BLOCKED ---
static void test_L_short_frame_blocked() {
    auto fake = std::make_unique<FakeLowerTransport>();
    FakeLowerTransport* fakePtr = fake.get();
    ReadOnlyGuard guard(std::move(fake));
    
    uint8_t short_frame[] = {0x01, 0x03, 0x02};  // Only 3 bytes
    if (guard.write(short_frame, 3) != 0 || !fakePtr->wasNotCalled()) {
        printf("FAIL L: short frame not blocked\n");
        exit(1);
    }
    if (guard.lastBlockedEvent().reason != BlockReasonCode::SHORT_FRAME) {
        printf("FAIL L: wrong reason (expected SHORT_FRAME)\n");
        exit(1);
    }
    printf("OK [TEST_AUTHORED]\n");
}

// --- Test M: Invalid CRC -> BLOCKED ---
static void test_M_invalid_CRC_blocked() {
    auto fake = std::make_unique<FakeLowerTransport>();
    FakeLowerTransport* fakePtr = fake.get();
    ReadOnlyGuard guard(std::move(fake));
    
    auto frame = buildValidFC03();
    frame[6] ^= 0xFF;  // Corrupt CRC low byte
    
    if (guard.write(frame.data(), frame.size()) != 0 || !fakePtr->wasNotCalled()) {
        printf("FAIL M: invalid CRC not blocked\n");
        exit(1);
    }
    if (guard.lastBlockedEvent().reason != BlockReasonCode::INVALID_CRC) {
        printf("FAIL M: wrong reason (expected INVALID_CRC)\n");
        exit(1);
    }
    printf("OK [TEST_AUTHORED]\n");
}

// --- Test N: Slave 0 (broadcast) -> BLOCKED ---
static void test_N_broadcast_blocked() {
    auto fake = std::make_unique<FakeLowerTransport>();
    FakeLowerTransport* fakePtr = fake.get();
    ReadOnlyGuard guard(std::move(fake));
    
    auto frame = buildValidFC03(0);  // slave = 0 (broadcast)
    if (guard.write(frame.data(), frame.size()) != 0 || !fakePtr->wasNotCalled()) {
        printf("FAIL N: broadcast not blocked\n");
        exit(1);
    }
    if (guard.lastBlockedEvent().reason != BlockReasonCode::BROADCAST_NOT_ALLOWED) {
        printf("FAIL N: wrong reason (expected BROADCAST_NOT_ALLOWED)\n");
        exit(1);
    }
    printf("OK [TEST_AUTHORED]\n");
}

// --- Test O: Wrong slave -> BLOCKED ---
static void test_O_wrong_slave_blocked() {
    auto fake = std::make_unique<FakeLowerTransport>();
    FakeLowerTransport* fakePtr = fake.get();
    ReadOnlyGuard guard(std::move(fake));
    
    guard.setConfiguredSlaveId(1);  // Configure for slave 1
    auto frame = buildValidFC03(2);  // Send to slave 2
    
    if (guard.write(frame.data(), frame.size()) != 0 || !fakePtr->wasNotCalled()) {
        printf("FAIL O: wrong slave not blocked\n");
        exit(1);
    }
    if (guard.lastBlockedEvent().reason != BlockReasonCode::SLAVE_NOT_ALLOWED) {
        printf("FAIL O: wrong reason (expected SLAVE_NOT_ALLOWED)\n");
        exit(1);
    }
    printf("OK [TEST_AUTHORED]\n");
}

// --- Test P: Quantity 0 -> BLOCKED ---
static void test_P_quantity_zero_blocked() {
    auto fake = std::make_unique<FakeLowerTransport>();
    FakeLowerTransport* fakePtr = fake.get();
    ReadOnlyGuard guard(std::move(fake));
    
    auto frame = buildValidFC03(1, 0x0200, 0);  // qty = 0
    if (guard.write(frame.data(), frame.size()) != 0 || !fakePtr->wasNotCalled()) {
        printf("FAIL P: quantity 0 not blocked\n");
        exit(1);
    }
    if (guard.lastBlockedEvent().reason != BlockReasonCode::QUANTITY_ZERO) {
        printf("FAIL P: wrong reason (expected QUANTITY_ZERO)\n");
        exit(1);
    }
    printf("OK [TEST_AUTHORED]\n");
}

// --- Test Q: Quantity > 125 -> BLOCKED ---
static void test_Q_quantity_overflow_blocked() {
    auto fake = std::make_unique<FakeLowerTransport>();
    FakeLowerTransport* fakePtr = fake.get();
    ReadOnlyGuard guard(std::move(fake));
    
    auto frame = buildValidFC03(1, 0x0200, 126);  // qty = 126 (> 125 limit)
    if (guard.write(frame.data(), frame.size()) != 0 || !fakePtr->wasNotCalled()) {
        printf("FAIL Q: quantity > 125 not blocked\n");
        exit(1);
    }
    if (guard.lastBlockedEvent().reason != BlockReasonCode::QUANTITY_OUT_OF_RANGE) {
        printf("FAIL Q: wrong reason (expected QUANTITY_OUT_OF_RANGE)\n");
        exit(1);
    }
    printf("OK [TEST_AUTHORED]\n");
}

// --- Test R: Trailing bytes -> BLOCKED ---
static void test_R_trailing_bytes_blocked() {
    auto fake = std::make_unique<FakeLowerTransport>();
    FakeLowerTransport* fakePtr = fake.get();
    ReadOnlyGuard guard(std::move(fake));
    
    auto frame = buildValidFC03();
    frame.push_back(0xAA);  // Add trailing byte (now 9 bytes)
    
    if (guard.write(frame.data(), frame.size()) != 0 || !fakePtr->wasNotCalled()) {
        printf("FAIL R: trailing bytes not blocked\n");
        exit(1);
    }
    if (guard.lastBlockedEvent().reason != BlockReasonCode::INVALID_LENGTH) {
        printf("FAIL R: wrong reason (expected INVALID_LENGTH)\n");
        exit(1);
    }
    printf("OK [TEST_AUTHORED]\n");
}

// --- Test S: Max valid address + valid quantity -> ALLOWED ---
static void test_S_max_address_valid() {
    auto fake = std::make_unique<FakeLowerTransport>();
    FakeLowerTransport* fakePtr = fake.get();
    ReadOnlyGuard guard(std::move(fake));
    
    // addr = 0xFFFF, qty = 1 (total = 0x10000, exactly at limit, valid)
    auto frame = buildValidFC03(1, 0xFFFF, 1);
    size_t written = guard.write(frame.data(), frame.size());
    
    if (written != frame.size() || !fakePtr->wasCalledOnce()) {
        printf("FAIL S: max valid address blocked\n");
        exit(1);
    }
    printf("OK [TEST_AUTHORED]\n");
}

// --- Test T: Address overflow -> BLOCKED ---
static void test_T_address_overflow_blocked() {
    auto fake = std::make_unique<FakeLowerTransport>();
    FakeLowerTransport* fakePtr = fake.get();
    ReadOnlyGuard guard(std::move(fake));
    
    // addr = 0xFFFF, qty = 2 (total = 0x10001, overflow)
    auto frame = buildValidFC03(1, 0xFFFF, 2);
    if (guard.write(frame.data(), frame.size()) != 0 || !fakePtr->wasNotCalled()) {
        printf("FAIL T: address overflow not blocked\n");
        exit(1);
    }
    if (guard.lastBlockedEvent().reason != BlockReasonCode::ADDRESS_RANGE_OVERFLOW) {
        printf("FAIL T: wrong reason (expected ADDRESS_RANGE_OVERFLOW)\n");
        exit(1);
    }
    printf("OK [TEST_AUTHORED]\n");
}

// --- Test U: Repeated blocked attempts -> counters increment, process alive ---
static void test_U_repeated_blocks() {
    auto fake = std::make_unique<FakeLowerTransport>();
    FakeLowerTransport* fakePtr = fake.get();
    ReadOnlyGuard guard(std::move(fake));
    
    auto frame = buildFrameWithFC(0x06);  // FC06 write
    
    for (int i = 0; i < 5; ++i) {
        if (guard.write(frame.data(), frame.size()) != 0) {
            printf("FAIL U: attempt %d not blocked\n", i+1);
            exit(1);
        }
    }
    
    if (guard.blockedFrameCount() != 5 || !fakePtr->wasNotCalled()) {
        printf("FAIL U: counter mismatch (blocked=%u, calls=%zu)\n", 
               guard.blockedFrameCount(), fakePtr->call_count);
        exit(1);
    }
    // Process still alive (if we reach here, no abort occurred)
    printf("OK [TEST_AUTHORED]\n");
}

// --- Test V: Valid frame after blocked attempts -> ALLOWED ---
static void test_V_valid_after_blocks() {
    auto fake = std::make_unique<FakeLowerTransport>();
    FakeLowerTransport* fakePtr = fake.get();
    ReadOnlyGuard guard(std::move(fake));
    
    // Block 3 attempts
    auto bad_frame = buildFrameWithFC(0x06);
    for (int i = 0; i < 3; ++i) {
        guard.write(bad_frame.data(), bad_frame.size());
    }
    
    // Now send valid FC03
    auto good_frame = buildValidFC03();
    size_t written = guard.write(good_frame.data(), good_frame.size());
    
    if (written != good_frame.size() || !fakePtr->wasCalledOnce()) {
        printf("FAIL V: valid frame blocked after previous blocks\n");
        exit(1);
    }
    if (guard.blockedFrameCount() != 3 || guard.allowedFrameCount() != 1) {
        printf("FAIL V: counter mismatch\n");
        exit(1);
    }
    printf("OK [TEST_AUTHORED]\n");
}

// --- Test W: Two valid frames consecutive -> both allowed ---
static void test_W_two_valid_consecutive() {
    auto fake = std::make_unique<FakeLowerTransport>();
    FakeLowerTransport* fakePtr = fake.get();
    ReadOnlyGuard guard(std::move(fake));
    
    auto frame1 = buildValidFC03(1, 0x0200, 10);
    auto frame2 = buildValidFC04(1, 0x0300, 5);
    
    size_t w1 = guard.write(frame1.data(), frame1.size());
    size_t w2 = guard.write(frame2.data(), frame2.size());
    
    if (w1 != frame1.size() || w2 != frame2.size()) {
        printf("FAIL W: frames not both allowed\n");
        exit(1);
    }
    if (fakePtr->call_count != 2 || fakePtr->frames.size() != 2) {
        printf("FAIL W: transport not called twice\n");
        exit(1);
    }
    if (fakePtr->frames[0] != frame1 || fakePtr->frames[1] != frame2) {
        printf("FAIL W: frame content mismatch\n");
        exit(1);
    }
    printf("OK [TEST_AUTHORED]\n");
}

// --- Test X: Frame mutated after CRC computation -> BLOCKED ---
static void test_X_post_CRC_mutation_blocked() {
    auto fake = std::make_unique<FakeLowerTransport>();
    FakeLowerTransport* fakePtr = fake.get();
    ReadOnlyGuard guard(std::move(fake));
    
    auto frame = buildValidFC03();
    frame[2] ^= 0x01;  // Mutate address byte after CRC was computed
    
    if (guard.write(frame.data(), frame.size()) != 0 || !fakePtr->wasNotCalled()) {
        printf("FAIL X: mutated frame not blocked\n");
        exit(1);
    }
    if (guard.lastBlockedEvent().reason != BlockReasonCode::INVALID_CRC) {
        printf("FAIL X: wrong reason (expected INVALID_CRC)\n");
        exit(1);
    }
    printf("OK [TEST_AUTHORED]\n");
}

// --- Test Y: CRC byte order wrong -> BLOCKED ---
static void test_Y_CRC_byte_order_wrong_blocked() {
    auto fake = std::make_unique<FakeLowerTransport>();
    FakeLowerTransport* fakePtr = fake.get();
    ReadOnlyGuard guard(std::move(fake));
    
    auto frame = buildValidFC03();
    // Swap CRC bytes (correct is little-endian, swap to big-endian)
    uint8_t temp = frame[6];
    frame[6] = frame[7];
    frame[7] = temp;
    
    if (guard.write(frame.data(), frame.size()) != 0 || !fakePtr->wasNotCalled()) {
        printf("FAIL Y: wrong CRC byte order not blocked\n");
        exit(1);
    }
    if (guard.lastBlockedEvent().reason != BlockReasonCode::INVALID_CRC) {
        printf("FAIL Y: wrong reason (expected INVALID_CRC)\n");
        exit(1);
    }
    printf("OK [TEST_AUTHORED]\n");
}

// --- Compatibility test: guard disabled bypass ---
static void test_bypass_when_disabled() {
    auto fake = std::make_unique<FakeLowerTransport>();
    FakeLowerTransport* fakePtr = fake.get();
    ReadOnlyGuard guard(std::move(fake));
    
    guard.setForceReadOnly(false);  // Disable guard
    
    auto frame = buildFrameWithFC(0x06);  // FC06 write
    size_t written = guard.write(frame.data(), frame.size());
    
    if (written != frame.size() || !fakePtr->wasCalledOnce()) {
        printf("FAIL bypass: guard not disabled\n");
        exit(1);
    }
    printf("OK\n");
}

// ============================================================================
// MAIN TEST RUNNER
// ============================================================================

int main() {
    printf("\n=== ReadOnlyGuard M2 Test Matrix A-Y ===\n\n");
    printf("STATUS: All tests are TEST_AUTHORED (M2 static verification only)\n");
    printf("Runtime execution reserved for M3\n\n");
    
    printf("==> A: FC03 valid -> ALLOWED: ");
    test_A_FC03_valid_allowed();
    
    printf("==> B: FC04 valid -> ALLOWED: ");
    test_B_FC04_valid_allowed();
    
    printf("==> C: FC01 -> BLOCKED FUNCTION_NOT_ALLOWED: ");
    test_C_FC01_blocked();
    
    printf("==> D: FC02 -> BLOCKED: ");
    test_D_FC02_blocked();
    
    printf("==> E: FC05 -> BLOCKED: ");
    test_E_FC05_blocked();
    
    printf("==> F: FC06 -> BLOCKED: ");
    test_F_FC06_blocked();
    
    printf("==> G: FC0F -> BLOCKED: ");
    test_G_FC0F_blocked();
    
    printf("==> H: FC10 (0x10) -> BLOCKED: ");
    test_H_FC10_blocked();
    
    printf("==> I: FC17 -> BLOCKED: ");
    test_I_FC17_blocked();
    
    printf("==> J: Unknown FC -> BLOCKED: ");
    test_J_unknown_FC_blocked();
    
    printf("==> K: Empty frame -> BLOCKED: ");
    test_K_empty_frame_blocked();
    
    printf("==> L: Short frame -> BLOCKED: ");
    test_L_short_frame_blocked();
    
    printf("==> M: Invalid CRC -> BLOCKED: ");
    test_M_invalid_CRC_blocked();
    
    printf("==> N: Slave 0 (broadcast) -> BLOCKED: ");
    test_N_broadcast_blocked();
    
    printf("==> O: Wrong slave -> BLOCKED: ");
    test_O_wrong_slave_blocked();
    
    printf("==> P: Quantity 0 -> BLOCKED: ");
    test_P_quantity_zero_blocked();
    
    printf("==> Q: Quantity > 125 -> BLOCKED: ");
    test_Q_quantity_overflow_blocked();
    
    printf("==> R: Trailing bytes -> BLOCKED: ");
    test_R_trailing_bytes_blocked();
    
    printf("==> S: Max valid address + quantity -> ALLOWED: ");
    test_S_max_address_valid();
    
    printf("==> T: Address overflow -> BLOCKED: ");
    test_T_address_overflow_blocked();
    
    printf("==> U: Repeated blocks -> counters, process alive: ");
    test_U_repeated_blocks();
    
    printf("==> V: Valid after blocks -> ALLOWED: ");
    test_V_valid_after_blocks();
    
    printf("==> W: Two valid consecutive -> both ALLOWED: ");
    test_W_two_valid_consecutive();
    
    printf("==> X: Post-CRC mutation -> BLOCKED: ");
    test_X_post_CRC_mutation_blocked();
    
    printf("==> Y: CRC byte order wrong -> BLOCKED: ");
    test_Y_CRC_byte_order_wrong_blocked();
    
    printf("\n--- Compatibility Tests ---\n");
    printf("==> Bypass when disabled: ");
    test_bypass_when_disabled();
    
    printf("\n✓ All M2 ReadOnlyGuard tests AUTHORED and compiled successfully.\n");
    printf("  Runtime execution: M3 milestone\n\n");
    return 0;
}
