// SPDX-License-Identifier: MIT
#include "v51/shadow_core.h"
#include <cassert>
#include <string_view>
using namespace heliograph::v51;
EnergyState sample(double soc,double grid,int ms) {
    EnergyState e;
    e.goodReads=1;
    e.soc={soc,uint32_t(ms),Quality::Valid};
    e.grid={grid,uint32_t(ms),Quality::Valid};
    e.battery={0,uint32_t(ms),Quality::Valid};
    return e;
}
int main() {
    StrategyManager m;
    Policy p;
    assert(p.valid());
    assert(m.tick(sample(50,600,1000),p,1,1000).finalWatts == -600);
    assert(m.tick(sample(20,600,2000),p,1,2000).finalWatts == 0);
    assert(m.tick(sample(50,500,1000),p,1,8000).strategy==Strategy::Safe);
    p.strategy=1;
    assert(m.tick(sample(50,-1200,9000),p,2,9000).finalWatts==1000);
    assert(m.tick(sample(79,-1200,10000),p,2,10000).candidate==250);
    assert(m.tick(sample(77,-1200,10500),p,2,10500).candidate==250);
    assert(m.tick(sample(75,-1200,10700),p,2,10700).candidate==1000);
    assert(m.tick(sample(80,-1200,11000),p,2,11000).strategy==Strategy::SelfUse);
    p.strategy=4; p.fallback=2; p.deadlineSec=10;
    auto d=m.tick(sample(50,-100,12000),p,3,12000);
    assert(d.unreachable && d.missingWh && d.averageW && d.finalWatts==100);
    assert(m.tick(sample(50,-100,23000),p,3,23000).strategy==Strategy::Native);
    p.strategy=5; p.maintenanceHoldSec=2;
    assert(std::string_view(m.tick(sample(90,-100,24000),p,4,24000).state)=="HOLD");
    assert(m.tick(sample(90,-100,27000),p,4,27000).strategy==Strategy::Native);
    auto faulty=sample(85,-100,30000);
    m.tick(faulty,p,5,30000);
    faulty.readHalted=true;
    assert(m.tick(faulty,p,5,31000).strategy==Strategy::Safe);
    d=m.tick(sample(85,-100,32000),p,5,32000);
    assert(d.strategy==Strategy::Native && std::string_view(d.transition)=="MAINTENANCE_FAULT_ABORT");
    assert(!d.executed);
    p.ceiling=10;
    assert(!p.valid());
}
