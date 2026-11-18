// SPDX-License-Identifier: MIT
#include "v51/shadow_core.h"
#include <cassert>

int main() {
    using namespace heliograph::v51;
    EnergyState input{};
    input.goodReads = 3;
    input.soc = {50, 1000, Quality::Valid};
    input.grid = {600, 1000, Quality::Valid};
    input.battery = {0, 1000, Quality::Valid};
    auto result = arbitrate(input, Strategy::SelfUse, 1000, {}, 1000);
    assert(result.finalWatts == -600 && !result.executed);
    input.soc.value = 20;
    assert(arbitrate(input, Strategy::SelfUse, 1000, -600, 1000).finalWatts == 0);
    input.grid.value = -2000;
    input.soc.value = 50;
    assert(arbitrate(input, Strategy::ChargeFirst, 1000, {}, 1000).finalWatts == 1000);
    input.soc.value = 95;
    assert(arbitrate(input, Strategy::ChargeFirst, 1000, 500, 1000).finalWatts == 0);
    assert(!arbitrate(input, Strategy::Native, 1000, {}, 1000).finalWatts);
    assert(arbitrate(input, Strategy::SelfUse, 7000, {}, 1000).strategy == Strategy::Safe);
    assert(arbitrate(input, Strategy::Safe, 1000, {}, 1000).strategy == Strategy::Safe);
    input.readHalted = true;
    assert(!arbitrate(input, Strategy::SelfUse, 1000, {}, 1000).candidate);
}
