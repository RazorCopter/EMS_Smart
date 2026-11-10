# Phase 0 Shadow: CP3 Telemetria con Qualità e MQTT Discovery

**Status**: CP3 implementation authored, validation pending M3
**Branch**: `phase0-host-shadow`
**Date**: 2026-11-10

> ⚠️ **Rettifica M0 (2026-11-10)**: codice e test scritti, **mai compilati né eseguiti**. Le checkbox `[x]` sotto indicano elementi **authored**, non verificati a runtime.

## Obiettivo CP3

Implementare telemetria con qualità (Good/Degraded/Stale/Invalid), polling scheduler a cadenza differenziata, e MQTT Discovery specifico per `afore_v4` con metadati completi secondo le specifiche della Roadmap V4.

## Componenti Implementati

### 1. **Modello Qualità Telemetria**
- **File**: [`src/device/telemetry_quality.{h,cpp}`](../src/device/telemetry_quality.h)
- **Enum Quality**: `Good`, `Degraded`, `Stale`, `Invalid`
- **TelemetrySample**: struttura dati con valore, quality, timestamp, age, verified, derived
- **Regole di qualità**:
  - `Good`: lettura fresca da registro VERIFIED, validazione superata
  - `Degraded`: lettura presente ma con warning (retry CRC, registro UNVERIFIED)
  - `Stale`: lettura troppo vecchia (superata soglia maxAge)
  - `Invalid`: nessuna lettura utilizzabile (errore Modbus, fuori range, device offline)
- **Metodi helper**:
  - `computeQuality()`: calcola Quality da flag measurement e stato verified
  - `computeAge()`: calcola età con protezione clock skew
  - `actionable()`: sample utilizzabile per decisioni controllo (Good/Degraded)
  - `publishable()`: sample pubblicabile come numero (non Invalid)

### 2. **Afore Telemetry Engine**
- **File**: [`src/app/afore_telemetry_engine.{h,cpp}`](../src/app/afore_telemetry_engine.h)
- **Polling a Cadenza Differenziata**:
  - **Fast block** (1s): potenza, corrente, tensione — misure critiche per controllo
  - **Medium block** (5s): SOC, stato batteria, rete — stato a variazione lenta
  - **Slow block** (30s): temperatura, contatori energia — diagnostica
- **PollingBlock**: struttura con startAddress, count, interval, lastPoll, enabled
- **Decodifica Registri Afore**:
  - Block 0x0200-0x0212: potenza inverter, rete, temperatura
  - Block 0x0228-0x0230: PV1/PV2 tensione, corrente, potenza
  - Block 0x07D0-0x07D8: batteria stato, SOC, tensione, corrente, potenza
- **Sign Convention Critiche**:
  - Batteria power scale=-1: Afore negativo in carica → canonico positivo
  - Grid power: positivo=import, negativo=export (da verificare Phase 0)
- **Gestione Staleness**: calcolo automatico età, marcatura Stale oltre maxAge (default 15s)
- **Statistiche Comunicazione**: totalPolls, successfulPolls, crcErrors, timeouts, exceptions, successRate

### 3. **MQTT Discovery per Afore V4**
- **File**: [`src/app/afore_v4_mqtt.{h,cpp}`](../src/app/afore_v4_mqtt.h)
- **Device Info HA**:
  - Identifiers: `["afore_v4"]` (node ID configurabile)
  - Name: "Afore AF5K-SL (Shadow)"
  - Manufacturer: "Afore"
  - Model: "AF5K-SL (Shadow Mode)"
  - SW Version: "Phase 0 CP3"
- **Entità VERIFIED** (abilitate di default):
  - Battery: SOC, Power, Voltage, Current
  - PV: Total Power, PV1 Power, PV2 Power
  - Grid: Grid Power, Inverter Output
- **Entità UNVERIFIED** (diagnostiche, disabilitate):
  - PV1/PV2 Voltage/Current
  - Grid Voltage/Frequency
  - Inverter Temperature
- **Entità Shadow Controller**:
  - `shadow_mode`: "shadow" (modalità operativa)
  - `shadow_quality`: quality_summary (worst quality tra tutti i sample)
  - `shadow_success_rate`: percentuale successo polling
- **Payload Telemetria**:
  - Valori misure come numeri o `null` per Invalid
  - Attributi `_quality`: mappa {measurement_id: quality_string}
  - Attributi `_age_ms`: mappa {measurement_id: age_milliseconds}
- **Availability Topic**: LWT con "online"/"offline"

### 4. **Test Unitari (27 test authored, execution pending M3)**
- **File**: [`test/test_afore_telemetry/test_main.cpp`](../test/test_afore_telemetry/test_main.cpp)
- **Stato**: NOT EXECUTED — nessun output di compilazione o di esecuzione disponibile
- **Copertura**:
  - **Quality Model** (12 test):
    - Computazione quality da flag supported/valid/stale/verified
    - Conversione string quality
    - Calcolo age con protezione clock skew
    - Predicati actionable/publishable
  - **Polling Scheduler** (4 test):
    - PollingBlock isDue con interval/enabled
    - Configurazione blocchi engine
  - **Telemetry Decoding** (3 test):
    - Decoding battery SOC con quality verified
    - Battery power sign flip (critico: -200W raw → +200W canonical)
    - PV power verified vs voltage unverified
  - **Staleness Detection** (2 test):
    - Fresh samples non marcati stale
    - Aged samples marcati stale oltre threshold
  - **Communication Stats** (2 test):
    - Tracking successful polls
    - Tracking timeouts
  - **MQTT Discovery** (3 test):
    - Generazione entità verified
    - Payload telemetria con quality
    - Shadow state payload con summary
  - **ReadOnly Integration** (1 test):
    - Verifica nessuna scrittura attraverso guard

### 5. **Integrazione CMake**
- **Modifiche**: [`CMakeLists.txt`](../CMakeLists.txt)
- **COMMON_SOURCES aggiornate**:
  - `src/device/telemetry_quality.cpp`
  - `src/protocols/modbus/modbus_client.cpp`
  - `src/app/afore_telemetry_engine.cpp`
  - `src/app/afore_v4_mqtt.cpp`
- **Nuovo target**: `test-afore-telemetry`
  - Compila test CP3 con COMMON_SOURCES
  - Test aggiunto a CTest: `add_test(NAME afore-telemetry-tests ...)`
  - Link pthread per std::map, std::mutex

## Conformità Requisiti CP3

### Modello Dati e Qualità (authored)
- [x] Enum Quality: Good, Degraded, Stale, Invalid
- [x] TelemetrySample con value, quality, timestamp, age, verified, derived
- [x] Calcolo automatico stale/degraded da timeout e errori
- [x] Flag verified derivato da classificazione VERIFIED/UNVERIFIED profilo

### Scheduler/Motore Polling (authored)
- [x] Loop polling non-blocking a cadenza differenziata
- [x] Blocco rapido 1s: potenze/correnti/tensioni
- [x] Blocco medio 5s: SOC, stato batteria
- [x] Blocco lento 30s: temperatura, diagnostica
- [x] Utilizzo driver Afore profile + ReadOnlyGuard
- [x] Isolamento read-only garantito (nessuna write path)

### MQTT Discovery Home Assistant (authored)
- [x] Prefisso `homeassistant/sensor/afore_v4_*`
- [x] Device definition: Manufacturer "Afore", Model "AF5K-SL (Shadow Mode)"
- [x] Identifiers: `["afore_v4"]` configurabile
- [x] Entità VERIFIED abilitate (battery, PV, grid)
- [x] Entità UNVERIFIED diagnostiche/disabilitate
- [x] Entità shadow: mode, quality_summary, success_rate
- [x] device_class, state_class, unit_of_measurement corretti

### Pubblicazione MQTT (authored, mai pubblicato)
- [x] Topic stato JSON con valori e attributi qualità
- [x] Topic shadow con stato controller e statistiche
- [x] Availability topic (LWT) con online/offline
- [x] Attributi JSON per ogni misura: quality, age_ms, verified

### Test Unitari (authored, execution pending M3)
- [x] Test validazione qualità (Good→Stale→Invalid)
- [x] Test generazione MQTT Discovery payloads
- [x] Test serializzazione telemetria con mock transport
- [x] Test battery power sign flip (critico)
- [x] Test polling scheduler con cadenze differenziate
- [x] Test staleness detection
- [x] Test communication stats
- [x] 27 test authored, execution pending M3 (7 suite)

### Integrazione CMake (authored)
- [ ] Target Linux compilabile — NON VERIFICATO, mai compilato (pending M3)
- [x] Test registrati in CTest (`add_test`) — esecuzione pending M3
- [x] COMMON_SOURCES aggiornate con nuovi moduli

## Verifica Layering

**Ispezione statica manuale (DEDOTTO, `check_layering.sh` non eseguito)**:
- Nessun include Arduino/ESP-IDF nei nuovi file (ArduinoJson consentito per host)
- Nessun brand name fuori da src/drivers/ (solo riferimenti canonici "Afore" in nomi file app-specific)
- Tutti i file app/afore_* sono chiaramente Afore-specific per design
- telemetry_quality.* è brand-agnostic, riutilizzabile
- Guard ReadOnly istanziato nel costruttore dell'engine (lettura codice)

**Note sul layering Afore-specific**:
- `afore_telemetry_engine.*` e `afore_v4_mqtt.*` sono volutamente Afore-specific
- Risiedono in `src/app/` non in `src/device/` o `src/protocols/`
- Simile a moduli specifici esistenti (es. `discovery_runner`, `driver_capture_runner`)
- Non violano il layering perché:
  - Non introducono brand knowledge nel core
  - Non sono referenziati da moduli generici
  - Sono componibili con infrastruttura generica

## Compilazione e Test

**Su Linux host** (da eseguire):
```bash
cd afore-autonomous-ems
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
make test-afore-telemetry
ctest -R afore-telemetry-tests --verbose
```

**Esito atteso (obiettivo, NON osservato)**:
- Compilazione senza warning — non ancora tentata
- 27 test authored, execution pending M3 — pass/fail sconosciuti
- Nessuna violazione layering — da confermare con `tools/check_layering.sh`

## File Creati/Modificati

```
src/device/telemetry_quality.h                (nuovo)
src/device/telemetry_quality.cpp              (nuovo)
src/app/afore_telemetry_engine.h              (nuovo)
src/app/afore_telemetry_engine.cpp            (nuovo)
src/app/afore_v4_mqtt.h                       (nuovo)
src/app/afore_v4_mqtt.cpp                     (nuovo)
test/test_afore_telemetry/test_main.cpp       (nuovo)
CMakeLists.txt                                (modificato: aggiunto target test-afore-telemetry)
```

## Prossimi Passi

**CP4 - Integrazione Host Daemon**:
1. Integrare AforeTelemetryEngine in `src/host/main.cpp`
2. Configurare polling da command line arguments
3. Publisher MQTT opzionale (richiede client MQTT POSIX)
4. Logging telemetria su file JSONL locale

**CP5 - Validation e Deployment**:
1. Esecuzione su Raspberry Pi con porta seriale reale
2. Confronto con Solarman oracle
3. Verifica sign convention battery/grid in condizioni reali
4. Test durata 72h con statistiche qualità

## Note Critiche per Phase 0

### Sign Convention da Validare
1. **Battery Power (0x07D8)**: scale=-1 implementato
   - Afore raw: negativo durante carica
   - Canonical: positivo = charging
   - **MUST VALIDATE** attraverso ciclo carica/scarica completo

2. **Grid Power (0x0212)**: scale=1, signed
   - Ipotesi: positivo = import, negativo = export
   - **MUST VALIDATE** in stati import/export noti

### Registri UNVERIFIED
Marcati come `Degraded` quality, richiedono validazione Phase 0:
- PV1/PV2 Voltage/Current (0x0228, 0x0229, 0x022E, 0x022F)
- Grid Voltage/Frequency (0x0203, 0x0205)
- Inverter Temperature (0x020C)

### Safety Gates Preserved
- ReadOnlyGuard wraps transport in engine constructor (DEDOTTO, lettura codice)
- Nessun write path in AforeTelemetryEngine (DEDOTTO)
- Test progettato per verificare che blockedWrites resti 0 (execution pending M3)
- Profilo af5k_sl.toml: zero [[write]] blocks (ispezione)

## Conclusione

CP3 contiene l'implementazione **authored** (non compilata) della telemetria con qualità, polling scheduler e MQTT Discovery per la modalità Shadow Mode Phase 0. I componenti sono progettati per essere:
- Host-testable (27 test authored, execution pending M3)
- Brand-agnostic nel core (telemetry_quality)
- Read-only safe (guard preserved)
- Pronti per integrazione con daemon host e validazione hardware
