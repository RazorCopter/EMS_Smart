# Afore AF5K-SL — Modbus RTU register map

Single-phase 5kW hybrid inverter with battery port. Profile lives in
[`profiles/afore/af5k_sl.toml`](../profiles/afore/af5k_sl.toml).

**STATUS: Experimental — Phase 0 read-only shadow mode.** This map derives from official Afore
protocol documentation (`Doc/外部Ems调度 - En_240312.docx`) and validated Solarman integration
configuration (`afore_AF5K-Sl.yaml` via Home Assistant), cross-referenced against operational
telemetry from `Doc/Afore_Autonomous_EMS_V4_Roadmap.md` Section 4.

**CRITICAL SAFETY CONSTRAINTS:**
- **Phase 0 READ-ONLY**: No write operations permitted until Phase 6 controlled activation.
- **EEPROM Protection**: Register `0x0BC1` (`CurrMaxChg`) is **STRICTLY FORBIDDEN** from dynamic
  writes. Legacy automation V3.3 disabled due to EEPROM wear risk.
- **External EMS Dormant**: Control registers (`0x09C4`, `0x0A08`, `0x0A09-0x0A0A`) documented but
  not mapped in write path until Phase 6 validation criteria met.

## Connection

| | |
|---|---|
| Port | RS485 COM1, pins A/B (1/2 per Afore hybrid single-phase spec) |
| Line | 9600 8N1 (to be confirmed during Phase 0 scouting) |
| Register space | **Holding registers (function 03)** for readings; FC10 for External EMS writes |
| Factory unit ID | 1 (expected; verify via passive observation) |

The Afore AF5K-SL exposes Modbus RTU on COM1. **Phase 0 Rule:** Confirm baud rate, parity, slave
ID, and bus coexistence with existing Solarman datalogger before sending queries. Start with
passive sniffing if possible.

## Sources & Verification Taxonomy

| | What |
|---|---|
| **A** | Afore official documentation: `Doc/外部Ems调度 - En_240312.docx` (External EMS Modbus Protocol Specification) and `Doc/艾伏逆变器寄存器参数表（対外）v3.4_20251119_並網機和儲能機分類(2).xlsx` (Register Parameter Table) |
| **B** | `Doc/Afore_Autonomous_EMS_V4_Roadmap.md` Section 4 — Telemetry registers validated against Solarman integration |
| **C** | Solarman Home Assistant integration: `afore_AF5K-Sl.yaml` (production deployment, operational baseline) |

### VERIFIED vs UNVERIFIED Classification

In strict adherence to project safety rules (GAP_ASSESSMENT.md §7), no register address or scale
factor is assumed without documentation. Each register below carries one of two states:

- **VERIFIED**: Confirmed in official Afore documentation (Source A) **AND/OR** validated in
  operational Solarman integration (Sources B+C). Address, type, scale, and signedness are
  documented by at least one authoritative source.
  
- **UNVERIFIED**: Estimated from vendor table variations, inferred from protocol patterns, or
  observed in logs without cross-confirmation. Must be validated through passive observation in
  Phase 0 before promoting to VERIFIED. **These registers may be wrong.**

Every row states its verification status and source explicitly. No register is mapped without
stating confidence.

## Mapped Registers — Telemetry (Read-Only)

PDU addresses (0-based Modbus protocol addresses), holding registers (FC03).  
Format: `value = raw × scale + offset`

### PV (Photovoltaic) — String Power

| PDU | Hex | Type | Scale | Canonical id | Verification |
|---|---|---|---|---|---|
| 554 | 0x022A | u16 | 1 | `dc.power.total` | **VERIFIED** (B, C) — Total PV power, sum of string 1+2 |
| 557 | 0x022D | u16 | 1 | `dc.mppt_1.power` | **VERIFIED** (B) — PV string 1 power |
| 560 | 0x0230 | u16 | 1 | `dc.mppt_2.power` | **VERIFIED** (B) — PV string 2 power |

**Source notes:**
- Roadmap Section 4 explicitly lists `0x022A` (total), `0x022D` (PV1), `0x0230` (PV2) as U16, 1W.
- Solarman YAML confirms these addresses in active deployment.
- **Phase 0 validation:** Check for saturation at high power (>5000W) and zero handling at night.

### PV — String Voltage and Current

| PDU | Hex | Type | Scale | Canonical id | Verification |
|---|---|---|---|---|---|
| 552 | 0x0228 | u16 | 0.1 | `dc.mppt_1.voltage` | **UNVERIFIED** — Vendor table lists this offset; confirm via display cross-check |
| 553 | 0x0229 | u16 | 0.1 | `dc.mppt_1.current` | **UNVERIFIED** — Adjacent to voltage; scale 0.1A assumed from similar devices |
| 558 | 0x022E | u16 | 0.1 | `dc.mppt_2.voltage` | **UNVERIFIED** — Vendor table lists this offset; confirm via display cross-check |
| 559 | 0x022F | u16 | 0.1 | `dc.mppt_2.current` | **UNVERIFIED** — Adjacent to voltage; scale 0.1A assumed from similar devices |

**Source notes:**
- Roadmap Section 4 mentions these exist but does not explicitly verify addresses.
- GAP_ASSESSMENT.md §7.1 flags "offset variation between single-phase and 3-phase parameter tables."
- **Phase 0 validation:** Read during daylight with known PV panel specs; cross-check P = V × I.

### Battery

| PDU | Hex | Type | Scale | Canonical id | Verification |
|---|---|---|---|---|---|
| 2002 | 0x07D2 | u16 | 1 | `battery.soc` | **VERIFIED** (A, B, C) — State of Charge, 0–100% |
| 2004 | 0x07D4 | u16 | 0.1 | `battery.voltage` | **VERIFIED** (B, C) — Battery pack voltage |
| 2005 | 0x07D5 | s16 | 0.01 | `battery.current` | **VERIFIED** (B, C) — **Sign convention to confirm in Phase 0** |
| 2008 | 0x07D8 | s16 | 1 | `battery.power` | **VERIFIED** (B, C) — **Negative observed during charge** (Roadmap §4) |
| 2000 | 0x07D0 | u16 | — | `battery.state` (enum) | **VERIFIED** (B) — 4=Charge, 5=Discharge, 6=Standby |

**Sign convention critical notes:**
- **Battery Current (0x07D5):** Roadmap states "verificare codifica reale" — **direction unconfirmed**.
  Typical convention: positive = charging, negative = discharging. Confirm via simultaneous SOC
  observation.
- **Battery Power (0x07D8):** Roadmap explicitly states "Negativo osservato in carica" (negative
  observed during charge). This is **OPPOSITE** of canonical `battery.power` convention (positive
  = charging). **Profile uses `scale = -1` to flip sign** — verify this is correct across full
  charge/discharge cycle in Phase 0.

### Grid and Inverter Output

| PDU | Hex | Type | Scale | Canonical id | Verification |
|---|---|---|---|---|---|
| 530 | 0x0212 | s16 | 1 | `grid.power` | **VERIFIED** (B, C) — **Import/export sign to confirm in Phase 0** |
| 523 | 0x020B | s16 | 1 | `ac.power.total` | **VERIFIED** (B, C) — Inverter AC output power |

**Sign convention critical notes:**
- **Grid Power (0x0212):** Roadmap states "verificare convenzione import/export." Canonical
  convention: positive = importing from grid, negative = exporting to grid. **Phase 0 must confirm
  direction** by observing grid power during known import/export states.
- **Inverter Output (0x020B):** Roadmap describes as "potenza uscita," physical AC active power.
  Should be positive when producing.

### Grid — Voltage and Frequency

| PDU | Hex | Type | Scale | Canonical id | Verification |
|---|---|---|---|---|---|
| 515 | 0x0203 | u16 | 0.1 | `ac.phase_l1.voltage` | **UNVERIFIED** — Typical grid voltage register; confirm via multimeter cross-check |
| 517 | 0x0205 | u16 | 0.01 | `ac.frequency` | **UNVERIFIED** — Standard frequency register; confirm 50.00 Hz nominal |

**Source notes:**
- GAP_ASSESSMENT.md §7.1 lists these as UNVERIFIED; passive capture needed.
- Expected values: 230V ±10%, 50.00 Hz ±0.5 Hz for European grid.

### Inverter Status and Temperature

| PDU | Hex | Type | Scale | Canonical id | Verification |
|---|---|---|---|---|---|
| 512 | 0x0200 | u16 | — | `inverter.status` (enum) | **UNVERIFIED** — Running status; Solarman mapping varies by firmware |
| 524 | 0x020C | s16 | 0.1 | `inverter.temperature` | **UNVERIFIED** — Sensor location and scale to confirm |

**Source notes:**
- **Status Register Ambiguity:** Roadmap §4 warns "evitare confusione tra running status ed EMS
  mode." Register `0x0200` is inverter running status; `0x09C4` is EMS mode (External EMS control).
  These are **different registers with different semantics**.
- GAP_ASSESSMENT.md flags firmware revision sensitivity on status enum values.

## Not Mapped — External EMS Control Registers (Phase 6 Only)

The following registers are **DOCUMENTED but NOT MAPPED** in the read-only shadow profile. They
exist in official Afore External EMS specification but are excluded from Phase 0–5 to enforce
read-only safety boundaries.

| PDU | Hex | Function | Type | Verification | Phase 0 Status |
|---|---|---|---|---|---|
| 2500 | 0x09C4 | EMS Mode | u16 (enum) | **VERIFIED** (A) — 5 = External EMS | **NOT MAPPED** |
| 2568 | 0x0A08 | External Control Type | u16 (enum) | **VERIFIED** (A) — 0=battery power, 1=inverter power | **NOT MAPPED** |
| 2569–2570 | 0x0A09–0x0A0A | Active Power Setpoint | s32 | **VERIFIED** (A) — Signed 32-bit watts, requires FC10 | **NOT MAPPED** |
| 3009 | 0x0BC1 | Max Charge Current (`CurrMaxChg`) | u16 | **VERIFIED** (A, B) — **EEPROM HAZARD** | **STRICTLY FORBIDDEN** |

**Critical safety notes:**
- **Register 0x0BC1** writes to non-volatile EEPROM. Legacy automation V3.3 wrote this
  periodically, creating severe wear risk. Roadmap §5 explicitly forbids dynamic writes.
- **External EMS setpoints (0x0A09–0x0A0A)** require Modbus Function Code 10 (Write Multiple
  Registers). Heliograph's `ModbusClient` does not implement FC10, making accidental writes
  structurally impossible. These will be added only in Phase 6 after validation criteria met.
- **EMS Mode (0x09C4)** is a configuration register set once to enable External EMS control, not a
  dynamic telemetry value. Phase 0 may read it for diagnostics but must never write.

## Register Blocks — Polling Strategy

Phase 0 conservative read blocks (5–10s poll interval, zero retries during scouting):

```toml
[[block]]
space = "holding"
start = 512          # 0x0200–0x0212: Inverter status, output power, grid power
count = 19

[[block]]
space = "holding"
start = 552          # 0x0228–0x0230: PV1/PV2 voltage, current, power
count = 9

[[block]]
space = "holding"
start = 2000         # 0x07D0–0x07D8: Battery state, SOC, voltage, current, power
count = 9
```

**Phase 0 optimization:** Blocks are kept tight around verified registers. Wider blocks can be used
during initial trace-logging for raw dump inspection, then narrowed once map is confirmed.

**Block size constraint:** Modbus RTU read limit is 125 registers per transaction. All blocks above
respect this limit.

## What is Not Mapped, and Why

- **Energy counters** (if they exist): Phase 0 focuses on instantaneous power telemetry. Energy
  totals will be added in Phase 1 once register addresses confirmed.
- **Advanced diagnostic registers**: Phase 0 prioritizes control-loop telemetry (PV, battery, grid).
  Fault codes, MPPT efficiency, etc. will be mapped after core loop validated.
- **Three-phase registers**: AF5K-SL is single-phase. Any three-phase registers in vendor tables
  belong to different models (e.g., AF10K-T).

## Phase 0 Validation Checklist

Before this map moves from `experimental` to `beta`:

- [ ] **Serial parameters confirmed:** 9600 8N1, slave ID 1 validated via bus capture or Solarman
      query comparison.
- [ ] **Sign conventions verified:**
  - [ ] Battery power (0x07D8): Negative during charge, positive during discharge → profile scale
        = -1 is correct.
  - [ ] Battery current (0x07D5): Sign matches SOC change direction.
  - [ ] Grid power (0x0212): Positive = import, negative = export.
- [ ] **Voltage/current cross-checks:** PV and battery P = V × I within 2% tolerance.
- [ ] **UNVERIFIED registers promoted:** Addresses 0x0228, 0x0229, 0x022E, 0x022F, 0x0203, 0x0205,
      0x0200, 0x020C confirmed against inverter display.
- [ ] **24–72h endurance run:** ≥99.5% valid read rate, zero CRC errors, no Solarman degradation.
- [ ] **No bus collisions:** Coexistence with Solarman datalogger confirmed, or alternative single-
      master strategy documented.

## Bring-up Protocol (Phase 0)

1. **Passive inspection:** Photograph COM1 pinout, verify A/B polarity, document Solarman dongle
   model and firmware.
2. **Serial parameter discovery:** Use known parameters from Solarman config or vendor defaults
   (9600 8N1). Do not scan aggressively on operational bus.
3. **Conservative query:** Read one register at a time with 5–10s interval. Log every transaction
   (request, response, latency, CRC status).
4. **Cross-validation:** Compare every reading against:
   - Inverter's own LCD display (same timestamp within 1s)
   - Solarman Home Assistant sensor value (log time delta)
   - Physical measurements (multimeter for voltage, known load for power)
5. **Sign convention confirmation:** Observe battery power and grid power through full
   charge/discharge and import/export cycles. Document observed sign per state.
6. **Duration test:** Run 24–72h with comprehensive logging. Analyze: valid read rate, timeout
   rate, CRC error rate, latency P50/P95/P99, data age, Solarman operational status.

## Open Questions (Roadmap Section 14 Alignment)

1. **COM1 multi-master coexistence:** Can Raspberry Pi query inverter without disturbing Solarman
   datalogger? Presume NO until proven otherwise.
2. **Slave ID configuration:** Is unit ID 1 factory default, or configured per installation?
3. **Battery current sign:** Positive = charging or discharging? (Critical for control loop sign
   errors.)
4. **Grid power sign:** Positive = import or export? (Critical for self-consumption logic.)
5. **Firmware revision sensitivity:** Do register addresses or enum values vary by AF5K-SL firmware
   version?
6. **UNVERIFIED register accuracy:** Are PV voltage/current registers at documented offsets, or do
   they vary between single-phase and three-phase parameter tables?
7. **Max sustainable poll rate:** What polling frequency can COM1 handle without bus degradation?
   Start at 10s, reduce only with evidence.

## External EMS Protocol Summary (Phase 6 Reference)

For completeness, the External EMS control protocol (NOT ACTIVE IN PHASE 0):

- **Mode register (0x09C4):** Set to `5` (External EMS) to enable external control. Configuration
  register, written once.
- **Control type (0x0A08):** `0` = battery charge/discharge power, `1` = inverter output power,
  `2` = grid power (development).
- **Setpoint (0x0A09–0x0A0A):** Signed 32-bit watts. Requires Modbus FC10 (Write Multiple
  Registers). Must be repeated every <2 minutes (recommended 30–60s) or inverter assumes
  communication lost and reverts to 0W setpoint.
- **Sign convention (to verify in Phase 6):** Negative = charge, positive = discharge per Roadmap
  §5 (manufacturer guidance, subject to field confirmation).

**Phase 0 rule:** These registers may be READ for diagnostics (current EMS mode state) but must
NEVER be written until Phase 6 controlled activation after shadow mode validation.

---

**Document Status:** Draft for Phase 0 implementation. All UNVERIFIED registers subject to
correction after passive observation. Map will be updated as Phase 0 validation proceeds.

**Last Updated:** 2026-11-10  
**Schema Version:** 1 (aligned with Heliograph register map schema v1)  
**Profile Target:** `profiles/afore/af5k_sl.toml`
