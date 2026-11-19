#!/bin/sh
set -eu

RETENTION_DAYS="${BACKUP_RETENTION_DAYS:-14}"
INTERVAL_SECONDS="${BACKUP_INTERVAL_SECONDS:-86400}"

run_backup() {
  STAMP="$(date +%Y%m%d-%H%M%S)"
  TARGET="/backups/afore_companion_${STAMP}.sql.gz"
  TMP="${TARGET}.tmp"
  echo "[backup] Starting pg_dump -> ${TARGET}"
  pg_dump -h "${PGHOST:-postgres-timescaledb}" -U "${POSTGRES_USER}" -d "${POSTGRES_DB}" --no-owner --no-acl | gzip -9 > "${TMP}"
  mv "${TMP}" "${TARGET}"
  gzip -t "${TARGET}"
  date -Iseconds > /backups/.last_backup_ok
  find /backups -name 'afore_companion_*.sql.gz' -type f -mtime +"${RETENTION_DAYS}" -delete || true
  echo "[backup] Completed ${TARGET}"
}

# Initial wait for PostgreSQL readiness
until pg_isready -h "${PGHOST:-postgres-timescaledb}" -U "${POSTGRES_USER}" -d "${POSTGRES_DB}" >/dev/null 2>&1; do
  sleep 2
done

# Run immediate baseline backup on startup, then sleep interval
while true; do
  run_backup || echo "[backup] Backup failed!" >&2
  sleep "${INTERVAL_SECONDS}"
done
