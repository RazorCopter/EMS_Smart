# OpenEMS Reference Comparison & Adaptation Matrix (Milestone M1)

Date: 2026-11-13  
Reference Repository: [OpenEMS](https://github.com/OpenEMS/openems)  
Reference Commit: `769483ea2c2cc8be64fbf20094e70cea89f0f307`  
Target System: Afore AF5K-SL Hybrid Inverter with Battery  
Target Embedded Runtime: C++17 (Linux Host daemon & ESP32-S3 firmware)

---

## 1. Architectural Differences & Licensing Boundary

| Property | OpenEMS Reference | Afore Autonomous EMS Implementation |
|---|---|---|
| **Language & Runtime** | Java 17+ (OSGi / Apache Felix framework) | Pure C++17 (Common core shared between Linux POSIX and ESP32 FreeRTOS) |
| **Memory Footprint** | ~200–512 MB JVM heap | < 500 KB RAM heap budget (compatible with 512 KB SRAM ESP32) |
| **Cycle Frequency** | 1000 ms default cycle loop | 1000 ms core evaluation; Modbus polling 2000–5000 ms; control renew 30–60 s |
| **Licensing** | EPL-2.0 (Eclipse Public License 2.0) | MIT License. The algorithms are independently reimplemented in clean-room C++17 following the mathematical and control principles; no Java bytecode or OSGi classes copied. |

---

## 2. Controller-by-Controller Mapping

### 2.1 `Controller.Ess.Balancing`
- **OpenEMS Class**: `io.openems.edge.controller.ess.balancing.BalancingImpl`
- **Core Formula in OpenEMS**:
  ```
  activePower = ess.getActivePower() - meter.getActivePower()
  ```
  where positive meter power means grid purchase, and positive ESS power means discharge.
- **Afore EMS Adaptation**:
  - Adopt our explicit physical sign convention:
    - `grid_power_w > 0`: import from grid.
    - `battery_power_w > 0`: charging battery; `< 0`: discharging battery.
    - Target battery power to balance house consumption:
      $$\text{target\_battery\_power\_w} = \text{current\_battery\_power\_w} + \text{grid\_power\_w}$$
    - Clipping applied: clamped to $[-\text{max\_discharge\_w}, +\text{max\_charge\_w}]$, clamped to battery BMS charge/discharge limits, and limited by slew rate (W/s ramp).
  - **Deadband**: configurable $\pm 20\text{ W}$ deadband around 0 W grid exchange to prevent oscillation and relay wear.
  - **Missing Meter Policy**: If grid meter is unavailable or stale (> 5 s), battery discharge is immediately curtailed to 0 W to prevent unmeasured grid backfeeding.

### 2.2 `Controller.Ess.GridOptimizedCharge`
- **OpenEMS Class**: `io.openems.edge.controller.ess.gridoptimizedcharge.GridOptimizedChargeImpl`
- **OpenEMS Behavior**: Calculates forecasted solar surplus and limits battery charge during early morning so battery reaches 100% just before peak solar window, avoiding grid export feed-in limitation clipping.
- **Afore EMS Adaptation**:
  - **Solar Deferral Mode**: When solar forecast predicts daily production in excess of battery capacity and daytime load, the controller limits morning charge power to keep battery headroom available for the peak solar production hours.
  - **Fallbacks**: If solar forecast from Home Assistant is missing or expired (TTL > 3 hours), reverts immediately to standard immediate Self-Use charging.

### 2.3 `Controller.Ess.LimitTotalDischarge`
- **OpenEMS Class**: `io.openems.edge.controller.ess.limittotaldischarge.LimitTotalDischargeImpl`
- **OpenEMS Behavior**: Three-stage state machine protecting ESS deep discharge: Normal -> MinSoc -> CriticalSoc (forces grid charge if SOC drops below critical threshold).
- **Afore EMS Adaptation**:
  - **SOC Reserve Floor**: Configurable minimum discharge limit (e.g. 15% or 20%). When $\text{SOC} \le \text{soc\_min}$, battery discharge is strictly forbidden ($\text{max\_discharge} = 0$).
  - **Critical Low SOC Grid Rescue**: If $\text{SOC} < \text{soc\_critical}$ (e.g. 10%) due to self-discharge, an emergency low-power grid trickle charge (+500 W) is requested until $\text{SOC} \ge \text{soc\_min}$.
  - **Self-Discharge Compensation**: Hysteresis applied to prevent rapid cycling between rescue and idle.

### 2.4 `Controller.Ess.TimeOfUseTariff`
- **OpenEMS Class**: `io.openems.edge.controller.ess.timeofusetariff.TimeOfUseTariffImpl`
- **OpenEMS States**: Balancing, Delay Discharge, Charge Grid, Discharge Grid, Peak Shaving, Avoid Grid Sell Limit.
- **Afore EMS Adaptation**:
  - Matrix comparison:
    - *Balancing*: Supported (baseline mode).
    - *Delay Discharge*: Supported (holds battery during cheap grid windows for subsequent expensive hours).
    - *Charge Grid*: Supported (pre-charges battery from grid during ultra-cheap/negative electricity price slots, subject to explicit user configuration).
    - *Discharge Grid (Export Arbitrage)*: Initially excluded/dormant by default (requires grid operator approval and battery warranty verification; configurable flag disabled).
  - **Bounded Dynamic Optimizer**: 48-hour planning horizon in 15-minute or 60-minute discrete slots (up to 192 slots max). Bounded execution time (< 50 ms) suitable for low-power host and ESP32.
  - **Robustness**: Handles negative prices safely, flat tariffs gracefully, and gracefully recovers when price feed stops.

### 2.5 `Controller.Ess.FixStateOfCharge`
- **OpenEMS Class**: `io.openems.edge.controller.ess.fixstateofcharge.FixStateOfChargeImpl`
- **OpenEMS Behavior**: Periodically schedules a forced full charge (100% SOC) to enable BMS cell balancing and SOC recalibration.
- **Afore EMS Adaptation**:
  - **Maintenance Workflow**:
    - States: `IDLE`, `SCHEDULED`, `CHARGING_TO_TARGET`, `CELL_BALANCING_HOLD`, `COMPLETED`, `ABORTED`.
    - Enforces maximum duration timeout (e.g., 6 hours) to prevent indefinite high-voltage holding.
    - Automatic return to normal Self-Use once cell balancing is confirmed or timeout expires.
    - Persisted state across daemon restarts to avoid restarting full cycles prematurely.
