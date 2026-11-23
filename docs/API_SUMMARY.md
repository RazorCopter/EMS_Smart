# Riepilogo Completo delle API — Afore EMS v5.1 Prototype

**Firmware embedded C++/Arduino per ESP32-S3 (Waveshare RS485/CAN)**  
**Server Web:** ESP32Async/ESPAsyncWebServer 3.12.1 (LGPL-3.0) + AsyncTCP 3.5.0  
**JSON:** ArduinoJson 7.4.3 — tutti i handler sono non-bloccanti e leggono solo uno snapshot `shared_ptr<const DeviceState>`

---

## 1. REST API — `/api/v1/`

Tutte le route sono versionate sotto `/api/v1/`. I cambiamenti breaking creano `/api/v2/`.

### 1.1 Stato e Telemetria (pubbliche)

| Metodo | Path | Descrizione |
|---|---|---|
| `GET` | `/api/v1/status` | Riepilog bridge + device — include metriche AC/DC, energia, stato online/offline, RSSI WiFi, uptime |
| `GET` | `/api/v1/devices` | Lista ID dei dispositivi rilevati/configurati |
| `GET` | `/api/v1/devices/<id>` | Info singolo dispositivo (driver, modello, serial, stato) |
| `GET` | `/api/v1/devices/<id>/measurements` | Tutte le misure per un device specifico |
| `GET` | `/api/v1/devices/<id>/capabilities` | Capabilities del dispositivo (registri read/write supportati) |
| `GET` | `/api/v1/diagnostics` | Diagnostics del bridge (heap, uptime, errori RS485, poll counter) |
| `GET` | `/api/v1/drivers` | Driver registrati + descriptor (utile per wizard discovery) |
| `GET` / `POST` | `/api/v1/discovery` | Ultima report di discovery / avvia scansione bus |
| `GET` | `/api/v1/capture` | Ultima cattura passiva del bus RS485 (hex frames, checksum) |
| `GET` | `/api/v1/capture/driver` | Cattura driver-mode con direzione frame (tx/rx) |
| `GET` | `/api/v1/registers` | Ultimo risultato lettura registri Modbus |

### 1.2 Configurazione

| Metodo | Path | Autenticazione | Descrizione |
|---|---|---|---|
| `GET` | `/api/v1/config` | — | Config senza segreti (password username omessi con boolean indicator) |
| `PATCH` | `/api/v1/config` | ✔ | Modifica configurazione (applica live o richiede reboot) |
| `GET` | `/api/v1/config/backup` | ✔ | Download configurazione completa (`?secrets=true`) |
| `POST` | `/api/v1/config/restore` | ✔ | Applicare backup (`?dry_run=true` per preview) |
| `POST` | `/api/v1/actions/undo-restore` | ✔ | Swap back alla config precedente |

### 1.3 Actions (scritture)

| Metodo | Path | Autenticazione | Descrizione |
|---|---|---|---|
| `POST` | `/api/v1/actions/discover` | ✔ | Avvia discovery bus RS485 (quick/extended) |
| `POST` | `/api/v1/actions/capture` | ✔ | Registra traffico bus (`?mode=passive\|driver&seconds=&frames=`) |
| `POST` | `/api/v1/actions/read-registers` | ✔ | Lettura 1-16 registri Modbus una tantum |
| `POST` | `/api/v1/actions/poll` | ✔ | Forza poll immediato inverter |
| `POST` | `/api/v1/actions/reboot` | ✔ | Reboot ESP32 |
| `POST` | `/api/v1/actions/clear-coredump` | ✔ | Cancella crash dump |
| `POST` | `/api/v1/actions/factory-reset` | ✔ | Erase config NVS |

### 1.4 Commands (scrittura inverter)

| Metodo | Path | Autenticazione | Descrizione |
|---|---|---|---|
| `POST` | `/api/v1/devices/<id>/commands` | ✔ | Invia setpoint per write queue (202 Accepted + request_id) |
| `GET` | `/api/v1/devices/<id>/commands/<request_id>` | — | Stato comando inviato |

### 1.5 Relays e DRM

| Metodo | Path | Autenticazione | Descrizione |
|---|---|---|---|
| `POST` | `/api/v1/relays/<n>` | ✔ | Switch relay board (n=1..6) |
| `POST` | `/api/v1/drm/set` | ✔ | DRM curtailment mode |

### 1.6 OTA e Provisioning

| Metodo | Path | Autenticazione | Descrizione |
|---|---|---|---|
| `POST` | `/api/v1/ota` | ✔ | Firmware upload (`?board=` + `?sha256=`) |
| `POST` | `/api/v1/provision` | — | First-boot setup (WiFi + admin password) |

### 1.7 Event Streaming

| Metodo | Path | Descrizione |
|---|---|---|
| `GET` | `/api/v1/events` | Server-Sent Events (poll count, ~1Hz) |

**Nota:** Max 4 connessioni SSE concurrenti. Se SSE cade, il client polla `/api/v1/status` ogni 5s.

### 1.8 WiFi Scan

| Metodo | Path | Autenticazione | Descrizione |
|---|---|---|---|
| `GET` | `/api/v1/wifi/scan` | ✔ | Reti WiFi disponibili (per settings page) |

### 1.9 Logs (admin)

| Metodo | Path | Autenticazione | Descrizione |
|---|---|---|---|
| `GET` | `/api/v1/logs` | ✔ | Log ring in-memory (quote config values) |

---

## 2. Endpoint Pubblici Non-Versionati

| Path | Descrizione |
|---|---|
| `GET /metrics` | Prometheus metrics (`heliograph_inverter_*`, `heliograph_poll_*`, ecc.) |

---

## 3. Autenticazione

- **HTTP Basic** su HTTP non crittografato (device su LAN fidata)
- Username di default: `admin`; password **deve** essere impostata durante provisioning
- Senza password, tutti gli endpoints mutanti rifiutano con 401
- Le credenziali non appaiono mai in logs, SSE, MQTT o Prometheus

---

## 4. Format Errori (uniforme)

```json
{
  "error": {
    "code": "device_not_found",
    "message": "No device with id 'foo'",
    "request_id": "a3f1"
  }
}
```

| HTTP | Quando |
|---|---|
| 400 | JSON invalido, campo config sconosciuto, valore fuori range |
| 401 | Auth mancante/errata su endpoint protetti |
| 404 | Device o path sconosciuto |
| 409 | Discovery già in corso; bus RS485 occupato |
| 413 | Body > 4KB (8KB su `/config/restore`) |
| 429 | Rate limit — 1 req/s, condiviso su tutti gli write route |
| 503 | Nessun dato valido (cold start) |

**Mai un HTTP 200 con messaggio di errore nel body.**

---

## 5. MQTT Interface

**Client:** espMqttClient 1.7.3 (MIT, non-blocking, QoS 0/1/2, LWT, auto-reconnect)  
MQTT è **opzionale** — se disconnesso, polling, Modbus TCP, REST e web continuano a funzionare.

### 5.1 Topici Publish (bridge → broker)

| Topic | Retained | QoS | Contenuto |
|---|---|---|---|
| `heliograph/<bridge_id>/availability` | ✔ | 1 | `online` / `offline` |
| `heliograph/<bridge_id>/state` | ✔ | 0 | Misure + status (JSON) |
| `heliograph/<bridge_id>/diagnostics` | ✔ | 0 | Diagnostics bridge (JSON) |
| `heliograph/<bridge_id>/identity` | ✔ | 1 | Identity dispositivo (JSON) |
| `heliograph/<bridge_id>/capabilities` | ✔ | 1 | Capabilities (JSON) |

Per dispositivi aggiuntivi (2..N):
| Topic | Retained | QoS | Contenuto |
|---|---|---|---|
| `heliograph/<bridge_id>/device/<device_id>/state` | ✔ | 0 | Misure + status (JSON) |
| `heliograph/<bridge_id>/device/<device_id>/identity` | ✔ | 1 | Identity (JSON) |
| `heliograph/<bridge_id>/device/<device_id>/capabilities` | ✔ | 1 | Capabilities (JSON) |

### 5.2 Topici Subscribe (broker → bridge)

| Topic | Condizioni | Descrizione |
|---|---|---|
| `relay/<n>/set` | `read_only_mode=off` + `relays.enabled` | Comando relay board |
| `drm/set` | `read_only_mode=off` + `relays.enabled` | DRM curtailment |
| `<device>/command/set` | `read_only_mode=on` + `control.inverter_writes_enabled` | Command inverter |

### 5.3 Strategia di Publishing MQTT

- **Publish-on-change** con deadband: power ≥ 5W, voltage ≥ 0.5V, current ≥ 0.1A
- **Refresh periodico** ogni 60s (fixed, compile-time constant)
- `identity` e `capabilities` solo su change o dopo reconnect
- JSON bounded: `JsonDocument` con capacity checking

### 5.4 Home Assistant MQTT Discovery

Prefix `homeassistant/` (configurable). Entities generate **esclusivamente** da `state.measurements` + `capabilities`.

| Measurement | device_class | state_class | Unit |
|---|---|---|---|
| `ac.power.total`, `dc.power.total` | `power` | `measurement` | W |
| `*.voltage` | `voltage` | `measurement` | V |
| `*.current` | `current` | `measurement` | A |
| `ac.frequency` | `frequency` | `measurement` | Hz |
| `inverter.temperature` | `temperature` | `measurement` | °C |
| `energy.today`, `energy.total` | `energy` | `total_increasing` | kWh |
| `wifi_rssi` | `signal_strength` | `measurement` | dBm |

---

## 6. Prometheus `/metrics`

Tutte le metriche: lowercase, snake_case, unità base nel nome, counter terminano in `_total`.

```
heliograph_inverter_ac_power_watts          Gauge - AC output power
heliograph_inverter_online                  Gauge - Inverter status
heliograph_inverter_energy_today_kwh        Counter - Energy today
heliograph_inverter_energy_total_kwh        Counter - Total energy
heliograph_poll_success_total               Counter - Successful polls
heliograph_poll_failure_total               Counter - Failed polls
heliograph_rs485_checksum_errors_total      Counter - CRC errors
heliograph_rs485_timeouts_total             Counter - Timeouts
heliograph_wifi_rssi_dbm                    Gauge - WiFi strength
heliograph_uptime_seconds                   Gauge - Uptime
heliograph_build_info                       Gauge - Version info (labels: version, driver)
```

**Regole:**
- Misure invalidi sono **omesso**, non 0
- Serial number **non è un label** (high cardinality)
- Ogni series ora ha label `device="…"` per multi-inverter

---

## 7. Home Assistant Package

File nella directory `ha_package/` e `integrations/homeassistant/`:
- [`afore_sensors.yaml`](integrations/homeassistant/afore_sensors.yaml) — Entities sensori Modbus
- [`automations.yaml`](integrations/homeassistant/automations.yaml) — Automations HA
- [`lovelace_dashboard.yaml`](integrations/homeassistant/lovelace_dashboard.yaml) — Dashboard Lovelace

---

## 8. Architettura di Sicurezza

| Risorsa | Proprietario | Protezione |
|---|---|---|
| UART1 | `rs485Task` | `busMutex` in `Rs485Transport` |
| `DeviceState` | `StateStore` | mutex + `shared_ptr<const>` snapshot |
| NVS config | `ConfigurationStore` | mutex; writes only from `loopTask` |
| Diagnostics counters | `Diagnostics` | `std::atomic<uint32_t>` |

**Regola fondamentale:** nessun modulo di output vede l'UART. Tutti vedono solo `StateStore`.

---

## 9. Rate Limiting

- **1 req/s** condiviso su tutti gli write-ish route
- Colpisce: `/actions/*`, `POST /devices/<id>/commands`, `POST /relays/<n>`, `POST /drm/set`
- HTTP 429 quando superato

---

## 10. Sommario Conteggio Endpoint

| Categoria | Count |
|---|---|
| REST API (GET) | ~18 endpoints |
| REST API (POST/PATCH) | ~15 endpoints |
| MQTT Topics (publish) | 5 + 3×N (device subtree) |
| MQTT Topics (subscribe) | 3 |
| Prometheus metrics | ~12 metriche |
| HA Discovery entities | Dinamiche da measurements |

**Totale:** ~33 route REST, 8+ topic MQTT, ~12 metriche Prometheus

---

## Riferimenti Documentali

| Documento | Contenuto |
|---|---|
| [`docs/rest-api.md`](docs/rest-api.md) | Spec completa REST API |
| [`docs/mqtt.md`](docs/mqtt.md) | Design MQTT + HA Discovery |
| [`docs/prometheus.md`](docs/prometheus.md) | Metriche Prometheus |
| [`docs/architecture.md`](docs/architecture.md) | Layering, data flow, task model |
| [`docs/security.md`](docs/security.md) | Read-only guard, write capabilities |
| [`docs/modbus-register-map.md`](docs/modbus-register-map.md) | Registri Modbus |
| [`docs/sunspec.md`](docs/sunspec.md) | SunSpec protocol |
| [`docs/sungrow-sh-protocol.md`](docs/sungrow-sh-protocol.md) | SunGrow SH protocol |
| [`docs/eversolar-protocol.md`](docs/eversolar-protocol.md) | EverSolar/Zeversolar protocol |
