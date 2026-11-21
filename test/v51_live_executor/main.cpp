// SPDX-License-Identifier: MIT
#include "v51/live_executor.h"
#include "v51/trainer_response.h"
#include "drivers/afore/external_ems_actuator.h"

#include <cassert>

using namespace heliograph::v51;

struct Actuator final : LiveActuator {
    bool ownsControl = true;
    bool releaseOk = true;
    int readbackOffset = 0;
    int writes = 0;
    int releases = 0;

    bool hasControl() const override { return ownsControl; }
    std::optional<int> writeAndReadBack(int watts) override {
        ++writes;
        return watts + readbackOffset;
    }
    bool release() override {
        ++releases;
        return releaseOk;
    }
};

struct Bus final : heliograph::afore::ExternalEmsBus {
    uint16_t mode = 5, controlType = 0;
    uint16_t high = 0, low = 0;
    bool mismatch = false, owner = true;
    int writes = 0, failPowerReads = 0;

    bool ownsBus() const override { return owner; }
    bool readHolding(uint8_t unit, uint16_t address, uint16_t* words, uint16_t count) override {
        if (!owner || unit != 1) return false;
        if (address == 0x09C4 && count == 1) words[0] = mode;
        else if (address == 0x0A08 && count == 1) words[0] = controlType;
        else if (address == 0x0A09 && count == 2) {
            if (failPowerReads > 0) { --failPowerReads; return false; }
            words[0] = high;
            words[1] = low ^ unsigned(mismatch);
        } else return false;
        return true;
    }
    bool writeFrame(const uint8_t* frame, size_t size) override {
        if (!owner || frame[0] != 1 || frame[1] != 0x10) return false;
        ++writes;
        if (size == 13 && frame[2] == 0x0A && frame[3] == 0x09) {
            high = (uint16_t(frame[7]) << 8) | frame[8];
            low = (uint16_t(frame[9]) << 8) | frame[10];
        } else if (size == 11 && frame[2] == 0x09 && frame[3] == 0xC4) {
            mode = (uint16_t(frame[7]) << 8) | frame[8];
        } else return false;
        return true;
    }
};

int main() {
    TrainerResponse response;
    using Response = TrainerResponse::Result;
    response.confirmed(200, 1000);
    assert(response.observe(200, 2000, 2000) == Response::Waiting);
    assert(response.observe(180, 4000, 4000) == Response::Matched);
    assert(response.observe(-180, 5000, 5000) == Response::Waiting);
    assert(response.observe(-180, 15000, 15000) == Response::Mismatch);

    // A new setpoint resets the mismatch deadline so rapid updates still get full settle time.
    TrainerResponse rapid;
    rapid.confirmed(156, 1000);
    assert(rapid.observe(1, 3000, 3000) == Response::Waiting);
    rapid.confirmed(200, 9000);
    assert(rapid.observe(202, 11000, 11000) == Response::Waiting);
    assert(rapid.observe(202, 12000, 12000) == Response::Matched);

    // High-power Trainer envelope scales to 15% of setpoint while keeping >= 60 W floor.
    TrainerResponse highPower;
    highPower.confirmed(2000, 1000);
    assert(highPower.observe(1850, 4000, 4000) == Response::Matched);
    assert(highPower.observe(-2000, 5000, 5000) == Response::Waiting);
    assert(highPower.observe(-2000, 15000, 15000) == Response::Mismatch);

    // Changing setpoints every 2s with a stuck battery must still trip within 20s.
    TrainerResponse stuck;
    stuck.confirmed(200, 1000);
    for (uint32_t t = 3000; t <= 19000; t += 2000) {
        assert(stuck.observe(0, t, t) == Response::Waiting);
        stuck.confirmed(200 + int(t / 100), t);
    }
    assert(stuck.observe(0, 21000, 21000) == Response::Mismatch);
    assert(std::string_view(stuck.diagnostic().deadline) == "VERIFICATION_GAP_TIMEOUT");
    assert(std::string_view(stuck.diagnostic().sample) == "NO_SETTLED_SAMPLE");
    assert(std::string_view(response.diagnostic().deadline) == "STABLE_COMMAND_TIMEOUT");
    assert(std::string_view(response.diagnostic().sample) == "OUTSIDE_TOLERANCE");

    Bus takeover;
    takeover.mode = 1;
    heliograph::afore::ExternalEmsActuator session(takeover, 1, 1, false);
    takeover.controlType = 1;
    assert(!session.enterExternalBatteryControl() && takeover.writes == 0);
    takeover.controlType = 0;
    assert(session.enterExternalBatteryControl() && takeover.mode == 5);
    assert(session.release() && takeover.mode == 1 && takeover.writes == 2);

    // Byte-for-byte command examples from the device protocol specification.
    const std::array<uint8_t, 13> positive{
        0x01, 0x10, 0x0A, 0x09, 0x00, 0x02, 0x04, 0x00, 0x00, 0x07, 0xD0, 0x4E, 0xC9};
    const std::array<uint8_t, 13> negative{
        0x01, 0x10, 0x0A, 0x09, 0x00, 0x02, 0x04, 0xFF, 0xFF, 0xF8, 0x30, 0x0E, 0x95};
    assert(heliograph::afore::externalPowerFrame(1, 2000) == positive);
    assert(heliograph::afore::externalPowerFrame(1, -2000) == negative);
    assert(!heliograph::afore::externalPowerFrame(0, 0));
    assert(!heliograph::afore::externalPowerFrame(248, 0));

    Bus bus;
    heliograph::afore::ExternalEmsActuator direct(bus, 1, 1, false);
    heliograph::afore::ExternalEmsActuator inverted(bus, 1, 1, true);
    assert(direct.writeAndReadBack(-2000) == -2000);
    assert(bus.high == 0xFFFF && bus.low == 0xF830);
    assert(inverted.writeAndReadBack(-2000) == -2000);
    assert(bus.high == 0 && bus.low == 2000);
    assert(inverted.writeAndReadBack(2000) == 2000);
    assert(bus.high == 0xFFFF && bus.low == 0xF830);
    assert(inverted.writeAndReadBack(0) == 0 && bus.high == 0 && bus.low == 0);
    assert(!inverted.writeAndReadBack(INT32_MIN) && bus.writes == 4);
    bus.controlType = 1;
    assert(!inverted.writeAndReadBack(100) && bus.writes == 4);
    bus.controlType = 0;
    bus.owner = false;
    assert(!inverted.writeAndReadBack(100) && !inverted.release() && bus.writes == 4);
    bus.owner = true;
    bus.mismatch = true;
    assert(!inverted.writeAndReadBack(100) && bus.writes == 5);
    assert(inverted.release() && bus.mode == 1 && bus.writes == 6);
    assert(!inverted.writeAndReadBack(100) && bus.writes == 6);

    Decision decision;
    decision.strategy = Strategy::SelfUse;
    decision.finalWatts = -200;
    decision.lower = -250;
    decision.upper = 250;
    constexpr uint32_t maxAge = 1000;

    Actuator actuator;
    LiveExecutor executor(actuator);
    assert(executor.tick(decision, 0, 0, maxAge).status == LiveStatus::Shadow);
    actuator.ownsControl = false;
    assert(!executor.arm(0, 200000));
    assert(actuator.writes == 0);
    actuator.ownsControl = true;
    assert(!executor.arm(0, 0));
    assert(executor.arm(0, 200000));
    assert(!executor.arm(0, 200000));
    assert(executor.tick(decision, 0, 0, maxAge).confirmedWatts == -200);
    const auto holding = executor.tick(decision, 29999, 29999, maxAge);
    assert(holding.status == LiveStatus::Holding && !holding.confirmedWatts);
    assert(actuator.writes == 1);
    assert(executor.tick(decision, 30000, 30000, maxAge).confirmedWatts == -200);
    decision.finalWatts = 0;
    assert(executor.tick(decision, 30001, 30001, maxAge).confirmedWatts == 0);
    assert(actuator.writes == 3);
    assert(executor.disarm().status == LiveStatus::Shadow);
    assert(executor.disarm().status == LiveStatus::Shadow && actuator.releases == 1);

    assert(executor.arm(0, 1000));
    assert(executor.tick(decision, 1000, 1000, maxAge).status == LiveStatus::LeaseExpired);
    assert(executor.arm(0, 200000));
    assert(executor.tick(decision, 0, 1001, maxAge).status == LiveStatus::DecisionInvalid);
    assert(executor.arm(0, 200000));
    decision.finalWatts = 251;
    assert(executor.tick(decision, 0, 0, maxAge).status == LiveStatus::DecisionInvalid);
    decision.finalWatts.reset();
    assert(executor.arm(0, 200000));
    assert(executor.tick(decision, 0, 0, maxAge).status == LiveStatus::DecisionInvalid);
    decision.finalWatts = 200;
    decision.strategy = Strategy::Native;
    assert(executor.arm(0, 200000));
    assert(executor.tick(decision, 0, 0, maxAge).status == LiveStatus::DecisionInvalid);

    decision.strategy = Strategy::ChargeFirst;
    assert(executor.arm(0, 200000));
    actuator.ownsControl = false;
    const int releases = actuator.releases;
    assert(executor.tick(decision, 0, 0, maxAge).status == LiveStatus::OwnershipLost);
    assert(actuator.releases == releases && actuator.writes == 3);

    actuator.ownsControl = true;
    assert(executor.arm(0, 200000));
    assert(executor.tick(decision, 0, 0, maxAge).confirmedWatts == 200);
    assert(executor.tick(decision, 120000, 120000, maxAge).status == LiveStatus::WatchdogExpired);
    assert(actuator.writes == 4);

    assert(executor.arm(0, 200000));
    actuator.readbackOffset = 1;
    const auto mismatch = executor.tick(decision, 0, 0, maxAge);
    assert(mismatch.status == LiveStatus::ReadbackFailed && !mismatch.confirmedWatts);
    assert(executor.arm(0, 200000));
    actuator.releaseOk = false;
    assert(executor.tick(decision, 0, 0, maxAge).status == LiveStatus::ReleaseFailed);
    assert(!executor.arm(0, 200000));

    // Single FC03 readback IO failure retries once; two consecutive failures stop.
    bus.mode = 5;
    bus.mismatch = false;
    bus.mode = 5;
    bus.failPowerReads = 1;
    assert(direct.writeAndReadBack(500) == 500);
    bus.failPowerReads = 2;
    assert(!direct.writeAndReadBack(500));
    assert(std::string_view(direct.lastError()) == "READBACK_IO_FAILED");

    Actuator afterReboot;
    LiveExecutor rebooted(afterReboot);
    assert(rebooted.tick(decision, 0, 0, maxAge).status == LiveStatus::Shadow);
    assert(afterReboot.writes == 0);
    assert(rebooted.arm(UINT32_MAX - 100, 200000));
    assert(rebooted.tick(decision, UINT32_MAX - 10, 10, maxAge).confirmedWatts == 200);

    Actuator regularBus;
    LiveExecutor continuous(regularBus);
    assert(continuous.arm(0, std::nullopt));
    for (uint32_t now = 0; now <= 360000; now += 30000)
        assert(continuous.tick(decision, now, now, maxAge).confirmedWatts == 200);
    assert(continuous.disarm().status == LiveStatus::Shadow && regularBus.releases == 1);
}
