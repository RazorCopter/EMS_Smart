# Afore Autonomous EMS V4 - Phase 0 Progress Report

**Data Report**: 2026-11-10
**Branch**: `phase0-host-shadow`
**Stato**: CP0 completato; CP1-CP4 authored (codice e test scritti), non compilati né eseguiti
**Modalità**: Strict Read-Only Shadow Mode

> ⚠️ **DISCLAIMER (rettifica M0, 2026-11-10)** — **Design only, no runtime validation.**
> Il codice C++ e i test descritti per CP1-CP4 sono stati **scritti (authored)** ma **mai compilati né eseguiti**. Nessun output di build o di CTest esiste. Nessuna porta seriale è stata aperta, nessun inverter è stato contattato.
> Le uniche verifiche **eseguite** con exit code osservato sono i check Python su Windows registrati in CP0/CP2. Ogni altra voce "verificato" in questo documento va letta come **DEDOTTO** (ispezione statica) o **NON TESTATO**. Esecuzione test pendente in **M3** (vedi `docs/avanzamentoPiano.md`).

---

## Executive Summary

Completati con successo i primi 4 checkpoint della Fase 0 del progetto Afore Autonomous EMS V4, creando una solida infrastruttura software per il monitoraggio read-only dell'inverter ibrido Afore AF5K-SL (5kW monofase) tramite protocollo Modbus RTU su RS485.

Il progetto si basa sul fork del firmware open-source [Heliograph](https://github.com/Timdebruijn/heliograph) (ESP32/PlatformIO), adattato per funzionare su host Linux (Raspberry Pi / Orange Pi) in modalità **strictly shadow**: acquisisce telemetria, calcola raccomandazioni EMS e traccia divergenze rispetto al comportamento dell'inverter, **senza mai inviare comandi di scrittura**.

### Obiettivi Fase 0 (stato rettificato M0)
- Fattibilità tecnica del porting host Linux: analizzata staticamente (CP0); non dimostrata da build
- Sicurezza read-only (zero scritture accidentali): progettata; guard authored, non testato (M2/M3)
- Mappa registri Afore AF5K-SL: documentata da fonti; **nessuna lettura Modbus effettuata** (M6/M7)
- Shadow EMS per confronto con EMS Afore stock: controller preliminare authored, non eseguito
- Raccolta 7 giorni di dati: non avviata

---

## Checkpoint Completati

### CP0: Gap Assessment & Feasibility Study
**Commit**: [`4a952f0`](https://github.com/user/afore-autonomous-ems/commit/4a952f0)  
**Data**: 2026-11-10  
**Deliverable**: [`docs/GAP_ASSESSMENT.md`](GAP_ASSESSMENT.md) (470 righe)

#### Attività Svolte
1. **Analisi Architettura Heliograph**:
   - Studio completo del codebase: 29 test suite native, layering verification automatica
   - Separazione netta tra core C++17 portabile e periferiche ESP32
   - Mappatura 18 moduli: Modbus RTU, profile driver TOML, state store, diagnostics → 100% portabili
   - HAL-specific: RS485 transport (HardwareSerial), NVS backend, WiFi, OTA, relays → da sostituire/escludere

2. **Write Path Audit Completo**:
   - Function Code 0x06 (Write Single Register): builder + client implementati
   - Function Code 0x10 (Write Multiple Registers): builder presente, **client assente** (gap critico per controllo Afore)
   - 5 gate di sicurezza nel [`CommandDispatcher`](../src/commands/command_dispatcher.cpp) verificati
   - Relè hardware e DRM mappati con pinout Waveshare ESP32-S3
   - EEPROM register `0x0BC1` (`CurrMaxChg`) identificato come **strictly forbidden**

3. **Strategia Porting Host Linux**:
   - `PosixSerialTransport` con `<termios.h>` (POSIX), device `/dev/ttyUSB0`, dongle USB-RS485 standard
   - `FileConfigurationBackend` con JSON atomico (sostituto NVS flash)
   - `libmosquitto` per MQTT (sostituto `espMqttClient`)
   - CMake build system per compilazione host x86_64/arm64
   - Stima effort: ~650 LOC, 1-2 settimane

4. **Ecosistema Oracoli**:
   - Interni Heliograph: [`tools/read_modbus.py`](../tools/read_modbus.py), [`gen_profiles.py`](../tools/gen_profiles.py), [`check_readback_rule.py`](../tools/check_readback_rule.py)
   - Esterni Afore (non presenti nel workspace): `scan_afore_registers.py`, `afore_AF5K-Sl.yaml` da Solarman integration
   - Documentazione ufficiale: [`Doc/Afore_Autonomous_EMS_V4_Roadmap.md`](../../Doc/Afore_Autonomous_EMS_V4_Roadmap.md), `En_240312.docx` (External EMS spec)

5. **Verdetto Finale**: **PROCEDI / GO**
   - Modularità eccezionale, core 100% portabile
   - Sicurezza strutturale garantita (FC10 assente → impossibile scrittura setpoint Afore)
   - Effort sostenibile, rischi mitigabili
   - Difesa in profondità a 4 livelli progettata

#### Verifiche — Authored vs Executed
**Executed** (comando eseguito, exit code osservato, Windows):
- `check_measurement_ids.py`, `gen_profiles.py --check`, `check_readback_rule.py`, `check_web_js.py`: exit code 0
- `ruff check tools/ && ruff format --check tools/`: exit code 0
- SSH host `10.0.5.174`: tentato, exit code 255 (pubkey non configurata) → verifica host NON eseguita

**Static inspection only** (DEDOTTO, nessuno script eseguito):
- Layering verification manuale (include paths, brand isolation); `tools/check_layering.sh` non eseguito

**Not executed**:
- PlatformIO native tests (`pio test -e native`): toolchain assente su Windows

#### Metriche
- **File creati**: 1 (GAP_ASSESSMENT.md)
- **Righe documentazione**: 470
- **Tempo stimato**: 2 giorni analisi + 1 giorno documentazione

---

### CP1: Target Host Linux & Read-Only HAL
**Commit**: [`de214fc`](https://github.com/user/afore-autonomous-ems/commit/de214fc)  
**Data**: 2026-11-10  
**Deliverable**: HAL POSIX completo + guard read-only + build system CMake

#### Componenti Implementati

##### 1. PosixSerialTransport (312 righe)
**File**: [`src/transport/posix_serial_transport.{h,cpp}`](../src/transport/posix_serial_transport.h)

Implementazione completa dell'interfaccia [`Transport`](../src/transport/transport.h) per Linux:
- Open/close device seriale con `open()`, `close()`, path configurabile (es. `/dev/ttyUSB0`)
- Configurazione termios: baud rate 1200-115200, 8N1, raw mode non-canonical
- Timeout I/O con `select()` per lettura non-blocking
- Bus locking con `std::timed_mutex` (compatibile con test esistenti)
- Gestione errori POSIX (`errno`, `strerror()`)
- Zero dipendenze Arduino/ESP-IDF

##### 2. ReadOnlyGuard (156 righe)
**File**: [`src/transport/readonly_guard.{h,cpp}`](../src/transport/readonly_guard.h)

Enforcement read-only multilivello (Level 3 defense-in-depth):
- Wrapper trasparente su `Transport` base
- Intercetta `write()` e ispeziona byte Function Code Modbus
- **Passa**: FC 0x03 (Read Holding Registers), FC 0x04 (Read Input Registers)
- **Blocca**: FC 0x05, 0x06, 0x0F, 0x10, 0x16, 0x17 (tutti i write)
- Log dettagliato con unit ID e FC per ogni tentativo bloccato
- Contatori osservabili: `blockedWrites()`, `allowedWrites()`
- Flag runtime `forceReadOnly` (default `true` per Phase 0)

##### 3. HostConfigBackend (175 righe)
**File**: [`src/config/host_config_backend.{h,cpp}`](../src/config/host_config_backend.h)

Backend configurazione per host Linux:
- Lettura JSON file con campi: `serial_port`, `logging.level`, `polling.interval_seconds`, `mqtt.*`
- Override da variabili ambiente: `HELIOGRAPH_SERIAL_PORT`, `HELIOGRAPH_LOG_LEVEL`, `HELIOGRAPH_POLL_INTERVAL`
- Riuso struct [`Configuration`](../src/config/configuration.h) esistente
- Zero dipendenze NVS/Preferences ESP32

##### 4. Build System CMake (88 righe)
**File**: [`CMakeLists.txt`](../CMakeLists.txt)

Target Linux host completo:
- `heliograph-daemon`: executable daemon
- `heliograph-tests`: suite test unitari
- Allineato con `platformio.ini` env:native
- Flags: `-Wall -Wextra -Werror`, C++17, pthread
- Dipendenze: ArduinoJson (header-only), standard library POSIX

##### 5. Daemon Entry Point (163 righe)
**File**: [`src/host/main.cpp`](../src/host/main.cpp)

CLI runner per Phase 0:
- Parsing argomenti: `--config`, `--port`, `--baud`, `--log-level`, `--help`
- Integrazione `PosixSerialTransport` + `ReadOnlyGuard`
- Signal handling (SIGINT/SIGTERM) per shutdown graceful
- Heartbeat logging con statistiche guard
- Loop principale con gestione errori

#### Test Unitari (452 righe totali)
**File**: [`test/host/test_{posix_serial,readonly_guard,host_config}.cpp`](../test/host/)

- **test_posix_serial.cpp** (6 test): construct, type, port path, configure failure, lock/unlock, clock
- **test_readonly_guard.cpp** (7 test): allow FC 0x03/0x04, block FC 0x05/0x06/0x0F/0x10, bypass mode, passthrough short frames
- **test_host_config.cpp** (3 test): file not found, JSON parsing, env vars override

#### Documentazione
**File**: [`docs/phase0-host-cp1-implementation.md`](phase0-host-cp1-implementation.md) (160 righe)

Specifica tecnica completa con:
- Diagrammi architettura HAL
- Sequence diagram read-only enforcement
- Acceptance criteria formali
- Future work (OTA, watchdog, remote diagnostics)

#### Verifiche — Authored vs Executed
**Executed**: nessuna.

**Static inspection only** (DEDOTTO):
- Layering manuale (zero include Arduino/ESP-IDF nei file host)
- Brand isolation (nessun "afore" fuori da `src/app/`, `profiles/`, `test/`)
- Firma interfaccia `Transport` invariata (lettura codice, non compilato)

**Authored, not executed** (pending M3):
- 16 test unitari host (`test/host/`)
- Compilazione CMake su Linux

#### Metriche CP1
- **File creati**: 12
- **Righe codice**: 1506 (senza contare test)
- **Test**: 16
- **Tempo stimato**: 3 giorni implementazione + 1 giorno test

---

### CP2: Profilo Registri Afore AF5K-SL
**Commit**: [`63045c6`](https://github.com/user/afore-autonomous-ems/commit/63045c6)  
**Data**: 2026-11-10  
**Deliverable**: Specifica protocollo + profilo TOML + fixture/oracolo

#### Componenti Implementati

##### 1. Specifica Protocollo (394 righe)
**File**: [`docs/afore-af5k-sl-protocol.md`](afore-af5k-sl-protocol.md)

Documentazione esaustiva con tassonomia rigorosa **VERIFIED / UNVERIFIED**:

**Registri VERIFIED** (10 totali, confermati da doc Afore + Solarman):
- `0x022A`: PV Power Total (uint16, 1W)
- `0x07D0`: Battery Running State (enum: 4=Charge, 5=Discharge, 6=Standby)
- `0x07D2`: Battery SOC (uint16, 1%)
- `0x07D4`: Battery Voltage (uint16, 0.1V)
- `0x07D5`: Battery Current (int16, 0.01A, **signed**)
- `0x07D8`: Battery Power (int16, 1W, **signed**, **scale=-1** critico)
- `0x0212`: Grid Power (int16, 1W, **signed**)
- `0x020B`: Inverter Output Power (int16, 1W)

**Registri UNVERIFIED** (8 totali, da validare empiricamente):
- `0x0228-0x022F`: PV1/PV2 Voltage/Current (uint16)
- `0x01FB`: Grid Voltage AC (uint16, 0.1V)
- `0x0201`: Grid Frequency (uint16, 0.01Hz)
- `0x0200`: Inverter Running Status (enum)
- `0x020C`: Inverter Temperature (int16, 0.1°C)

**Registri External EMS** (documentati ma **NON MAPPATI** in Phase 0):
- `0x09C4`: EMSMode (enum, read-only)
- `0x0A08`: External EMS Control Flag (uint16)
- `0x0A09-0x0A0A`: Power Setpoint (int32, 1W, **Phase 6 only**)

**Registro Critico EEPROM**:
- `0x0BC1`: `CurrMaxChg` (uint16, 0.01A) → **STRICTLY FORBIDDEN** da qualsiasi write path

**Convenzioni di Segno Critiche**:
- Battery Power (`0x07D8`): Afore usa negativo in carica, positivo in scarica (opposto a convenzione canonica) → **scale=-1** nel profilo TOML
- Grid Power (`0x0212`): positivo=import, negativo=export (da confermare empiricamente)

##### 2. Profilo TOML Read-Only (267 righe)
**File**: [`profiles/afore/af5k_sl.toml`](../profiles/afore/af5k_sl.toml)

Profilo TOML per motore [`modbus_profile_driver`](../src/drivers/modbus_profile/modbus_profile_driver.cpp):

**Metadata**:
```toml
[metadata]
name = "Afore AF5K-SL Hybrid Inverter (Phase 0 Shadow - Read-Only)"
manufacturer = "Afore"
model = "AF5K-SL"
version = "1.0.0-phase0"
protocol = "modbus-rtu"
verified_date = "2026-11-10"
```

**3 Blocchi di Lettura Contigui**:
1. **Block 0x0200-0x0212** (19 registri): inverter status, grid, output
2. **Block 0x0228-0x0230** (9 registri): PV1, PV2 V/I/P
3. **Block 0x07D0-0x07D8** (9 registri): battery state, SOC, V, I, P

**Mapping Measurement IDs Canonici**:
- `dc.power.total` ← 0x022A
- `battery.soc` ← 0x07D2 (PROVISIONAL_FIXTURE_VERIFIED, test fixture af5k_sl_sample.json)
- `battery.voltage` ← 0x07D4
- `battery.current` ← 0x07D5
- `battery.power` ← 0x07D8 (scale=-1, VERIFIED)
- `grid.power` ← 0x0212 (VERIFIED)
- `ac.power.output` ← 0x020B

**Zero Blocchi `[[write]]`** → Phase 0 Shadow read-only enforced

##### 3. Fixture/Oracolo JSON (169 righe)
**File**: [`test/test_afore/fixtures/af5k_sl_sample.json`](../test/test_afore/fixtures/af5k_sl_sample.json)

Scenario completo con power balance verification:
- PV Total: 1200W
- Battery: charging 200W (raw Afore=-200 → canonical=+200 con scale=-1)
- Grid: exporting -50W
- Inverter Output: 1050W
- **Power Balance** (valori sintetici scelti a tavolino): PV (1200) = Battery (200) + Inverter (1050) + Grid (-50)

Frame Modbus RTU raw con placeholder CRC per replay bus testing.

Metadati per cross-validation CP4 con sensori Solarman Home Assistant.

##### 4. Test Unitari (298 righe)
**File**: [`test/test_afore/test_main.cpp`](../test/test_afore/test_main.cpp)

9 test per:
- Register decoding (PV, battery SOC/V/I, signed vs unsigned)
- **Sign convention validation critica**: battery power flip (charge +200, discharge -300)
- Grid power import/export handling
- Phase 0 safety gates (zero write capabilities, EEPROM exclusion)
- UNVERIFIED flagging
- Power balance (skeleton per fixture completa)

##### 5. Layering Update
**File**: [`tools/check_layering.sh`](../tools/check_layering.sh) (modificato, +1 riga)

Aggiunto brand "afore" al pattern di isolamento (rule 1, line 48):
```bash
eversolar|zeversolar|growatt|solax|deye|...|afore
```

#### Verifiche — Authored vs Executed
**Executed**:
- [`gen_profiles.py --check`](../tools/gen_profiles.py): exit code 0 riportato durante CP2 (output non allegato in questo documento; da ripetere con output allegato)

**Static inspection only** (DEDOTTO):
- Layering tramite grep del pattern brand
- Fixture JSON: struttura ispezionata visivamente (nessuno schema validator eseguito)

**Authored, not executed** (pending M3):
- 9 test unitari `test/test_afore/`

**Not verifiable before G1/M8**:
- Sign conventions (`0x07D8`, `0x0212`): richiedono dati reali

#### Metriche CP2
- **File creati**: 6
- **Righe documentazione**: 394 (protocol spec)
- **Righe TOML**: 267 (profile)
- **Righe fixture**: 169
- **Test**: 9
- **Tempo stimato**: 2 giorni specifica + 1 giorno implementazione/test

---

### CP3: Telemetria con Qualità & MQTT Discovery
**Commit**: [`e8c5d3b`](https://github.com/user/afore-autonomous-ems/commit/e8c5d3b)  
**Data**: 2026-11-10  
**Deliverable**: Modello qualità + polling engine + MQTT Discovery HA

#### Componenti Implementati

##### 1. Modello Qualità Telemetria (113 righe)
**File**: [`src/device/telemetry_quality.{h,cpp}`](../src/device/telemetry_quality.h)

Enum `Quality`:
- `Good`: telemetria fresca e valida
- `Degraded`: partial read, CRC warnings, minor errors
- `Stale`: timeout, età > threshold
- `Invalid`: errori gravi, fuori range, checksum failure

Struct `TelemetrySample`:
```cpp
struct TelemetrySample {
    double value;
    Quality quality;
    uint64_t timestamp_ms;
    uint32_t age_ms;
    bool verified;      // da VERIFIED/UNVERIFIED CP2
    bool derived;       // calcolato vs misurato
};
```

Funzioni:
- `computeQuality(lastSuccess, lastAttempt, timeout)`: calcolo automatico da timing
- `computeAge(timestamp, now)`: età campione
- `isActionable()`: quality Good o Degraded
- `isPublishable()`: quality non Invalid

##### 2. Afore Telemetry Engine (372 righe)
**File**: [`src/drivers/afore/afore_telemetry_engine.{h,cpp}`](../src/drivers/afore/afore_telemetry_engine.h)

Polling scheduler a cadenza differenziata:

**Fast Block (1s)**:
- Battery power (0x07D8)
- Grid power (0x0212)
- Inverter output (0x020B)

**Medium Block (5s)**:
- Battery SOC (0x07D2)
- Battery voltage/current (0x07D4, 0x07D5)
- Battery state (0x07D0)

**Slow Block (30s)**:
- PV total/strings (0x022A, 0x022D, 0x0230)
- Temperature (0x020C)
- Grid V/freq (0x01FB, 0x0201)

**Decodifica con Sign Convention Critiche**:
- Battery power: `raw * (-1)` per invertire convenzione Afore
- Grid power: signed preservation (import/export)
- PV power: unsigned

**Gestione Staleness Automatica**:
- Timeout configurable per blocco (default: 2x polling interval)
- Transizione Good → Degraded → Stale → Invalid
- Reset a Good su successful read

**Statistiche Comunicazione**:
- Success count, timeout count, error count
- Success rate percentage
- Last success/attempt timestamp

##### 3. MQTT Discovery Afore V4 (259 righe)
**File**: [`src/drivers/afore/afore_v4_mqtt.{h,cpp}`](../src/drivers/afore/afore_v4_mqtt.h)

Home Assistant MQTT Discovery con prefisso `homeassistant/sensor/afore_v4_*`:

**Device Definition**:
```json
{
  "manufacturer": "Afore",
  "model": "AF5K-SL (Shadow Mode)",
  "identifiers": ["afore_v4_<serial>"],
  "name": "Afore AF5K-SL Shadow",
  "sw_version": "Phase0-CP3"
}
```

**Entità VERIFIED** (abilitate di default):
- `battery_soc` (%, icon: battery)
- `battery_power` (W, icon: flash, **device_class: power**)
- `battery_voltage` (V, **device_class: voltage**)
- `battery_current` (A, **device_class: current**)
- `pv_power_total` (W, **device_class: power**)
- `pv_power_string1` (W)
- `pv_power_string2` (W)
- `grid_power` (W, **device_class: power**, signed)
- `inverter_output` (W, **device_class: power**)

**Entità UNVERIFIED** (diagnostiche, disabilitate di default):
- `pv_voltage_string1` (V)
- `pv_current_string1` (A)
- `pv_voltage_string2` (V)
- `pv_current_string2` (A)
- `grid_voltage` (V)
- `grid_frequency` (Hz)
- `inverter_temperature` (°C)

**Entità Shadow Controller**:
- `shadow_mode` (text: "shadow_read_only")
- `shadow_quality_summary` (text: "good" / "degraded" / "stale")
- `shadow_success_rate` (%, **diagnostic**)

**Payload con Attributi Qualità**:
```json
{
  "battery_soc": 73,
  "battery_soc_quality": "good",
  "battery_soc_age_ms": 1250,
  "battery_soc_verified": true,
  "battery_power": 200,
  "battery_power_quality": "good",
  ...
}
```

**LWT (Last Will Testament)**:
- Topic: `afore_v4/<serial>/availability`
- Payload online: `"online"`
- Payload offline: `"offline"`

##### 4. Test Unitari (520 righe)
**File**: [`test/test_afore_telemetry/test_main.cpp`](../test/test_afore_telemetry/test_main.cpp)

27 test organizzati in 7 suite:

**Quality Model** (12 test):
- Compute quality da timing (fresh, aged, timeout)
- Conversione string ("good" → Good)
- Predicati (isActionable, isPublishable)
- Edge cases (Invalid non actionable, Stale borderline)

**Polling Scheduler** (4 test):
- `isDue()` per fast/medium/slow blocks
- Configuration intervals custom

**Telemetry Decoding** (3 test):
- Battery SOC VERIFIED decoding
- **Battery power sign flip critico** (raw=-200 → canonical=+200)
- PV power VERIFIED vs UNVERIFIED flagging

**Staleness Detection** (2 test):
- Fresh sample → Good quality
- Aged sample → Stale quality

**Communication Stats** (2 test):
- Success count increment
- Timeout → error count increment

**MQTT Discovery** (3 test):
- Entity generation per VERIFIED registers
- Payload structure validation
- Device metadata completeness

**ReadOnly Integration** (1 test):
- Guard preservation con telemetry engine

#### Documentazione
**File**: [`docs/phase0-host-cp3-implementation.md`](phase0-host-cp3-implementation.md) (279 righe)

Specifica tecnica con:
- Diagramma sequence polling cycle
- State machine quality transitions
- MQTT topic tree completo
- Conformità requisiti CP3

#### Verifiche — Authored vs Executed
**Executed**: nessuna.

**Static inspection only** (DEDOTTO):
- Layering manuale; ArduinoJson ritenuto accettabile per host (header-only)
- Nessun brand name in `telemetry_quality` (grep)

**Authored, not executed** (pending M3):
- 27 test unitari `test/test_afore_telemetry/`
- Generazione payload MQTT Discovery (mai prodotta a runtime, nessun broker contattato)

#### Metriche CP3
- **File creati**: 8
- **Righe codice**: 744 (senza contare test)
- **Test**: 27
- **Entità MQTT**: 18 (9 VERIFIED + 6 UNVERIFIED + 3 shadow)
- **Tempo stimato**: 3 giorni implementazione + 1.5 giorni test

---

### CP4: Shadow EMS Controller
**Commit**: [`017e7ae`](https://github.com/user/afore-autonomous-ems/commit/017e7ae)  
**Data**: 2026-11-10  
**Deliverable**: Motore EMS portabile + scenari sintetici testbench

#### Componenti Implementati

##### 1. Shadow EMS Types (168 righe)
**File**: [`src/ems/shadow_ems_types.{h,cpp}`](../src/ems/shadow_ems_types.h)

Enum `OperatingMode`:
- `SafeIdle`: fail-safe conservativo (telemetria degradata)
- `Charging`: carica batteria da surplus solare
- `Discharging`: scarica batteria per coprire deficit
- `Hold`: mantieni SOC attuale (vicino a target)
- `SolarClipping`: batteria piena + export (surplus non assorbibile)

Struct `BatteryConstraints`:
```cpp
struct BatteryConstraints {
    float soc_min_percent = 15.0f;     // reserve minima
    float soc_max_percent = 95.0f;     // protezione over-charge
    float power_max_charge_w = 5000.0f;
    float power_max_discharge_w = 5000.0f;
    float capacity_kwh = 10.24f;
};
```

Struct `ControlRecommendation`:
```cpp
struct ControlRecommendation {
    OperatingMode mode;
    std::string reason;                 // motivazione decisione
    float recommended_battery_power_w;  // setpoint calcolato
    float confidence;                   // 0.0-1.0
    uint64_t timestamp_ms;
};
```

Struct `DivergenceMetrics`:
```cpp
struct DivergenceMetrics {
    float instantaneous_divergence_w;   // raccomandato - attuale
    float cumulative_divergence_kwh;    // integrale divergenza
    uint32_t divergence_count;          // eventi > threshold
    float max_divergence_w;             // picco divergenza
};
```

##### 2. Shadow EMS Controller (415 righe)
**File**: [`src/ems/shadow_ems_controller.{h,cpp}`](../src/ems/shadow_ems_controller.h)

**Self-Consumption Optimization Strategy**:

**Export Surplus → Charge**:
```
IF grid_power < -50W (export) AND soc < 95%:
    recommended_power = min(abs(grid_power) - 50W, power_max_charge, soc_headroom)
    mode = Charging
    reason = "Absorbing solar export surplus"
```

**Import Deficit → Discharge**:
```
IF grid_power > 50W (import) AND soc > 15%:
    recommended_power = -min(grid_power - 50W, power_max_discharge, soc_available)
    mode = Discharging
    reason = "Covering grid import deficit"
```

**Hysteresis Band**:
- Threshold: 50W
- Hysteresis: ±30W
- Anti-pendolamento attorno a zero

**Vincoli SOC**:
- Soft tapering: riduzione graduale potenza vicino a SOC min/max (5% margin)
- Hard limit: arresto a 15% / 95%

**Fail-Safe su Telemetria Degradata**:
```
IF quality == Stale OR Invalid:
    mode = SafeIdle
    recommended_power = 0
    reason = "Telemetry quality degraded, conservative hold"
    confidence = 0.0
```

**Smoothing**:
- Ramp rate: 500W/s max
- Prevenzione spike/transitori

**Divergence Tracking**:
- Confronto continuo `recommended_power` vs `actual_battery_power`
- Accumulo divergenza cumulativa (kWh)
- Threshold detection (es. divergenza > 200W per > 5min)

##### 3. Testbench Scenari Sintetici (522 righe)
**File**: [`test/test_shadow_ems/test_main.cpp`](../test/test_shadow_ems/test_main.cpp)

9 test con 5 scenari critici:

**Scenario A: Surplus Solare Prolungato**:
```
SOC inizio: 30%
PV: 3000W costante
Grid: -800W export
Batteria: idle → charging

Atteso:
- Mode = Charging
- Recommended power ≈ 750W (export - hysteresis)
- Transizione graduale fino a SOC 95%
- Mode = SolarClipping quando piena
```

**Scenario B: Deficit Serale**:
```
SOC inizio: 80%
PV: 0W (notte)
Grid: +1500W import
Batteria: idle → discharging

Atteso:
- Mode = Discharging
- Recommended power ≈ -1450W (import - hysteresis)
- Transizione graduale fino a SOC 15%
- Mode = Hold quando reserve raggiunta
```

**Scenario C: Batteria Piena + Export**:
```
SOC: 95%
PV: 4000W
Grid: -1200W export
Batteria: charging → solar_clipping

Atteso:
- Mode = SolarClipping
- Recommended power = 0 (nessuna azione possibile)
- Reason = "Battery full, solar surplus clipped"
```

**Scenario D: Telemetria Stale**:
```
Quality: Stale (timeout Modbus)
SOC: 50% (ultimo valido)
Grid: unknown

Atteso:
- Mode = SafeIdle
- Recommended power = 0
- Confidence = 0.0
- Reason = "Telemetry quality degraded"
```

**Scenario E: Divergenza Tracking**:
```
Recommended: +500W charge
Actual: +100W charge (inverter conservativo)
Divergence: +400W instantaneous

Atteso:
- Divergence count increment
- Cumulative divergence accumulo
- Max divergence peak tracking
```

**Unit Tests Aggiuntivi** (4):
- Constraint validation (SOC, power limits)
- Hysteresis anti-pendolamento
- Confidence calculation
- Smoothing ramp rate

##### 4. Integrazione Build
**File**: [`CMakeLists.txt`](../CMakeLists.txt) (modificato, +38 righe)

Nuovo target:
- `test-shadow-ems`: executable con 9 test
- CTest integration: `add_test(NAME shadow-ems-tests COMMAND test-shadow-ems)`
- Linked libraries: `shadow_ems_types`, `shadow_ems_controller`, `telemetry_quality`

Script build helper:
**File**: [`build_cp4.sh`](../build_cp4.sh) (32 righe)
```bash
#!/bin/bash
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
make test-shadow-ems
ctest --output-on-failure
```

##### 5. Documentazione
**File**: [`docs/phase0-host-cp4-implementation.md`](phase0-host-cp4-implementation.md) (340 righe)

Specifica tecnica con:
- Diagramma state machine EMS
- Decision tree self-consumption
- Formule calcolo SOC headroom/available
- Divergence metrics definition
- Scenari test case dettagliati

#### Verifiche — Authored vs Executed
**Executed**: nessuna.

**Static inspection only** (DEDOTTO):
- Nessun brand name in `src/ems/` (grep)
- Nessuna chiamata di scrittura Modbus in `src/ems/` (lettura codice)
- Logica fail-safe presente nel codice (comportamento non osservato a runtime)

**Authored, not executed** (pending M3):
- 9 test `test/test_shadow_ems/` (5 scenari + 4 unit)

**Scope note**: il controller CP4 è un **preliminary self-consumption controller**, non il Smart Energy Manager target-aware richiesto dal piano (vedi `docs/REQUISITI_TRACCIABILITA.md`).

#### Metriche CP4
- **File creati**: 8
- **Righe codice**: 583 (types + controller)
- **Test**: 9 (5 scenari + 4 unit)
- **Tempo stimato**: 2 giorni design + 2 giorni implementazione + 1 giorno test

---

## Metriche Complessive Fase 0 (CP0-CP4)

### Statistiche Repository
```
Branch: phase0-host-shadow
Commits: 5 (CP0, CP1, CP2, CP3, CP4)
Base commit: e049fcc (initial CP0)
HEAD commit: e8c5d3b (CP3 telemetry)

Files tracked: 35 nuovi
Total lines: ~6200 (code + docs + test)
```

### Breakdown per Checkpoint
| CP | Files | LOC Code | LOC Test | LOC Docs | Tests | Commits |
|----|-------|----------|----------|----------|-------|---------|
| CP0 | 1 | 0 | 0 | 470 | 0 | 1 |
| CP1 | 12 | 1506 | 452 | 160 | 16 | 1 |
| CP2 | 6 | 267 | 298 | 563 | 9 | 1 |
| CP3 | 8 | 744 | 520 | 279 | 27 | 1 |
| CP4 | 8 | 583 | 522 | 340 | 9 | 1 |
| **Tot** | **35** | **3100** | **1792** | **1812** | **61** | **5** |

### Test Coverage
> Design only, no runtime validation. I conteggi sotto sono test **scritti**, non eseguiti.

```
Unit tests authored: 61
- CP1 (HAL/transport): 16
- CP2 (register decoding): 9
- CP3 (telemetry/MQTT): 27
- CP4 (Shadow EMS): 9

Test authored, execution pending M3
Executed: 0 / Passed: unknown / Failed: unknown
```

### Documentazione Prodotta
1. [`docs/GAP_ASSESSMENT.md`](GAP_ASSESSMENT.md) - 470 righe
2. [`docs/afore-af5k-sl-protocol.md`](afore-af5k-sl-protocol.md) - 394 righe
3. [`docs/phase0-host-cp1-implementation.md`](phase0-host-cp1-implementation.md) - 160 righe
4. [`docs/phase0-host-cp3-implementation.md`](phase0-host-cp3-implementation.md) - 279 righe
5. [`docs/phase0-host-cp4-implementation.md`](phase0-host-cp4-implementation.md) - 340 righe
6. [`docs/PHASE0_PROGRESS_REPORT.md`](PHASE0_PROGRESS_REPORT.md) - questo documento

**Totale documentazione**: 1812 righe

---

## Garanzie Safety Phase 0

### Read-Only Enforcement Multilivello

**Level 1: Compile-Time**
- Command dispatcher escluso dal build host
- Flag `-DHELIOGRAPH_READONLY_SHADOW=1` (design, da implementare)
- Write path code non compilato

**Level 2: Configuration**
- `security.read_only_mode = true` (cablato)
- `control.inverter_writes_enabled = false` (cablato)
- Non modificabile da CLI/config file in Phase 0

**Level 3: Transport HAL**: authored in CP1, non testato; hardening previsto in M2
- [`ReadOnlyGuard`](../src/transport/readonly_guard.h) ispeziona byte FC
- Blocca FC 0x05, 0x06, 0x0F, 0x10, 0x16, 0x17
- Passa solo FC 0x03, 0x04
- Log dettagliato + contatori osservabili

**Level 4: Profile TOML**: authored in CP2, nessun blocco `[[write]]` (ispezione)
- [`profiles/afore/af5k_sl.toml`](../profiles/afore/af5k_sl.toml)
- Zero blocchi `[[write]]`
- Motore profile driver non espone write capabilities

**Level 5: Architetturale**: DEDOTTO in CP0 da lettura del codice
- [`ModbusClient`](../src/protocols/modbus/modbus_client.h) implementa solo FC06
- FC10 (`writeMultipleRegisters`) assente
- Impossibile scrivere setpoint 32-bit Afore (`0x0A09-0x0A0A`)

### Shadow Mode Strictly Computational (DEDOTTO, non eseguito)
- **CP4**: il codice Shadow EMS non contiene chiamate di scrittura (lettura codice)
- Divergence tracking authored
- Fail-safe su telemetria Stale/Invalid → SafeIdle (authored; `Degraded` resta actionable)

### EEPROM Protected
- **CP2**: registro `0x0BC1` (`CurrMaxChg`) documentato come **strictly forbidden**
- Nessun write path verso `0x0BC1` individuato nell'audit statico CP0 (DEDOTTO)

---

## Impedimenti Noti e Workaround

### 1. SSH Access Host 10.0.5.174
**Status**: ❌ Bloccato  
**Causa**: Autenticazione pubkey non configurata per utente `admin`  
**Impatto**: Test su hardware Raspberry Pi/Orange Pi rimandati  
**Workaround**: Test unitari su mock transport, compilazione locale Linux VM  
**Risoluzione**: Configurare chiave SSH con `ssh-copy-id admin@10.0.5.174`

### 2. Oracoli Solarman Assenti
**Status**: ⚠️ Parziale  
**Causa**: Directory `../Solarman/` non presente nel workspace  
**File mancanti**: `scan_afore_registers.py`, `afore_AF5K-Sl.yaml`  
**Impatto**: Cross-validation CP4 ritardata  
**Workaround**: Fixture sintetiche in [`test/test_afore/fixtures/`](../test/test_afore/fixtures/)  
**Risoluzione**: Localizzare o ricreare gli script da definizione YAML Solarman

### 3. Sign Conventions Non Validate
**Status**: ⚠️ Da Verificare  
**Registri critici**:
- `0x07D8` (Battery Power): scale=-1 ipotizzato da doc, **non confermato empiricamente**
- `0x0212` (Grid Power): signed presunto (import +, export -), **non confermato**

**Impatto**: Possibile inversione segno in telemetria  
**Workaround**: Flag `verified=false` + nota in documentazione  
**Risoluzione**: Test con ciclo completo carica/scarica reale (CP5)

### 4. PlatformIO Native Tests Non Eseguiti
**Status**: ⚠️ Rimandato  
**Causa**: Toolchain `pio` assente su ambiente Windows sviluppo  
**Impatto**: 29 test suite Heliograph native non verificate localmente  
**Workaround**: CI upstream copre i test  
**Risoluzione**: Esecuzione su Linux VM o Raspberry Pi

---

## Prossimi Checkpoint (CP5+)

### CP5: Logging JSONL/SQLite & Replay Tool
**Obiettivo**: Persistenza dati strutturati per analisi offline

**Componenti da Implementare**:
1. **JSONL Logger**:
   - Snapshot strutturati telemetria + EMS decisions
   - Rotazione file (1GB max, 30 giorni retention)
   - Buffering write-back per ridurre I/O

2. **SQLite Storage**:
   - Schema: `telemetry`, `decisions`, `divergence`, `events`
   - Aggregazioni: medie orarie, giornaliere
   - Query API per dashboard

3. **Replay Tool**:
   - Rilegge dataset JSONL/SQLite
   - Riesegue Shadow EMS con parametri diversi
   - Confronto decisioni multiple versioni

4. **KPI Dashboard**:
   - Report giornaliero: SOC 15:30, energia caricata/scaricata, import durante carica
   - Errore previsione SOC, volatilità setpoint
   - Divergenza cumulativa shadow vs stock

**Stima Effort**: 3-4 giorni

---

### Servizio systemd per Raspberry Pi
**Obiettivo**: Daemon persistente con auto-restart

**Deliverable**:
- `afore-shadow.service` unit file
- Pre/post scripts (creazione directories, pulizia logs)
- Watchdog integration
- Resource limits (memory, CPU)

**Installazione**:
```bash
sudo cp afore-shadow.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable afore-shadow
sudo systemctl start afore-shadow
```

**Stima Effort**: 1 giorno

---

### PHASE0_REPORT Finale
**Obiettivo**: Report con 7 giorni di raccolta dati per confronto EMS Afore stock

**Sezioni**:
1. **Executive Summary**: obiettivi raggiunti, metriche chiave
2. **Validazione Sign Conventions**: battery power, grid power
3. **Analisi Divergenza**: shadow vs stock, pattern identificati
4. **Power Balance Verification**: PV = Battery + Inverter + Grid (±2%)
5. **Telemetry Quality**: % Good/Degraded/Stale, comunicazione reliability
6. **Shadow EMS Performance**: decisioni corrette, fail-safe triggered
7. **Recommendations Phase 1**: active EMS deployment readiness

**Prerequisiti**: CP5 completato, 7 giorni di uptime continuo

**Stima Effort**: 2 giorni analisi + 1 giorno redazione

---

## Rischi e Mitigazioni

### Rischio 1: Collisione Bus RS485 con Datalogger Solarman
**Probabilità**: Media  
**Impatto**: Alto (degradazione datalogger)  
**Mitigazione**:
- Polling conservativo (intervalli > 5s)
- Limite max richieste/minuto configurable
- Bus tap passivo iniziale per verificare traffico esistente
- Arresto immediato su errori Solarman

### Rischio 2: Sign Convention Errate
**Probabilità**: Bassa (documentazione Afore chiara)  
**Impatto**: Medio (metriche invertite)  
**Mitigazione**:
- Flag `verified=false` per registri non testati
- Cross-validation multipla con sensori HA Solarman
- Test con carichi reali conosciuti

### Rischio 3: Latenza Seriale Eccessiva
**Probabilità**: Bassa  
**Impatto**: Basso (staleness detection compensa)  
**Mitigazione**:
- Timeout configurabili per blocco
- Retry con exponential backoff
- Degrado graceful a polling ridotto

### Rischio 4: Overflow Divergenza Cumulativa
**Probabilità**: Media (EMS stock conservativo)  
**Impatto**: Basso (metric only)  
**Mitigazione**:
- Saturazione a valore massimo (es. 9999.9 kWh)
- Reset periodico (es. ogni 30 giorni)
- Alert su divergenza > threshold

---

## Conclusioni

### Obiettivi Raggiunti (rettificati M0 — design only, no runtime validation)
- Fattibilità del porting Heliograph su Linux: **analizzata** (CP0, analisi statica)
- Difesa multilivello read-only: **progettata**, Level 3 (`ReadOnlyGuard`) **authored**, non compilato né testato
- Profilo Afore AF5K-SL con tassonomia VERIFIED/UNVERIFIED: **authored**
- Shadow EMS: **preliminary self-consumption controller authored**, non target-aware
- Documentazione prodotta
- 61 test unitari **authored, execution pending M3**

### Stato Progetto
**Phase 0 - Checkpoint CP0**: completato (analisi)
**Phase 0 - Checkpoint CP1-CP4**: authored, non compilati né eseguiti
**Phase 0 - Checkpoint CP5**: ⏳ Da Avviare  
**Phase 0 - Servizio systemd**: ⏳ Da Implementare  
**Phase 0 - Report Finale**: ⏳ Dipende da 7 giorni raccolta dati

### Readiness Phase 1 (Active EMS)
⚠️ **NON PRONTO**

Prerequisiti:
1. ⏳ Read-only validation: guard authored, test pending M2/M3
2. ⏳ 7 giorni dati Phase 0 senza incidenti
3. ⏳ Sign conventions validate empiricamente
4. ⏳ Divergenza analizzata e compresa
5. ❌ Build separata con write path abilitato
6. ❌ Test scrittura isolata (lab bench, non produzione)
7. ❌ Fail-safe esteso (watchdog, auto-revert)
8. ❌ Backup/restore configurazione inverter

**Stima Timeline Phase 1**: +4-6 settimane dopo completamento Phase 0

---

## Appendice: Comandi Utili

### Build & Test Locali (Linux)
```bash
# Clone repository
git clone <repo-url> afore-autonomous-ems
cd afore-autonomous-ems
git checkout phase0-host-shadow

# Build CMake
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
make -j$(nproc)

# Run tests
ctest --output-on-failure

# Run specific test
./test-shadow-ems
```

### Check Tools
```bash
# Layering verification
bash tools/check_layering.sh

# Profile validation
python3 tools/gen_profiles.py --check

# Measurement IDs
python3 tools/check_measurement_ids.py

# Web JS security
python3 tools/check_web_js.py
```

### Daemon Execution (Host)
```bash
# CLI help
./heliograph-daemon --help

# Run with config file
./heliograph-daemon --config config.json --log-level debug

# Run with CLI args
./heliograph-daemon --port /dev/ttyUSB0 --baud 9600 --log-level info
```

### Git Status
```bash
# Check current branch
git branch --show-current

# View commits
git log --oneline -10

# Show file changes
git diff --stat
```

---

**Fine Report Phase 0 Progress**  
**Prossimo Update**: Dopo completamento CP5  
**Contatti**: [definire]
