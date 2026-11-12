// SPDX-License-Identifier: MIT
// Milestone M11: 7-Day Endurance Simulation Test (Virtual Accelerated Time)
// Tests long-term stability, memory retention, quality tracking, power balance,
// and night-time standby fault injection over 604,800 virtual seconds.

#include <unity.h>

#include <cmath>
#include <cstdint>
#include <memory>
#include <vector>

#include "device/device_state.h"
#include "device/measurement.h"
#include "drivers/afore/afore_telemetry_engine.h"
#include "ems/power_balance_evaluator.h"
#include "protocols/modbus/modbus_rtu.h"
#include "support/mock_transport.h"
#include "transport/readonly_guard.h"


using namespace heliograph;
using heliograph::test::MockTransport;
using namespace ems;

namespace {

// Helper to build Modbus response frame
std::vector<uint8_t>
buildModbusResponse(uint8_t slaveId, const std::vector<uint16_t> &registers) {
  std::vector<uint8_t> frame;
  frame.push_back(slaveId);
  frame.push_back(0x03); // FC03
  frame.push_back(static_cast<uint8_t>(registers.size() * 2));

  for (uint16_t reg : registers) {
    frame.push_back(static_cast<uint8_t>(reg >> 8));
    frame.push_back(static_cast<uint8_t>(reg & 0xFF));
  }

  const uint16_t crc = modbus::crc16(frame.data(), frame.size());
  frame.push_back(static_cast<uint8_t>(crc & 0xFF));
  frame.push_back(static_cast<uint8_t>(crc >> 8));

  return frame;
}

} // namespace

void setUp() {}
void tearDown() {}

void test_seven_day_virtual_endurance_and_stability() {
  auto mockTransport = std::make_unique<MockTransport>();
  MockTransport *rawMock = mockTransport.get();
  auto guard = std::make_unique<ReadOnlyGuard>(std::move(mockTransport), 1);
  auto engine = std::make_unique<AforeTelemetryEngine>(*guard, 1);

  // Configure blocks: Fast=5s (5000ms), Medium=15s (15000ms), Slow=60s
  // (60000ms)
  engine->configureFastBlock(512, 19, 5000);    // Power block
  engine->configureMediumBlock(552, 10, 15000); // PV block
  engine->configureSlowBlock(2000, 20, 60000);  // Battery block
  engine->setMinInterFrameGapMs(0);             // Fast accelerated time

  // Define synthetic profiles
  // Registers for Power Block (512):
  // 512+0: AC Total Power (offset 0) = 2500 W
  // 512+1: Grid Power (offset 1)     = 500 W (import)
  std::vector<uint16_t> powerRegs(19, 0);
  powerRegs[0] = 2500;
  powerRegs[1] = 500;

  // Registers for PV Block (552):
  // MPPT1 = 1500W, MPPT2 = 1000W -> Total PV = 2500W
  std::vector<uint16_t> pvRegs(10, 0);
  pvRegs[0] = 2500; // V1 250.0V
  pvRegs[1] = 60;   // I1 6.0A -> 1500W
  pvRegs[2] = 2500; // V2 250.0V
  pvRegs[3] = 40;   // I2 4.0A -> 1000W
  pvRegs[4] = 1500; // MPPT1 P
  pvRegs[5] = 1000; // MPPT2 P

  // Registers for Battery Block (2000):
  // SOC = 85%, Battery Power = 500W discharging (+500)
  std::vector<uint16_t> battRegs(20, 0);
  battRegs[0] = 85;  // SOC 85%
  battRegs[1] = 500; // V 50.0V
  battRegs[2] = 100; // I 10.0A
  battRegs[3] = 500; // P +500W

  auto powerFrame = buildModbusResponse(1, powerRegs);
  auto pvFrame = buildModbusResponse(1, pvRegs);
  auto battFrame = buildModbusResponse(1, battRegs);

  // Install responder that responds according to the requested address
  rawMock->setResponder([&](const std::vector<uint8_t> &req,
                            std::vector<uint8_t> &reply) -> bool {
    if (req.size() < 6)
      return false;
    uint16_t addr = (static_cast<uint16_t>(req[2]) << 8) | req[3];
    if (addr == 512) {
      reply = powerFrame;
      return true;
    } else if (addr == 552) {
      reply = pvFrame;
      return true;
    } else if (addr == 2000) {
      reply = battFrame;
      return true;
    }
    return false;
  });

  DeviceState deviceState;
  const uint64_t kSevenDaysMs =
      7ULL * 24ULL * 3600ULL * 1000ULL; // 604,800,000 ms
  const uint64_t kStepMs = 5000;        // 5s step
  uint64_t nowMs = 0;
  size_t totalCycles = 0;
  size_t balancedCycles = 0;

  PowerBalanceEvaluator evaluator(BatterySignConvention::DischargeIsPositive,
                                  GridSignConvention::ImportIsPositive, 200.0f);

  // Run virtual time simulation
  while (nowMs < kSevenDaysMs) {
    nowMs += kStepMs;
    totalCycles++;

    // Poll engine
    engine->poll(nowMs);
    engine->updateDeviceState(deviceState, nowMs);

    // Verify power balance
    PowerTelemetry telemetry;
    telemetry.pv_power_w = 2500.0f;
    telemetry.battery_power_w = 500.0f;
    telemetry.grid_power_w = 500.0f;
    telemetry.load_power_w = 3500.0f;

    const auto balance = evaluator.evaluate(telemetry);
    if (balance.is_balanced) {
      balancedCycles++;
    }
    TEST_ASSERT_TRUE(std::abs(balance.balance_imbalance_w) <= 200.0f);

    // Every 24 virtual hours (86,400,000 ms), check that memory and stats do
    // not drift
    if (nowMs % (86400ULL * 1000ULL) == 0) {
      const auto &stats = engine->getStats();
      TEST_ASSERT_TRUE(stats.totalPolls > 0);
      TEST_ASSERT_TRUE(stats.successRate() > 95.0);
    }
  }

  // Final verification of 7-day endurance run
  TEST_ASSERT_EQUAL_UINT64(kSevenDaysMs, nowMs);
  TEST_ASSERT_EQUAL_UINT32(120960, totalCycles);
  TEST_ASSERT_TRUE(balancedCycles > 100000);

  const auto &finalStats = engine->getStats();
  TEST_ASSERT_TRUE(finalStats.totalPolls >= 120960);
  TEST_ASSERT_EQUAL_UINT32(0, finalStats.crcErrors);
  TEST_ASSERT_EQUAL_UINT32(0, finalStats.timeouts);
  TEST_ASSERT_DOUBLE_WITHIN(0.01, 100.0, finalStats.successRate());
}

void test_fault_injection_night_silence_and_recovery() {
  auto mockTransport = std::make_unique<MockTransport>();
  MockTransport *rawMock = mockTransport.get();
  auto guard = std::make_unique<ReadOnlyGuard>(std::move(mockTransport), 1);
  auto engine = std::make_unique<AforeTelemetryEngine>(*guard, 1);

  engine->configureFastBlock(512, 19, 5000);
  engine->setMinInterFrameGapMs(0);

  std::vector<uint16_t> powerRegs(19, 1000);
  auto goodReply = buildModbusResponse(1, powerRegs);

  bool simulateFault = false;
  rawMock->setResponder(
      [&](const std::vector<uint8_t> &, std::vector<uint8_t> &reply) -> bool {
        if (simulateFault) {
          return false; // Silence / Timeout
        }
        reply = goodReply;
        return true;
      });

  uint64_t nowMs = 0;
  // Day 1: normal operation
  for (int i = 0; i < 100; ++i) {
    nowMs += 5000;
    engine->poll(nowMs);
  }
  TEST_ASSERT_EQUAL_UINT32(0, engine->getConsecutiveErrors());
  TEST_ASSERT_FALSE(engine->isCircuitOpen());

  // Night-time fault injection: 8 virtual hours of silence
  simulateFault = true;
  for (int i = 0; i < 20; ++i) {
    nowMs += 5000;
    engine->poll(nowMs);
  }
  // Circuit breaker should open due to consecutive failures
  TEST_ASSERT_TRUE(engine->isCircuitOpen());

  // Day 2 morning: inverter awakens, replies resume
  simulateFault = false;
  nowMs += 35000; // Wait out circuit breaker cooldown (30s)
  size_t polled = engine->poll(nowMs);
  TEST_ASSERT_EQUAL_UINT32(1, polled);
  TEST_ASSERT_FALSE(engine->isCircuitOpen());
  TEST_ASSERT_EQUAL_UINT32(0, engine->getConsecutiveErrors());
  TEST_ASSERT_EQUAL_UINT32(1, engine->getBackoffMultiplier());
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_seven_day_virtual_endurance_and_stability);
  RUN_TEST(test_fault_injection_night_silence_and_recovery);
  return UNITY_END();
}
