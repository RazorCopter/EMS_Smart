# ADR-002: Physical Sign Conventions, Units, and Actuation Control Plane

## Context
Energy management systems often suffer from ambiguous sign conventions across differing hardware protocols (e.g. meter active power vs battery active power). The Afore AF5K-SL hybrid inverter documentation and Modbus register maps define external EMS control via registers `0x0A08` (EMS Mode) and `0x0A09-0x0A0A` (EMS Active Power Setpoint, 32-bit signed integer).

## Decision
1. **Canonical Internal Physical Conventions**:
   - `grid_power_w > 0`: Power imported from the grid (house consuming grid power).
   - `grid_power_w < 0`: Power exported to the grid (surplus injected into the grid).
   - `battery_power_w > 0`: Power flowing INTO the battery (charging).
   - `battery_power_w < 0`: Power flowing OUT OF the battery (discharging).
   - `pv_power_w >= 0`: Total solar photovoltaic generation.
   - `load_power_w >= 0`: Total household active power consumption.
   - Power balance equation:
     $$\text{load\_power\_w} = \text{pv\_power\_w} + \text{grid\_power\_w} - \text{battery\_power\_w}$$
2. **Standard Units**:
   - Power: Watts ($W$).
   - Energy: Watt-hours ($Wh$) or Kilowatt-hours ($kWh$) with explicit field name suffix.
   - State of Charge: Percent points ($0.0 \dots 100.0\%$).
   - Ramping: Watts per second ($W/s$).
3. **Control Actuation Plane (External EMS)**:
   - Control operates on the DC battery power setpoint via Modbus FC10 (Write Multiple Registers).
   - Setpoint is written atomically as a 32-bit signed integer into registers `0x0A09` (high word) and `0x0A0A` (low word).
   - Mode switch: Register `0x0A08` set to `0x0001` (External EMS Mode).
   - Refresh cadence: Setpoint MUST be refreshed every 30 to 60 seconds (vendor watchdog timeout is 120 seconds, after which inverter transitions to 0 W idle).
4. **Permanent Register Exclusions**:
   - Registers `0x0BC1` and `0x0BC2` (Maximum Charge/Discharge Current) write into non-volatile EEPROM and are STRICTLY AND PERMANENTLY FORBIDDEN from all dynamic EMS write paths to prevent hardware destruction by wear-out.

## Consequences
- Every algorithm inside the EMS core computes strictly in these canonical physical units.
- Any hardware driver or protocol adapter handles sign flipping and register scaling at the driver boundary, verified by unit tests (such as `test-power-balance`).
