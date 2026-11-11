// SPDX-License-Identifier: MIT
// M3A Safety Core Test - ReadOnlyGuard Offline Validation

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <vector>

#include "fake_lower_transport.h"
#include "../src/transport/readonly_guard.h"
#include "../src/protocols/modbus/modbus_rtu.h"

using namespace heliograph;

// Minimal test harness
static int tests_executed = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define CHECK_TRUE(cond, msg) \
    do { \
        ++tests_executed; \
        if (cond) { \
            ++tests_passed; \
        } else { \
            ++tests_failed; \
            printf("FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); \
        } \
    } while(0)

#define CHECK_FALSE(cond, msg) CHECK_TRUE(!(cond), msg)
#define CHECK_EQ(a, b, msg) CHECK_TRUE((a) == (b), msg)

#define CHECK_BYTES_EQ(actual_bytes, expected_bytes, msg) \
    CHECK_TRUE((actual_bytes) == (expected_bytes), msg)

// Helper to build valid Modbus RTU frames with correct CRC
static std::vector<uint8_t> buildFrame(uint8_t slave, uint8_t fc, 
                                       uint16_t addr, uint16_t qty) {
    std::vector<uint8_t> frame(6);
    frame[0] = slave;
    frame[1] = fc;
    frame[2] = static_cast<uint8_t>((addr >> 8) & 0xFF);
    frame[3] = static_cast<uint8_t>(addr & 0xFF);
    frame[4] = static_cast<uint8_t>((qty >> 8) & 0xFF);
    frame[5] = static_cast<uint8_t>(qty & 0xFF);
    
    // Append CRC using production function
    uint16_t crc = modbus::crc16(frame.data(), frame.size());
    frame.push_back(static_cast<uint8_t>(crc & 0xFF));
    frame.push_back(static_cast<uint8_t>((crc >> 8) & 0xFF));
    
    return frame;
}

int main() {
    printf("M3A Safety Core Tests - ReadOnlyGuard Offline Validation\n");
    printf("==========================================================\n\n");

    // ALLOWED FRAMES
    {
        printf("Testing ALLOWED frames (FC03/FC04 valid)...\n");
        auto fake = std::make_unique<FakeLowerTransport>();
        auto* fake_ptr = fake.get();
        ReadOnlyGuard guard(std::move(fake), 1);

        // Test 1: FC03 valid, slave 1, quantity 1
        {
            fake_ptr->reset();
            auto frame = buildFrame(1, 0x03, 0x0200, 1);
            size_t written = guard.write(frame.data(), frame.size());
            CHECK_EQ(written, 8u, "FC03 qty=1 allowed");
            CHECK_EQ(fake_ptr->call_count(), 1u, "FC03 qty=1 transmitted");
            CHECK_BYTES_EQ(fake_ptr->total_bytes(), 8u, "FC03 qty=1 bytes");
        }

        // Test 2: FC04 valid, slave 1, quantity 1
        {
            fake_ptr->reset();
            auto frame = buildFrame(1, 0x04, 0x0200, 1);
            size_t written = guard.write(frame.data(), frame.size());
            CHECK_EQ(written, 8u, "FC04 qty=1 allowed");
            CHECK_EQ(fake_ptr->call_count(), 1u, "FC04 qty=1 transmitted");
            CHECK_BYTES_EQ(fake_ptr->total_bytes(), 8u, "FC04 qty=1 bytes");
        }

        // Test 3: FC04 valid, quantity 125 (max)
        {
            fake_ptr->reset();
            auto frame = buildFrame(1, 0x04, 0x0000, 125);
            size_t written = guard.write(frame.data(), frame.size());
            CHECK_EQ(written, 8u, "FC04 qty=125 allowed");
            CHECK_EQ(fake_ptr->call_count(), 1u, "FC04 qty=125 transmitted");
            CHECK_BYTES_EQ(fake_ptr->total_bytes(), 8u, "FC04 qty=125 bytes");
        }

        // Test 4: High address without overflow
        {
            fake_ptr->reset();
            auto frame = buildFrame(1, 0x03, 0xFFFF, 1);
            size_t written = guard.write(frame.data(), frame.size());
            CHECK_EQ(written, 8u, "FC03 addr=0xFFFF qty=1 allowed");
            CHECK_EQ(fake_ptr->call_count(), 1u, "High addr transmitted");
            CHECK_BYTES_EQ(fake_ptr->total_bytes(), 8u, "High addr bytes");
        }
    }

    // BLOCKED FRAMES
    {
        printf("\nTesting BLOCKED frames...\n");
        auto fake = std::make_unique<FakeLowerTransport>();
        auto* fake_ptr = fake.get();
        ReadOnlyGuard guard(std::move(fake), 1);

        // Test 5: Empty frame
        {
            fake_ptr->reset();
            size_t written = guard.write(nullptr, 0);
            CHECK_EQ(written, 0u, "Empty frame blocked");
            CHECK_EQ(fake_ptr->call_count(), 0u, "Empty frame not transmitted");
            CHECK_BYTES_EQ(fake_ptr->total_bytes(), 0u, "Empty frame 0 bytes");
        }

        // Test 6: Short frame
        {
            fake_ptr->reset();
            uint8_t short_frame[] = {0x01, 0x03, 0x00};
            size_t written = guard.write(short_frame, sizeof(short_frame));
            CHECK_EQ(written, 0u, "Short frame blocked");
            CHECK_EQ(fake_ptr->call_count(), 0u, "Short frame not transmitted");
            CHECK_BYTES_EQ(fake_ptr->total_bytes(), 0u, "Short frame 0 bytes");
        }

        // Test 7: Long frame / trailing bytes
        {
            fake_ptr->reset();
            auto frame = buildFrame(1, 0x03, 0x0200, 1);
            frame.push_back(0xFF);  // Extra byte
            size_t written = guard.write(frame.data(), frame.size());
            CHECK_EQ(written, 0u, "Long frame blocked");
            CHECK_EQ(fake_ptr->call_count(), 0u, "Long frame not transmitted");
            CHECK_BYTES_EQ(fake_ptr->total_bytes(), 0u, "Long frame 0 bytes");
        }

        // Test 8: Bad CRC
        {
            fake_ptr->reset();
            auto frame = buildFrame(1, 0x03, 0x0200, 1);
            frame[6] ^= 0xFF;  // Corrupt CRC
            size_t written = guard.write(frame.data(), frame.size());
            CHECK_EQ(written, 0u, "Bad CRC blocked");
            CHECK_EQ(fake_ptr->call_count(), 0u, "Bad CRC not transmitted");
            CHECK_BYTES_EQ(fake_ptr->total_bytes(), 0u, "Bad CRC 0 bytes");
        }

        // Test 9: Swapped CRC bytes
        {
            fake_ptr->reset();
            auto frame = buildFrame(1, 0x03, 0x0200, 1);
            std::swap(frame[6], frame[7]);
            size_t written = guard.write(frame.data(), frame.size());
            CHECK_EQ(written, 0u, "Swapped CRC blocked");
            CHECK_EQ(fake_ptr->call_count(), 0u, "Swapped CRC not transmitted");
            CHECK_BYTES_EQ(fake_ptr->total_bytes(), 0u, "Swapped CRC 0 bytes");
        }

        // Test 10: Broadcast (slave 0)
        {
            fake_ptr->reset();
            auto frame = buildFrame(0, 0x03, 0x0200, 1);
            size_t written = guard.write(frame.data(), frame.size());
            CHECK_EQ(written, 0u, "Broadcast blocked");
            CHECK_EQ(fake_ptr->call_count(), 0u, "Broadcast not transmitted");
            CHECK_BYTES_EQ(fake_ptr->total_bytes(), 0u, "Broadcast 0 bytes");
        }

        // Test 11: Wrong slave (slave 2)
        {
            fake_ptr->reset();
            auto frame = buildFrame(2, 0x03, 0x0200, 1);
            size_t written = guard.write(frame.data(), frame.size());
            CHECK_EQ(written, 0u, "Wrong slave blocked");
            CHECK_EQ(fake_ptr->call_count(), 0u, "Wrong slave not transmitted");
            CHECK_BYTES_EQ(fake_ptr->total_bytes(), 0u, "Wrong slave 0 bytes");
        }

        // Test 12: FC01 (Read Coils)
        {
            fake_ptr->reset();
            auto frame = buildFrame(1, 0x01, 0x0200, 1);
            size_t written = guard.write(frame.data(), frame.size());
            CHECK_EQ(written, 0u, "FC01 blocked");
            CHECK_EQ(fake_ptr->call_count(), 0u, "FC01 not transmitted");
            CHECK_BYTES_EQ(fake_ptr->total_bytes(), 0u, "FC01 0 bytes");
        }

        // Test 13: FC02 (Read Discrete Inputs)
        {
            fake_ptr->reset();
            auto frame = buildFrame(1, 0x02, 0x0200, 1);
            size_t written = guard.write(frame.data(), frame.size());
            CHECK_EQ(written, 0u, "FC02 blocked");
            CHECK_EQ(fake_ptr->call_count(), 0u, "FC02 not transmitted");
            CHECK_BYTES_EQ(fake_ptr->total_bytes(), 0u, "FC02 0 bytes");
        }

        // Test 14: FC05 (Write Single Coil)
        {
            fake_ptr->reset();
            auto frame = buildFrame(1, 0x05, 0x0200, 1);
            size_t written = guard.write(frame.data(), frame.size());
            CHECK_EQ(written, 0u, "FC05 blocked");
            CHECK_EQ(fake_ptr->call_count(), 0u, "FC05 not transmitted");
            CHECK_BYTES_EQ(fake_ptr->total_bytes(), 0u, "FC05 0 bytes");
        }

        // Test 15: FC06 (Write Single Register)
        {
            fake_ptr->reset();
            auto frame = buildFrame(1, 0x06, 0x0200, 1);
            size_t written = guard.write(frame.data(), frame.size());
            CHECK_EQ(written, 0u, "FC06 blocked");
            CHECK_EQ(fake_ptr->call_count(), 0u, "FC06 not transmitted");
            CHECK_BYTES_EQ(fake_ptr->total_bytes(), 0u, "FC06 0 bytes");
        }

        // Test 16: FC0F (Write Multiple Coils)
        {
            fake_ptr->reset();
            auto frame = buildFrame(1, 0x0F, 0x0200, 1);
            size_t written = guard.write(frame.data(), frame.size());
            CHECK_EQ(written, 0u, "FC0F blocked");
            CHECK_EQ(fake_ptr->call_count(), 0u, "FC0F not transmitted");
            CHECK_BYTES_EQ(fake_ptr->total_bytes(), 0u, "FC0F 0 bytes");
        }

        // Test 17: FC10 (Write Multiple Registers)
        {
            fake_ptr->reset();
            auto frame = buildFrame(1, 0x10, 0x0200, 1);
            size_t written = guard.write(frame.data(), frame.size());
            CHECK_EQ(written, 0u, "FC10 blocked");
            CHECK_EQ(fake_ptr->call_count(), 0u, "FC10 not transmitted");
            CHECK_BYTES_EQ(fake_ptr->total_bytes(), 0u, "FC10 0 bytes");
        }

        // Test 18: FC17 (Read/Write Multiple Registers)
        {
            fake_ptr->reset();
            auto frame = buildFrame(1, 0x17, 0x0200, 1);
            size_t written = guard.write(frame.data(), frame.size());
            CHECK_EQ(written, 0u, "FC17 blocked");
            CHECK_EQ(fake_ptr->call_count(), 0u, "FC17 not transmitted");
            CHECK_BYTES_EQ(fake_ptr->total_bytes(), 0u, "FC17 0 bytes");
        }

        // Test 19: Unknown function code
        {
            fake_ptr->reset();
            auto frame = buildFrame(1, 0x99, 0x0200, 1);
            size_t written = guard.write(frame.data(), frame.size());
            CHECK_EQ(written, 0u, "FC99 blocked");
            CHECK_EQ(fake_ptr->call_count(), 0u, "FC99 not transmitted");
            CHECK_BYTES_EQ(fake_ptr->total_bytes(), 0u, "FC99 0 bytes");
        }

        // Test 20: Quantity zero
        {
            fake_ptr->reset();
            auto frame = buildFrame(1, 0x03, 0x0200, 0);
            size_t written = guard.write(frame.data(), frame.size());
            CHECK_EQ(written, 0u, "Qty=0 blocked");
            CHECK_EQ(fake_ptr->call_count(), 0u, "Qty=0 not transmitted");
            CHECK_BYTES_EQ(fake_ptr->total_bytes(), 0u, "Qty=0 0 bytes");
        }

        // Test 21: Quantity 126 (over limit)
        {
            fake_ptr->reset();
            auto frame = buildFrame(1, 0x03, 0x0200, 126);
            size_t written = guard.write(frame.data(), frame.size());
            CHECK_EQ(written, 0u, "Qty=126 blocked");
            CHECK_EQ(fake_ptr->call_count(), 0u, "Qty=126 not transmitted");
            CHECK_BYTES_EQ(fake_ptr->total_bytes(), 0u, "Qty=126 0 bytes");
        }

        // Test 22: Address overflow
        {
            fake_ptr->reset();
            auto frame = buildFrame(1, 0x03, 0xFFFF, 2);
            size_t written = guard.write(frame.data(), frame.size());
            CHECK_EQ(written, 0u, "Overflow blocked");
            CHECK_EQ(fake_ptr->call_count(), 0u, "Overflow not transmitted");
            CHECK_BYTES_EQ(fake_ptr->total_bytes(), 0u, "Overflow 0 bytes");
        }

        // Test 23: Frame modified after CRC computation
        {
            fake_ptr->reset();
            auto frame = buildFrame(1, 0x03, 0x0200, 1);
            frame[1] = 0x06;  // Change FC after CRC computed
            size_t written = guard.write(frame.data(), frame.size());
            CHECK_EQ(written, 0u, "Modified frame blocked");
            CHECK_EQ(fake_ptr->call_count(), 0u, "Modified frame not transmitted");
            CHECK_BYTES_EQ(fake_ptr->total_bytes(), 0u, "Modified frame 0 bytes");
        }

        // Test 24: Multiple blocks increment counter
        {
            fake_ptr->reset();
            uint32_t count_before = guard.blockedFrameCount();
            auto frame = buildFrame(1, 0x06, 0x0200, 1);
            guard.write(frame.data(), frame.size());
            guard.write(frame.data(), frame.size());
            guard.write(frame.data(), frame.size());
            CHECK_EQ(guard.blockedFrameCount(), count_before + 3u, "Blocked counter increments");
            CHECK_EQ(fake_ptr->call_count(), 0u, "Multiple blocks 0 transmissions");
            CHECK_BYTES_EQ(fake_ptr->total_bytes(), 0u, "Multiple blocks 0 bytes");
        }

        // Test 25: After blocks, valid frame still passes
        {
            fake_ptr->reset();
            auto bad = buildFrame(1, 0x06, 0x0200, 1);
            guard.write(bad.data(), bad.size());
            fake_ptr->reset();
            auto good = buildFrame(1, 0x04, 0x0200, 1);
            size_t written = guard.write(good.data(), good.size());
            CHECK_EQ(written, 8u, "After block, valid passes");
            CHECK_EQ(fake_ptr->call_count(), 1u, "After block, valid transmitted");
            CHECK_BYTES_EQ(fake_ptr->total_bytes(), 8u, "After block, valid bytes");
        }

        // Test 26: configuredSlaveId returns 1
        {
            CHECK_EQ(guard.configuredSlaveId(), 1u, "configuredSlaveId() == 1");
        }
    }

    printf("\n==========================================================\n");
    printf("M3A tests executed: %d\n", tests_executed);
    printf("M3A tests passed: %d\n", tests_passed);
    printf("M3A tests failed: %d\n", tests_failed);
    
    return tests_failed == 0 ? 0 : 1;
}
