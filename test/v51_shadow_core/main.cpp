// SPDX-License-Identifier: MIT
#include "v51/shadow_core.h"
#include <cassert>
#include <string_view>
using namespace heliograph::v51;
EnergyState sample(double soc,double grid,int ms) {
    EnergyState e;
    e.goodReads=1;
    e.batteryVoltage={51.2,uint32_t(ms),Quality::Valid};
    e.soc={soc,uint32_t(ms),Quality::Valid};
    e.grid={grid,uint32_t(ms),Quality::Valid};
    e.battery={0,uint32_t(ms),Quality::Valid};
    return e;
}
int main() {
    StrategyManager m;
    Policy p;
    assert(p.target==100 && p.capacityWh==10240 && p.nominalMv==51200 && p.valid());
    p.target=80;
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
    Policy limits;
    limits.discharge=800;
    limits.dischargeA=8;
    auto power=sample(50,600,33000);
    power.batteryVoltage={50,33000,Quality::Valid};
    assert(m.tick(power,limits,6,33000).finalWatts==-400);
    power.batteryVoltage.status=Quality::Missing;
    assert(m.tick(power,limits,6,33000).finalWatts==0);
    limits.dischargeA=0;
    limits.chargeA=0;
    limits.exportLimit=200;
    power.grid.value=-500;
    assert(m.tick(power,limits,7,33000).lower==300);
    Policy daily;
    daily.strategy=4;
    StrategyManager scheduler;
    tm time{};
    time.tm_year=126; time.tm_yday=320; time.tm_hour=15; time.tm_min=29;
    assert(scheduler.tick(sample(50,-2000,40000),daily,1,40000,&time).strategy==Strategy::Target);
    time.tm_min=30;
    assert(scheduler.tick(sample(50,-2000,41000),daily,1,41000,&time).strategy==Strategy::SelfUse);
    time.tm_yday=321; time.tm_hour=8; time.tm_min=0;
    assert(scheduler.tick(sample(50,-2000,42000),daily,1,42000,&time).strategy==Strategy::Target);
    time.tm_yday=320;
    assert(scheduler.tick(sample(50,-2000,43000),daily,1,43000,&time).strategy==Strategy::SelfUse);
    time.tm_yday=321;
    assert(scheduler.tick(sample(50,-2000,44000),daily,1,44000,&time).strategy==Strategy::SelfUse);
    time.tm_yday=322;
    assert(scheduler.tick(sample(50,-2000,45000),daily,1,45000,&time,3600).strategy==Strategy::Target);

    StrategyManager lateClock;
    daily.deadlineSec=2;
    assert(lateClock.tick(sample(50,-2000,1000),daily,1,1000).strategy==Strategy::Target);
    assert(lateClock.tick(sample(50,-2000,3000),daily,1,3000).strategy==Strategy::SelfUse);
    assert(lateClock.tick(sample(50,-2000,4000),daily,1,4000,&time,3600).strategy==Strategy::SelfUse);
    p.ceiling=10;
    assert(!p.valid());
}
