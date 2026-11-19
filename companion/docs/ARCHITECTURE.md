# Architettura · Afore EMS Companion

## Principio fondamentale: Zero interferenza con l'EMS

```
Inverter Afore ──(RS485 FC04 read-only)──▶ ESP32-S3 (unica autorità EMS / SHADOW)
                                               │
                 ┌─────────────────────────────┴───────────────────────────┐
                 ▼ (MQTT QoS 0/1)                                          ▼ (REST GET bootstrap/backfill)
         Broker MQTT (10.0.10.10:1883) ◀── HA SEM & Forecast           collector
                 │                                                         │
                 └─────────────────────────▶ collector ────────────────────┼──▶ timescaledb
                                                                           │       │
                                            WebSocket / SSE ◀──── api ◀────────────┘ (NOTIFY afore_live)
                                                   │
                                            reverse-proxy (Nginx :8080)
                                                   │
                                                  web (React + ECharts)
```

- **Nessun accesso Modbus**: il container non ha seriale, né librerie Modbus, né route di scrittura verso l'inverter.
- **Resilienza**: la perdita del Companion, di PostgreSQL o del broker non altera il ciclo di acquisizione e decisione locale dell'ESP32.
- **Latenza LIVE < 2s**: il collector persiste i campioni su TimescaleDB e invia `NOTIFY afore_live`, ricevuto da `api` via `asyncpg` e inoltrato istantaneamente ai browser connessi su `/api/v1/ws/live` o `/api/v1/stream/live`.
