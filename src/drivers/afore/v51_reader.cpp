// SPDX-License-Identifier: MIT
#include "drivers/afore/v51_reader.h"

#include <Arduino.h>
#include "v51/remote.h"
#include <array>
#include <cmath>
#include <cstring>

namespace heliograph::v51 {
namespace {
enum class WordType { U16, S16, U32, S32 };
struct Block { uint16_t address, count; };
struct Field { const char* id; uint16_t address; WordType type; double scale; };

#include "v51_profile_generated.h"

constexpr uint8_t kUnit = 1;
constexpr Block kSocProbe{2002, 1};
constexpr auto kBlockCount = sizeof(kBlocks) / sizeof(*kBlocks);

Sample* matchField(EnergyState& state, const char* id) {
    if (strcmp(id, "battery.soc") == 0) return &state.soc;
    if (strcmp(id, "dc.power.total") == 0) return &state.pv;
    if (strcmp(id, "grid.power") == 0) return &state.grid;
    if (strcmp(id, "battery.power") == 0) return &state.battery;
    if (strcmp(id, "ac.power.total") == 0) return &state.inverter;
    if (strcmp(id, "inverter.temperature") == 0) return &state.temperature;
    if (strcmp(id, "ac.phase_l1.voltage") == 0) return &state.voltage;
    if (strcmp(id, "ac.frequency") == 0) return &state.frequency;
    if (strcmp(id, "battery.voltage") == 0) return &state.batteryVoltage;
    if (strcmp(id, "battery.current") == 0) return &state.batteryCurrent;
    if (strcmp(id, "dc.mppt_1.voltage") == 0) return &state.pv1v;
    if (strcmp(id, "dc.mppt_1.current") == 0) return &state.pv1a;
    if (strcmp(id, "dc.mppt_1.power") == 0) return &state.pv1w;
    if (strcmp(id, "dc.mppt_2.voltage") == 0) return &state.pv2v;
    if (strcmp(id, "dc.mppt_2.current") == 0) return &state.pv2a;
    if (strcmp(id, "dc.mppt_2.power") == 0) return &state.pv2w;
    return nullptr;
}

void update(EnergyState& state, Block block, const uint16_t* words, uint32_t stamp, bool success) {
    for (const auto& field : kFields) {
        const bool wide = field.type == WordType::S32 || field.type == WordType::U32;
        if (field.address < block.address ||
            uint32_t(field.address) + (wide ? 2 : 1) > uint32_t(block.address) + block.count) {
            continue;
        }
        auto* sample = matchField(state, field.id);
        if (!sample) {
            continue;
        }
        if (!success) {
            sample->status = Quality::ReadError;
            continue;
        }
        const auto offset = field.address - block.address;
        const auto upper = words[offset];
        double raw = upper;
        if (wide) {
            const auto joined = (uint32_t(upper) << 16) | words[offset + 1];
            raw = field.type == WordType::S32 ? double(static_cast<int32_t>(joined)) : double(joined);
        } else if (field.type == WordType::S16) {
            raw = static_cast<int16_t>(upper);
        }
        const double value = raw * field.scale;
        const bool validSoc = std::isfinite(value) && std::abs(value) <= 30000 &&
                              (sample != &state.soc || (value >= 0 && value <= 100)) &&
                              (sample != &state.pv || value >= 0);
        if (state.goodReads <= kBlockCount) {
            char line[220];
            if (validSoc) snprintf(line, sizeof(line), "FC04_FIELD id=%s addr=%u scale=%.5g value=%.4g quality=valid age_ms=0", field.id, field.address, field.scale, value);
            else snprintf(line, sizeof(line), "FC04_FIELD id=%s addr=%u scale=%.5g value=null quality=invalid", field.id, field.address, field.scale);
            recordLog(line);
        }
        *sample = {validSoc ? std::optional<double>(value) : std::nullopt, stamp,
                   validSoc ? Quality::Valid : Quality::Invalid};
    }

    const auto inverter = state.inverter.usable(stamp);
    const auto grid = state.grid.usable(stamp);
    if (inverter && grid && state.inverter.stamp == state.grid.stamp && *inverter + *grid >= 0) {
        state.load = {*inverter + *grid, state.grid.stamp, Quality::Valid};
    } else {
        state.load = {};
    }
}
}

void AforeReader::start(uint32_t now) {
    if (!started_) {
        started_ = true;
        state_.readHalted = !transport_.begin(kBaud);
        nextPoll_ = now + 5000;
        recordLog(state_.readHalted ? "RS485_INIT_FAILED" : "RS485_LISTEN_ONLY");
    }
}

void AforeReader::tick(uint32_t now) {
    if (!started_ || state_.readHalted) {
        return;
    }
    if (!transport_.observeBus()) {
        state_.readHalted = true;
        recordLog("RS485_HALTED unexpected traffic while idle; possible second master");
        return;
    }
    if (static_cast<int32_t>(now - nextPoll_) < 0) {
        return;
    }

    const auto block = probed_ ? kBlocks[block_] : kSocProbe;
    std::array<uint16_t, 125> words{};
    const auto result = transport_.readInputRegisters(kUnit, block.address, block.count, words.data());
    bool ok = result == ReadOnlyModbusTransport::Result::Ok;
    const auto stamp = millis();
    update(state_, block, words.data(), stamp, ok);
    if (ok && ((block.address == 2002 || block.address == 2000) &&
               (state_.soc.status != Quality::Valid ||
                (block.address == 2000 && state_.battery.status != Quality::Valid)) ||
               (block.address == 507 && state_.grid.status != Quality::Valid) ||
               (block.address == 552 && state_.pv.status != Quality::Valid))) {
        recordLog("FC04_DECODE_INVALID");
        ok = false;
    }
    if (ok) {
        if (state_.goodReads <= kBlockCount) {
            char line[220];
            int used = snprintf(line, sizeof(line), "FC04_BLOCK addr=%u count=%u raw_hex=", block.address, block.count);
            for (unsigned i = 0; i < block.count && used + 6 < int(sizeof(line)); ++i) {
                used += snprintf(line + used, sizeof(line) - used, "%04x ", words[i]);
            }
            recordLog(line);
        }
        ++state_.goodReads;
        failures_ = 0;
        if (!probed_) {
            probed_ = true;
            char line[96];
            snprintf(line, sizeof(line), "FC04_SOC_PROBE addr=2002 valid=%d raw=%u", state_.soc.status == Quality::Valid, words[0]);
            recordLog(line);
        } else {
            block_ = (block_ + 1) % kBlockCount;
        }
    } else {
        ++state_.errors;
        ++failures_;
        state_.readHalted = result == ReadOnlyModbusTransport::Result::Traffic || failures_ >= 3;
        char line[128];
        snprintf(line, sizeof(line), "FC04_ERROR addr=%u count=%u result=%d failures=%u%s",
                      block.address, block.count, int(result), failures_,
                      state_.readHalted ? " RS485_HALTED" : "");
        recordLog(line);
    }
    nextPoll_ = stamp + (ok ? 400 : 1000);
}
}
