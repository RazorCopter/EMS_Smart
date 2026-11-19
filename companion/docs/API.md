# API Companion v1 · Afore EMS Companion

Tutte le route sono versionate sotto `/api/v1/` e sono **strettamente in sola lettura verso l'impianto** (nessun endpoint di controllo inverter).

## Autenticazione & Ruoli

- `POST /api/v1/auth/login`: rate-limited (10 tentativi/min per IP), imposta cookie `HttpOnly` firmato (`afore_companion_session`) + cookie CSRF (`afore_companion_csrf`).
- `POST /api/v1/auth/logout`: rimuove la sessione.
- `GET /api/v1/auth/me`: restituisce utente, ruolo (`viewer` o `admin`) e CSRF token.

## Endpoint dati (Viewer + Admin)

- `GET /api/v1/health`: liveness/readiness DB + modalità `SHADOW`.
- `GET /api/v1/system/status`: stato degli 8 componenti (`esp32`, `mqtt`, `home_assistant`, `modbus`, `ntp`, `collector`, `database`, `aux1`).
- `GET /api/v1/live`: ultimo snapshot telemetria/decisione, KPI oggi/ieri, sparkline orarie, eventi recenti, stato sistema.
- `WS /api/v1/ws/live` e `GET /api/v1/stream/live`: streaming real-time guidato da PostgreSQL `NOTIFY afore_live`.
- `GET /api/v1/history?range=today|yesterday|7d|30d|12m|custom&bucket=auto|1m|5m|15m|1h|1d`: serie storica con bucket espliciti e `null` sui gap.
- `GET /api/v1/decisions` & `GET /api/v1/decisions/{id}`: audit decisioni SHADOW (`executed=false`, `applied=null`, `modbus_write_count=0`).
- `GET /api/v1/events`: timeline eventi di sistema/strategia/Safety.
- `GET /api/v1/kpi`: bilancio energetico, formule documentate di autoconsumo e autosufficienza, qualità dati.
- `GET /api/v1/comparison?watt_threshold=100`: confronto SEM reale vs ESP SHADOW simulato.
- `GET /api/v1/battery`: stato batteria, limiti, provenienza esplicita (`measured`, `derived`, `configured`, `provisional`, `unavailable`), sessioni e riserve.
- `GET /api/v1/reports`: riepilogo report giornaliero/settimanale/mensile/personalizzato.
- `GET /api/v1/export?format=csv|json|pdf&scope=history|decisions|comparison|report`: esportazione file.
- `GET /metrics`: metriche Prometheus.

## Endpoint amministrativi Companion (Admin + CSRF)

- `GET /api/v1/settings`: stato connessioni senza segreti (`password_configured: true/false`), retention e soglie.
- `PATCH /api/v1/settings`: aggiorna soglia confronto, timezone, unità e preferenze UI.
- `POST /api/v1/settings/test-connection?target=esp32|database`: test diagnostico in sola lettura.
