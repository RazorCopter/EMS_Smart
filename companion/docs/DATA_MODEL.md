# Modello Dati & TimescaleDB · Afore EMS Companion

## Tabelle principali (`database/src/companion_db/models.py`)

| Tabella | Tipo | Chiave / Deduplicazione | Retention predefinita |
|---|---|---|---|
| `devices` | Relazionale | `device_id` | Permanente |
| `mqtt_messages` | Hypertable (`received_at`) | `(id, received_at)` | 30 giorni (compressa dopo 3g) |
| `telemetry_samples` | Hypertable (`source_timestamp`) | `(device_id, source_timestamp, uptime_ms)` | 90 giorni (compressa dopo 7g) |
| `energy_samples` | Hypertable (`bucket_start`) | `(bucket_start, device_id)` | Permanente |
| `ems_decisions` | Hypertable (`source_timestamp`) | `(device_id, boot_id, seq, source_timestamp)` | Permanente (compressa dopo 14g) |
| `ems_events` | Relazionale | `id` | Permanente |
| `ems_strategy_intervals` | Relazionale | `id` | Permanente |
| `external_inputs` | Hypertable (`received_at`) | `(id, received_at)` | 90 giorni |
| `sem_comparisons` | Hypertable (`compared_at`) | `(compared_at, device_id)` | Permanente |
| `configuration_revisions` | Relazionale | `id` | Permanente |
| `system_health` | Relazionale | `(device_id, component)` | Ultimo stato |
| `aux_events` | Relazionale | `id` | Permanente |
| `companion_users` / `companion_settings` | Relazionali | `username` / `key` | Permanente |

## Continuous Aggregates

- `telemetry_1m`: bucket 1 minuto su `telemetry_samples`, retention 2 anni (`730` giorni configurabile).
- `telemetry_1h`: bucket 1 ora su `telemetry_samples`, nessuna scadenza.

## Gestione `null`, `boot_id` e gap

- I campi assenti dall'inverter restano `NULL` in tutte le tabelle e aggregazioni; nessuna query usa `COALESCE(..., 0)`.
- Quando l'ESP32 (`v5.1-console-5c`) non emette un campo `boot_id` esplicito, il collector sintetizza e persiste un `boot_id` deterministico al rilevamento di `seq < last_seq` o calo di `uptime_ms`, emettendo l'evento `ESP_REBOOT`.
- L'accumulatore energetico integra solo intervalli contigui (`dt <= 60s`); gli intervalli superiori vengono contabilizzati in `gap_seconds` e mai interpolati.
