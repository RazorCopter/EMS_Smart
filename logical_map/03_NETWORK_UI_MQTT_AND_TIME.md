# 03 — Rete, REST API, Dashboard Web, MQTT/HA e Tempo

---

## Tempo, RTC e Fuso Orario

### `src/network/rtc_time.h` (48 righe) & `rtc_time.cpp` (95 righe)
- **Scopo**: Driver I2C per l'RTC hardware (PCF85063 / DS3231 sulla scheda Waveshare), lettura/scrittura ora UTC con validazione range.

### `src/v51/iana_zones.h` (606 righe)
- **Scopo**: Tabella statica fusi orari IANA → stringhe POSIX `TZ` (es. `Europe/Rome` → `CET-1CEST,M3.5.0,M10.5.0/3`) per calcolo ora legale (DST) offline.

### `src/v51/clock_service.h` (22 righe) & `clock_service.cpp` (290 righe)
- **Scopo**: Servizio orologio unificato (`TimeSource { Unsynced, Rtc, Ntp }`):
  - Sincronizza SNTP quando Wi-Fi è connesso e aggiorna l'RTC hardware.
  - Espone `clockStamp()` (`uptime`, `utc`, `source`) e gli endpoint HTTP `/api/v1/time` (GET/POST per impostazione timezone e orario manuale).

---

## HTTP Server, Wi-Fi, OTA e Diagnostica

### `src/v51/remote.h` (80 righe) & `remote.cpp` (919 righe)
- **Scopo**: Stack di rete Wi-Fi STA/AP, server `esp_http_server` (`httpd`, stack `10240 B`), autenticazione HTTP Basic, ring buffer log (`/api/v1/logs`), OTA firmato ECDSA P-256 (`/api/v1/ota`), e campionamento risorse FreeRTOS (`sampleSystemResources()` / `sampleSystemResourceSummary()`).
- **Endpoint principali**:
  - `GET /api/v1/health` (con `executed` e `applied` reali), `GET /api/v1/system`, `GET /api/v1/telemetry`, `GET /api/v1/logs`
  - `POST /api/v1/ota` (verifica SHA-256 + firma DER ECDSA P-256)
  - `POST /api/v1/wifi`, `POST /api/v1/password`
  - Registra i router di `clock_service`, `ems_runtime`, `m4`, `m5`.

---

## MQTT, Home Assistant e Audit Decisioni

### `src/outputs/mqtt/home_assistant_discovery.h` (85 righe)
- **Scopo**: Helper per costruire i payload JSON di Home Assistant MQTT Discovery.

### `src/v51/m4.h` (18 righe) & `m4.cpp` (1001 righe)
- **Scopo**: Client ESP-MQTT (`v51_mqtt` + `mqtt_task`), pubblicazione telemetria/decisioni, Home Assistant Discovery, e ring buffer storico decisioni.
- **Dettagli chiave**:
  - Sottoscrive `ems/command`, `forecast/solar`, `input/+` prima di pubblicare `availability = online` (su `MQTT_EVENT_SUBSCRIBED` o fallback 2 s).
  - Rifiuta comandi MQTT con flag `RETAIN` (`RETAINED` ACK `accepted: false`).
  - Espone `GET /api/v1/ems/decision` (ultima decisione) e `GET /api/v1/ems/decisions` (NDJSON storico per backfill del Companion).
  - Serve la dashboard su `GET /` tramite `dashboardPage()` / `dashboardSize()`.

### `src/v51/m5.h` (27 righe) & `m5.cpp` (418 righe)
- **Scopo**: Gestione input predittivi Home Assistant, comandi manuali (`operating_mode`, `live_trainer`, `live_sign`, `aux`) e controllo relè `AUX1` (mantenuto `OFF` nell'EMS).
- **Funzioni chiave**:
  - `acceptHaInput(kind, data, size)` & `readPredictiveInputs(nowUtc, deadlineUtc)`: valida e converte i JSON MQTT (`forecast_solar`, `forecast_ratio`, `weather`, `load_profile`, `heartbeat`) in `PredictiveInputs` per `StrategyManager`.
  - `manualCommand(action, payload, origin)`: instrada `PREFLIGHT`/`LIVE`/`SHADOW` verso `requestLiveTrial()` (per `WEB_UI`/`REST_API`, e solo `SHADOW` per `MQTT` come disarmo fail-safe) e `live_trainer` verso `configureLiveTrainer()`.
  - Espone `GET /api/v1/ha/inputs`, `GET/POST /api/v1/manual`.

---

## Web Console Locale

### `src/v51/dashboard.cpp` (7 righe) & `src/v51/dashboard.html` (684 righe)
- **Scopo**: Single-page application locale (HTML+CSS+JS inline, compressa in gzip al build time da `gen_v51_dashboard.py` e servita da `dashboard.cpp`).
- **Funzionalità UI**:
  - Header dinamico (`modeBadge`: `SHADOW` / `LIVE · N SCRITTURE`, LED `FC04`, `MQTT`, `HA`, `NTP`).
  - Tab `Overview` (flusso istantaneo FV/Casa/Batteria/Rete, stato batteria, decisione EMS), `Strategie EMS` (Self Use, Charge First, Target SOC, Maintenance, ecc.), `Batteria e limiti`, `MQTT`, `Sistema` (controllo `SHADOW`/`PREFLIGHT`/`LIVE`, `Trainer`, OTA, risorse task).
