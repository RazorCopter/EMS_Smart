# Mappatura MQTT & Home Assistant · Afore EMS Companion

## Topic ingeriti (sola sottoscrizione)

| Topic | Frequenza tipica | Destinazione DB | Note |
|---|---|---|---|
| `afore/afore-ems/v1/availability` | LWT / connect | `system_health` (`esp32`), `ems_events` | `"online"` / `"offline"` |
| `afore/afore-ems/v1/telemetry` | 5 s | `telemetry_samples`, `energy_samples`, `system_health` | Canali nullable con `quality` e `age_ms` |
| `afore/afore-ems/v1/ems/state` | 5 s (retained) | `ems_decisions` | Stato SHADOW corrente |
| `afore/afore-ems/v1/ems/decision` | 5 s | `ems_decisions`, `ems_strategy_intervals`, `ems_events` | `executed=false`, `applied=null`, `modbus_write_count=0` |
| `afore/afore-ems/v1/ems/ack` | Su comando | `aux_events`, `ems_events` | Conferma comandi SHADOW/AUX |
| `afore/afore-ems/v1/config/state` | 30 s | `configuration_revisions`, `ems_events` | Policy configurata ESP32 |
| `afore/afore-ems/v1/diagnostics` | 30 s | `system_health` (`esp32`), `devices` | RSSI, heap, PSRAM, reset_reason, reads/errors |
| `afore/afore-ems/v1/system/manual` | 5 s | `system_health` (`aux1`) | `operating_mode=SHADOW`, `live_lock`, `aux1` |
| `afore/afore-ems/v1/system/ha_inputs` | 5 s | `system_health` (`home_assistant`), `external_inputs` | Provenienza, età e scadenza input HA |
| `afore/afore-ems/v1/input/#` & `forecast/solar` | Evento / 5 min | `external_inputs` | `forecast_solar`, `forecast_ratio`, `weather`, `load_profile`, `heartbeat` |
| `afore/afore-ems/v1/sem/#` | 30 s / cambio stato | `external_inputs`, `sem_comparisons` | Topic dedicato prodotto da HA (`deploy/ha/afore_companion_sem.yaml`) |

## Integrazione Home Assistant per Confronto SEM

Installare `deploy/ha/afore_companion_sem.yaml` in `/config/packages/` su Home Assistant per pubblicare lo snapshot SEM (`sensor.sem_decisione_prevista`, `sensor.sem_corrente_calcolata`, `select.solarman_modalita_ems`) su `afore/afore-ems/v1/sem/snapshot`.
