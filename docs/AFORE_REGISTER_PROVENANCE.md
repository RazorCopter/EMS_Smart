# Afore AF5K-SL — Register Provenance & Phase 0 Conservative Probe Plan

> **STATUS:** Offline Architectural Baseline (Milestone M6).  
> **EXECUTION CONSTRAINT:** 100% OFFLINE DOCUMENTAL VALIDATION. No serial port access, no physical bus probing, no hardware interrogation. Physical probing is strictly deferred until formal G1/G2 governance approvals.

---

## 1. Documental Sources & Baseline Taxonomy

The register definitions for the Afore AF5K-SL single-phase hybrid inverter (5 kW) are derived from four documented baselines:

1. **Source A1 (Excel Afore v3.4):**  
   `Doc/艾伏逆变器寄存器参数表（对外）v3.4_20251119_并网机和储能机分类(2).xlsx`  
   Sheets: `运行信息04` (Read-only dynamic operational telemetry, FC 0x04) and `参数读写03-06-10` (Settings/Parameters, FC 0x03, 0x06, 0x10).
2. **Source A2 (Document En_240312 External EMS):**  
   `Doc/外部Ems调度  - En_240312.docx` (External EMS Modbus Protocol Specification).
3. **Source B (Afore Autonomous EMS V4 Roadmap):**  
   `Doc/Afore_Autonomous_EMS_V4_Roadmap.md` (Section 4 Telemetry Registers & Section 5 EEPROM Protection).
4. **Source C (Solarman Production Integration):**  
   Solarman Home Assistant integration configuration `afore_AF5K-Sl.yaml` (field-deployed operational baseline).
5. **Empirical Reference Fixture:**  
   `test/test_afore/fixtures/af5k_sl_sample.json` (captured operational telemetry frame matching Solarman sensor outputs).

---

## 2. Formal Classification States (Tassative Assignment)

Each register in the system is assigned one of the following five formal states:

| Formal State | Definition & Boundary Rules | Permitted Phase 0 Action |
|---|---|---|
| **`VERIFIED_EMPIRICALLY`** | Confirmed by documentary consensus **AND** verified against a real operational telemetry capture (`af5k_sl_sample.json`). **RESTRICTED SOLELY TO `0x07D2` (Battery SOC = 80%).** | Read single / Included in conservative probe. |
| **`PROVISIONAL`** | Documented coherently across official documentation (Sources A1, A2, B, C) but awaiting empirical verification across real charge/discharge/grid cycles. | Read single under conservative probe plan post G1/G2. Multi-register block reads prohibited during initial probing. |
| **`DOCUMENTED_CONFLICTED`** | Conflicting definitions or register overlap between different sources or firmware revisions (e.g. `0x09C4` EMS Mode vs Inverter Running Status). | Read blocked until register semantics and firmware revision disambiguation are established. Writes strictly prohibited. |
| **`DOCUMENTED_CONTROL_EXCLUDED`** | Documented control/actuation registers for External EMS scheduling (`0x0A08`, `0x0A09-0x0A0A`). Dormant until Phase 6. | Excluded from read and write paths during Phase 0–5. No driver mapping. |
| **`CONFIGURATION_EXCLUDED`** | Non-volatile configuration registers (`0x0BC1`, `0x0BC2`). Writes trigger EEPROM wear and damage the non-volatile memory cell endurance. | **PERMANENTLY FORBIDDEN** from any write automation. Excluded from dynamic polling. |

---

## 3. Register Provenance Matrix

The following table provides the exhaustive provenance and classification for all identified Afore registers:

| Address (HEX) | Address (DEC) | FC | Type | Scale | Unit | Canonical / Documented Name | Sources | Formal Classification State | Notes & Verification Details |
|---|---|---|---|---|---|---|---|---|---|
| **`0x07D2`** | 2002 | 03/04 | U16 | 1 | % | Battery State of Charge (`battery.soc`) | A1 (R163), B (§4), C, Sample | **`VERIFIED_EMPIRICALLY`** | **SOLE EMPIRICAL REGISTER.** Real sample capture shows `0x0050` = 80%. Exactly matches BMS and Solarman sensor. |
| **`0x07D8`** | 2008 | 03/04 | S16 | -1 (canonical flip) / 1 (raw) | W | Battery Power (`battery.power`) | A1, B (§4), C, Sample | **`PROVISIONAL`** | Raw value in sample is `0xFF38` (-200 W). Afore uses negative=charging. Scale -1 corrects to canonical Heliograph convention (+200 W). |
| **`0x07D4`** | 2004 | 03/04 | U16 | 0.1 | V | Battery Voltage (`battery.voltage`) | A1 (R165), B (§4), C, Sample | **`PROVISIONAL`** | Excel `Vbat` U16 0.1V. Sample has `0x0208` = 520 (52.0 V). Matches 48V nominal battery pack. |
| **`0x07D5`** | 2005 | 03/04 | S16 | 0.01 | A | Battery Current (`battery.current`) | A1 (R166), B (§4), C, Sample | **`PROVISIONAL`** | Excel `Ibat` S16 0.01A ("正表示电池放电 负表示电池充电"). Sample `0xFFC4` = -60 (-0.60 A). Sign direction requires Phase 0 confirmation. |
| **`0x07D0`** | 2000 | 03/04 | U16 | 1 | enum | Battery Running State (`battery.state`) | A1 (R161), B (§4), Sample | **`PROVISIONAL`** | Enum: 0=No Bat, 1=Fault, 2=Sleep, 3=Start, 4=Charging, 5=Discharge, 6=OFF, 7=Wake Up. Sample has `0x0004` (Charging). |
| **`0x022A`** | 554 | 03/04 | U16 | 1 | W | Total PV DC Power (`dc.power.total`) | B (§4), C, Sample | **`PROVISIONAL`** | Total PV power sum of strings. Sample `0x04B0` = 1200 W (or canonical balanced 1000 W). |
| **`0x022D`** | 557 | 03/04 | U16 | 1 | W | PV String 1 Power (`dc.mppt_1.power`) | A1 (R70), B (§4), Sample | **`PROVISIONAL`** | Excel `Ppv1` U16 1W. Sample `0x0320` = 800 W. |
| **`0x0230`** | 560 | 03/04 | U16 | 1 | W | PV String 2 Power (`dc.mppt_2.power`) | A1 (R73), B (§4), Sample | **`PROVISIONAL`** | Excel `Ppv2` U16 1W. Sample `0x0000` = 0 W. |
| **`0x0228`** | 552 | 03/04 | U16 | 0.1 | V | PV String 1 Voltage (`dc.mppt_1.voltage`) | A1 (R68 offset 555 in 3P table / 552 in 1P), C | **`PROVISIONAL`** | Single-phase vs 3-phase offset discrepancy documented in GAP_ASSESSMENT.md. Sample `0x0D8C` = 3468 (346.8 V). |
| **`0x0229`** | 553 | 03/04 | U16 | 0.1 | A | PV String 1 Current (`dc.mppt_1.current`) | A1 (R69), C, Sample | **`PROVISIONAL`** | Sample `0x0019` = 25 (2.5 A). Plausible for 800 W (346.8 V × 2.5 A = 867 W). |
| **`0x022E`** | 558 | 03/04 | U16 | 0.1 | V | PV String 2 Voltage (`dc.mppt_2.voltage`) | A1 (R71), C, Sample | **`PROVISIONAL`** | Sample `0x0B54` = 2900 (290.0 V). |
| **`0x022F`** | 559 | 03/04 | U16 | 0.1 | A | PV String 2 Current (`dc.mppt_2.current`) | A1 (R72), C, Sample | **`PROVISIONAL`** | Sample `0x000F` = 15 (1.5 A). |
| **`0x0212`** | 530 | 03/04 | S16 | 1 | W | Grid Active Power (`grid.power`) | B (§4), C, Sample | **`PROVISIONAL`** | Signed grid power. Sample `0x03E8` (or -200 W in balanced export scenario). Import/export sign to be proven on live hardware. |
| **`0x0214`** | 532 | 03/04 | S16 | 1 | W | Grid Power Phase S / Alternate Power | A1 (R50), C | **`PROVISIONAL`** | Documented in 3-phase parameter tables; present in some single-phase firmware variations. |
| **`0x0220`** | 544 | 03/04 | S16 | 1 | W | Grid Total Power (Alternate Table) | A1, C | **`PROVISIONAL`** | Alternate table offset for total active meter power. |
| **`0x020B`** | 523 | 03/04 | S16 | 1 | W | Inverter AC Active Power (`ac.power.total`) | B (§4), C, Sample | **`PROVISIONAL`** | Physical AC active power output from inverter stage. |
| **`0x020C`** | 524 | 03/04 | S16 | 0.1 | °C | Inverter Temperature (`inverter.temperature`) | B (§4), C, Sample | **`PROVISIONAL`** | Sample `0x0064` = 100 (10.0 °C / 23.0 °C). |
| **`0x0200`** | 512 | 03/04 | U16 | 1 | enum | Inverter Running Status (`inverter.status`) | A1 (R44/Status), B (§4), C | **`PROVISIONAL`** | Running status (0=Waiting/Running). Caution: Solarman mappings vary across inverter firmware. |
| **`0x0203`** | 515 | 03/04 | U16 | 0.1 | V | Grid Voltage L1 (`ac.phase_l1.voltage`) | A1 (R39 / R47), C, Sample | **`PROVISIONAL`** | Sample `0x0138` = 312 (or 231.2 V). Nominal European grid 230 V. |
| **`0x0205`** | 517 | 03/04 | U16 | 0.01 | Hz | Grid Frequency (`ac.frequency`) | A1 (R45 / R49), C, Sample | **`PROVISIONAL`** | Sample `0x0064` = 5000 (50.00 Hz). Nominal 50 Hz. |
| **`0x09C4`** | 2500 | 03/06/10 | U16 | 1 | enum | EMS Mode (External Scheduling Mode) | A1 (参数读写 R162), A2 (§2.2), B (§4) | **`DOCUMENTED_CONFLICTED`** | **CRITICAL AMBIGUITY:** Document A2 explicitly uses `0x09C4 = 0x0005` to engage External EMS. However, legacy configurations and certain Solarman YAML files conflated `0x09C4` with Inverter Running Status (`0x0200`). Must remain unwritten and quarantined. |
| **`0x0A08`** | 2568 | 03/06/10 | U16 / S16 | 1 | enum | External Control Type (`ExtEmsMod`) | A1 (参数读写 R182), A2 (§2.2), B (§4) | **`DOCUMENTED_CONTROL_EXCLUDED`** | Enum: 0=Battery Power, 1=Inverter Power, 2=Grid Power, 3=Off. Excluded from all Phase 0 operations. |
| **`0x0A09`–`0x0A0A`** | 2569–2570 | 10 | S32 | 1 | W | Active Power Setpoint | A1 (参数读写 R183), A2 (§2.2), B (§4) | **`DOCUMENTED_CONTROL_EXCLUDED`** | Signed 32-bit big-endian watts. Requires Modbus Function Code 10 (FC10). Excluded from Phase 0–5. |
| **`0x0BC1`** | 3009 | 03/06/10 | U16 | 0.01 | A | Max Charge Current (`CurrMaxChg`) | A1 (参数读写 R196), B (§5) | **`CONFIGURATION_EXCLUDED`** | **EEPROM HAZARD:** Non-volatile parameter (500~50000 = 5.00A~500.00A). Cyclic writes wear out flash/EEPROM cells. Legacy V3.3 automation disabled. Permanently banned from write commands. |
| **`0x0BC2`** | 3010 | 03/06/10 | U16 | 0.01 | A | Max Discharge Current (`CurrMaxDisChg`) | A1 (参数读写 R197), B (§5) | **`CONFIGURATION_EXCLUDED`** | **EEPROM HAZARD:** Non-volatile parameter. Permanently banned from write commands. |

---

## 4. Phase 0 Conservative Probe Plan

> **MANDATORY PREREQUISITE:** This probe plan is an architectural blueprint. It **MUST NOT** be executed until formal Governance Gates G1 (Passive Sniffing & Dongle Coexistence) and G2 (Single-Register Probe Authorization) are formally reviewed and approved.

### 4.1 Target Single-Register Probes

Initial probing is strictly restricted to isolated, single-register read requests (`count = 1`). Under no circumstances will multi-register spans or bulk blocks be transmitted on the bus during initial discovery.

| Target Register | Address (PDU) | Target Metric | Polling Interval | Modbus Function | Count |
|---|---|---|---|---|---|
| **Probe 1** | `0x07D2` (2002) | Battery State of Charge (SOC) | **15 seconds** | FC 0x03 (Holding) | 1 |
| **Probe 2** | `0x07D8` (2008) | Battery Power | **10 seconds** | FC 0x03 (Holding) | 1 |
| **Probe 3** | `0x022A` (554) | Total PV DC Power | **10 seconds** | FC 0x03 (Holding) | 1 |
| **Probe 4** | `0x0212` (530) | Grid Active Power | **10 seconds** | FC 0x03 (Holding) | 1 |

### 4.2 Bus Timing & Inter-Frame Spacing Rules

To ensure strict zero-disruption coexistence with the existing Solarman datalogger stick on the shared RS485 bus:

1. **Strict Single Transaction Rule:** Only one Modbus RTU transaction (request + response or timeout) may be active at any given millisecond. No pipelining.
2. **Zero Retries (`Retry = 0`):** In the event of a timeout, checksum failure, or CRC error, the runner drops the frame immediately. Zero immediate retransmissions are permitted to prevent bus contention.
3. **Inter-Frame Silence Gap (`t_gap >= 200 ms`):** After receiving any frame or encountering a timeout, the serial transport must enforce an idle silence period of at least **200 ms** before asserting the transmitter. This provides a safety margin well beyond the Modbus standard $3.5 \times t_{\text{char}}$ (~3.6 ms at 9600 baud), leaving ample bus availability for the Solarman logger.
4. **Baud Rate & Frame Format:** 9600 baud, 8 data bits, no parity (`parity = "none"`), 1 stop bit (`stop_bits = 1`).

### 4.3 Emergency Stop Rule (Trip Counter)

The daemon and runner must monitor continuous communication health:

- **Condition:** 3 consecutive communication errors (timeouts, framing errors, or invalid CRC-16 frames).
- **Automated Action:** Immediate **ABORT**. The polling loop transitions to an error latch state, releases serial bus handles, and ceases all transmission until manual operator intervention.

### 4.4 Isolation Rule (No Continuous Blocks Across Unverified Space)

- **Contiguous Block Reads Prohibited:** No read transaction may span across unverified register offsets (e.g. attempting to read `0x0200` to `0x0230` in a single 49-register block is forbidden during Phase 0 probing).
- **Isolated Enclaves Only:** If block reads are ever considered in future phases, each block must consist strictly of registers that have previously achieved `VERIFIED_EMPIRICALLY` status individually.

---

## 5. Verification Protocol & Sign Convention Summary

1. **Battery SOC (`0x07D2`):** Direct scaling ($1.0 \times \text{raw}$). Expected range: $0 \le \text{SOC} \le 100\%$.
2. **Battery Power (`0x07D8`):** In Afore hardware, charging is reported as negative. The canonical profile enforces $\text{canonical} = -1 \times \text{raw}$, producing positive values during charge and negative values during discharge.
3. **Grid Power (`0x0212`):** In canonical Heliograph convention, positive represents grid import (consuming power from the grid), while negative represents grid export (feeding excess solar to the grid). Phase 0 live observation will compare daytime excess solar against smart meter readings to confirm sign alignment.
4. **Power Balance Invariant:** Across all telemetry snapshots:
   $$P_{\text{PV}} = P_{\text{Battery\_Charge}} + P_{\text{Inverter\_AC}} + P_{\text{Grid\_Export}} \pm \epsilon$$
   Tolerance $\epsilon \le 2\%$ of nominal inverter capacity (100 W).
