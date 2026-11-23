# Afore EMS Companion · v1.0.0

Companion container premium, **strettamente in sola lettura (SHADOW)**, per monitoraggio live, storico su TimescaleDB, timeline decisioni EMS, confronto SEM/ESP32, KPI energetici, diagnostica ed esportazioni.

> **Separazione delle responsabilità**
> L'ESP32 (`10.0.0.110`) resta l'unica autorità EMS locale. Se il Companion viene spento o riavviato, lettura Modbus FC04, decisioni EMS, Safety Engine, AUX1 e comunicazione MQTT/Home Assistant sull'ESP32 proseguono invariati. Il Companion non possiede credenziali Modbus/LIVE e non scrive sull'inverter.

## Servizi Docker (`docker-compose.yml`)

| Servizio | Ruolo | Porta interna / esposta |
|---|---|---|
| `timescaledb` | PostgreSQL 16 + TimescaleDB (hypertable, continuous aggregates, retention, compressione) | `5432` (interna) |
| `collector` | Ingestione asincrona MQTT (`aiomqtt`) + backfill/diagnostica REST ESP32 (sola lettura) + `NOTIFY afore_live` | `9101` (`/healthz`, `/metrics`) |
| `api` | Migrazione Alembic automatica all'avvio + FastAPI + Pydantic v2 + SQLAlchemy 2 + WebSocket/SSE live + export CSV/JSON/PDF | `8000` (interna) |
| `web` | React 18 + TypeScript strict + Vite + TanStack Query + Apache ECharts | `80` (interna) |
| `reverse-proxy` | Nginx proxy HTTP + WebSocket/SSE + gzip + static cache | `${COMPANION_HTTP_PORT:-8080}` |
| `backup` | Backup giornaliero `pg_dump` compresso con retention 7–14 giorni | — |

## Avvio rapido con Docker Compose

```bash
cd companion
cp .env.example .env
# Compilare in .env: POSTGRES_PASSWORD, COMPANION_SECRET_KEY,
# COMPANION_ADMIN_PASSWORD, COMPANION_VIEWER_PASSWORD, MQTT_USER, MQTT_PASSWORD, ESP32_PASSWORD
docker compose up -d --build
```

Accesso locale predefinito: `http://127.0.0.1:8080/`

Per test locale isolato con broker Mosquitto di sviluppo (`mosquitto-dev` su porta `11883`) e porte DB/API esposte:

```bash
docker compose -f docker-compose.yml -f docker-compose.dev.yml up -d --build
```

## Multi-architettura (`linux/amd64`, `linux/arm64`)

Tutti i Dockerfile (`api`, `collector`, `web`, `backup`) e l'immagine ufficiale `timescale/timescaledb:2.17.2-pg16` supportano nativamente `linux/amd64` e `linux/arm64`. Build multi-arch con `docker buildx`:

```bash
docker buildx build --platform linux/amd64,linux/arm64 -f deploy/docker/Dockerfile.api .
```

## Release version

For each Companion release, bump the same version in the API package's
`companion_api/__init__.py`, the collector, database and energy-domain package
metadata, the web package and lockfile, the Companion contracts metadata, and
this README. The API and both dashboard themes read their version from the
built packages. Verify the reported version after the container rebuild.
