# Operazioni & Deployment · Afore EMS Companion

## Prerequisiti host

- Docker Engine + Docker Compose v2 (`linux/amd64` o `linux/arm64`)
- Accesso LAN verso ESP32 (`http://10.0.0.110`) e Broker MQTT (`10.0.10.10:1883`)

## Avvio e verifica

```bash
cd companion
cp .env.example .env
# Impostare le password reali in .env
docker compose up -d --build
docker compose ps
docker compose logs -f companion-collector companion-api
```

## Healthcheck & Monitoraggio

- Reverse proxy: `GET http://127.0.0.1:8080/healthz`
- API health: `GET http://127.0.0.1:8080/api/v1/health`
- Metriche Prometheus API: `GET http://127.0.0.1:8080/metrics`
- Metriche Prometheus Collector: porta interna `9101` (`/metrics`, `/healthz`)

## Rollback / Spegnimento sicuro

Spegnere il Companion non ha alcun impatto sull'EMS:

```bash
docker compose down
```

L'ESP32 e Home Assistant continuano a operare normalmente su `10.0.0.110` e `10.0.10.10:1883`.
