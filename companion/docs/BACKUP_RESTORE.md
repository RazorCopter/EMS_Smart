# Backup & Restore PostgreSQL / TimescaleDB

## Backup automatico (`deploy/docker/backup.sh`)

- Il container `backup` esegue `pg_dump` compresso (`gzip -9`) nel volume persistente `/backups` (`afore_companion_YYYYMMDD-HHMMSS.sql.gz`).
- Verifica l'integrità dell'archivio con `gzip -t` e aggiorna `/backups/.last_backup_ok` (usato dal Docker `HEALTHCHECK`).
- Retention configurabile via `BACKUP_RETENTION_DAYS` (predefinita `14` giorni).

## Backup manuale on-demand

```bash
docker compose exec -T backup sh -c 'pg_dump -h postgres-timescaledb -U "$POSTGRES_USER" -d "$POSTGRES_DB" --no-owner --no-acl | gzip -9' > manual_backup.sql.gz
```

## Restore completo

1. Arrestare `companion-collector` e `companion-api`:
   ```bash
   docker compose stop companion-collector companion-api
   ```
2. Ripristinare il database (in un DB pulito o ricreato):
   ```bash
   gunzip -c afore_companion_YYYYMMDD-HHMMSS.sql.gz | docker compose exec -T postgres-timescaledb psql -U afore_companion -d afore_companion
   ```
3. Riavviare i servizi:
   ```bash
   docker compose start companion-api companion-collector
   ```
