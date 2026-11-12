// SPDX-License-Identifier: MIT
// Milestone M12: Approved Target Waveshare ESP32-S3-Relay-1CH-U HAL
// Verification Test

#include <unity.h>

#include "boards/waveshare_esp32_s3_relay_1ch.h"
#include "protocols/modbus/modbus_rtu.h"
#include "support/mock_transport.h"
#include "transport/readonly_guard.h"
#include "transport/serial_profile.h"

using namespace heliograph;
using heliograph::test::MockTransport;

void setUp() {}
void tearDown() {}

void test_waveshare_approved_target_identity() {
  TEST_ASSERT_EQUAL_STRING("Waveshare ESP32-S3-Relay-1CH", board::kName);
  TEST_ASSERT_EQUAL_STRING("relay-1ch", board::kId);

  // Official schematic pinout verification (pi131M31 isolator + SP3485EN on
  // ESP32-S3-WROOM-1U)
  TEST_ASSERT_EQUAL_INT32(17, board::kRs485Tx);
  TEST_ASSERT_EQUAL_INT32(18, board::kRs485Rx);
  TEST_ASSERT_EQUAL_INT32(21, board::kRs485De);
  TEST_ASSERT_EQUAL_INT32(1, board::kRs485UartNum);

  // Relay hardware presence and zero-actuation safety invariant
  TEST_ASSERT_EQUAL_INT32(1, board::kRelayCount);
  TEST_ASSERT_EQUAL_INT32(47, board::kRelayPins[0]);
  TEST_ASSERT_EQUAL_STRING("VERIFIED_WAVESHARE_RELAY_1CH_U",
                           BOARD_CONFIGURATION_STATUS);
}

void test_waveshare_serial_profile_defaults() {
  SerialProfile profile;
  profile.baudRate = 9600;
  profile.dataBits = 8;
  profile.parity = SerialParity::None;
  profile.stopBits = 1;

  TEST_ASSERT_EQUAL_UINT32(9600, profile.baudRate);
  TEST_ASSERT_EQUAL_UINT8(8, profile.dataBits);
  TEST_ASSERT_TRUE(profile.parity == SerialParity::None);
  TEST_ASSERT_EQUAL_UINT8(1, profile.stopBits);
}

void test_waveshare_readonly_guard_enforcement() {
  auto mockTransport = std::make_unique<MockTransport>();
  MockTransport *rawMock = mockTransport.get();
  ReadOnlyGuard guard(std::move(mockTransport), 1);

  // Attempting write must be blocked by ReadOnlyGuard on this HAL
  const uint8_t writeFrame[] = {0x01, 0x06, 0x02, 0x00, 0x00, 0x64, 0x89, 0x99};
  size_t written = guard.write(writeFrame, sizeof(writeFrame));
  TEST_ASSERT_EQUAL_UINT32(0, written);
  TEST_ASSERT_EQUAL_UINT32(0, rawMock->writes.size());
  TEST_ASSERT_EQUAL_UINT32(1, guard.blockedFrameCount());

  // Valid read request built via Modbus RTU codec must pass through
  uint8_t readFrame[16];
  size_t frameLen = 0;
  modbus::BuildResult res =
      modbus::buildReadRequest(1, modbus::kReadHoldingRegisters, 0x0200, 10,
                               readFrame, sizeof(readFrame), frameLen);
  TEST_ASSERT_TRUE(res == modbus::BuildResult::Ok);
  TEST_ASSERT_EQUAL_UINT32(8, frameLen);

  written = guard.write(readFrame, frameLen);
  TEST_ASSERT_EQUAL_UINT32(frameLen, written);
  TEST_ASSERT_EQUAL_UINT32(1, rawMock->writes.size());
  TEST_ASSERT_EQUAL_UINT32(frameLen, rawMock->writes[0].size());
  TEST_ASSERT_EQUAL_UINT32(1, guard.allowedFrameCount());
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_waveshare_approved_target_identity);
  RUN_TEST(test_waveshare_serial_profile_defaults);
  RUN_TEST(test_waveshare_readonly_guard_enforcement);
  return UNITY_END();
}
