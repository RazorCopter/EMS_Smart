from __future__ import annotations

from dataclasses import dataclass, field
from datetime import datetime, timezone
import json
import logging
from typing import Any
import uuid

from prometheus_client import Counter, Gauge
from sqlalchemy import select, text
from sqlalchemy.dialects.postgresql import insert
from sqlalchemy.ext.asyncio import AsyncSession, async_sessionmaker

from companion_db.models import (
    AuxEvent,
    ConfigurationRevision,
    Device,
    EmsDecision,
    EmsEvent,
    EmsStrategyInterval,
    EnergySample,
    ExternalInput,
    MqttMessage,
    SemComparison,
    SystemHealth,
    TelemetrySample,
)
from energy_domain import (
    AuxEventPayload,
    ConfigStatePayload,
    DecisionAuditPayload,
    DiagnosticsPayload,
    EnergyAccumulator,
    ExternalInputEnvelope,
    ManualStatePayload,
    SemComparisonThresholds,
    SemSnapshotPayload,
    TelemetryPayload,
    compare_sem_and_esp,
)
from energy_domain.models import parse_utc_timestamp

logger = logging.getLogger(__name__)

MQTT_MESSAGES_TOTAL = Counter(
    "companion_mqtt_messages_total",
    "Total MQTT messages processed by topic suffix and validity",
    ["suffix", "valid"],
)
TELEMETRY_LAST_TS = Gauge(
    "companion_telemetry_last_timestamp_seconds",
    "Unix timestamp of the last valid ingested telemetry sample",
)
COLLECTOR_GAPS_TOTAL = Counter(
    "companion_collector_sequence_gaps_total",
    "Detected ESP decision sequence gaps",
)
COLLECTOR_REBOOTS_TOTAL = Counter(
    "companion_collector_esp_reboots_total",
    "Detected ESP reboot / sequence resets",
)


@dataclass
class DeviceRuntimeState:
    boot_id: str | None = None
    last_seq: int | None = None
    last_uptime_ms: int | None = None
    last_telemetry_ts: datetime | None = None
    last_telemetry_payload: TelemetryPayload | None = None
    last_decision_payload: DecisionAuditPayload | None = None
    last_strategy: str | None = None
    last_configured_strategy: str | None = None
    last_safety_reason: str | None = None
    last_ha_online: bool | None = None
    last_config_revision: int | None = None
    last_sem_snapshot: SemSnapshotPayload | None = None
    active_interval_id: int | None = None
    active_interval_strategy: str | None = None
    active_interval_state: str | None = None
    active_interval_start: datetime | None = None
    active_interval_count: int = 0
    needs_backfill: bool = False


class IngestionEngine:
    """Stateless-friendly, idempotent ingestion pipeline for MQTT and REST snapshots."""

    def __init__(
        self,
        session_factory: async_sessionmaker[AsyncSession],
        *,
        device_id: str = "afore-ems",
        base_topic: str = "afore/afore-ems/v1",
        sem_prefix: str = "afore/afore-ems/v1/sem",
        max_energy_gap_s: float = 60.0,
        sem_thresholds: SemComparisonThresholds | None = None,
    ) -> None:
        self.session_factory = session_factory
        self.device_id = device_id
        self.base_topic = base_topic.rstrip("/")
        self.sem_prefix = sem_prefix.rstrip("/")
        self.max_energy_gap_s = max_energy_gap_s
        self.sem_thresholds = sem_thresholds or SemComparisonThresholds()
        self.state = DeviceRuntimeState()

    async def bootstrap_from_db(self) -> None:
        """Restores last known boot_id, seq, and uptime_ms on collector restart."""
        async with self.session_factory() as session:
            dev = await session.get(Device, self.device_id)
            if dev:
                self.state.boot_id = dev.current_boot_id
                self.state.last_seq = dev.last_seq
                self.state.last_uptime_ms = dev.last_uptime_ms

            last_tel = (
                await session.execute(
                    select(TelemetrySample)
                    .where(TelemetrySample.device_id == self.device_id)
                    .order_by(TelemetrySample.source_timestamp.desc())
                    .limit(1)
                )
            ).scalar_one_or_none()
            if last_tel:
                self.state.last_telemetry_ts = last_tel.source_timestamp

    def _topic_suffix(self, topic: str) -> str:
        prefix = f"{self.base_topic}/"
        if topic.startswith(prefix):
            return topic[len(prefix) :]
        return topic

    async def ingest_mqtt_message(
        self,
        topic: str,
        payload_bytes: bytes,
        *,
        qos: int = 0,
        retain: bool = False,
        received_at: datetime | None = None,
    ) -> None:
        now = received_at or datetime.now(timezone.utc)
        suffix = self._topic_suffix(topic)
        raw_text = payload_bytes.decode("utf-8", errors="replace")
        parsed_json: dict[str, Any] | None = None
        schema_valid = True
        validation_error: str | None = None

        if suffix == "availability":
            parsed_json = {"availability": raw_text.strip()}
        else:
            try:
                loaded = json.loads(raw_text)
                if isinstance(loaded, dict):
                    parsed_json = loaded
                else:
                    parsed_json = {"value": loaded}
            except Exception as exc:
                schema_valid = False
                validation_error = f"JSON_DECODE_ERROR: {exc}"[:250]

        async with self.session_factory() as session:
            raw_msg = MqttMessage(
                received_at=now,
                topic=topic,
                qos=qos,
                retain=retain,
                payload_bytes=len(payload_bytes),
                payload_json=parsed_json,
                raw_text=raw_text if not schema_valid else None,
                schema_valid=schema_valid,
                validation_error=validation_error,
            )
            session.add(raw_msg)
            await session.flush()

            if not schema_valid or parsed_json is None:
                MQTT_MESSAGES_TOTAL.labels(suffix=suffix, valid="false").inc()
                await session.commit()
                return

            try:
                if suffix == "availability":
                    await self._handle_availability(session, raw_text.strip(), now)
                elif suffix == "telemetry":
                    await self._handle_telemetry(
                        session, parsed_json, now, provenance="mqtt", source_topic=topic, raw_msg_id=raw_msg.id
                    )
                elif suffix in {"ems/decision", "ems/state"}:
                    await self._handle_decision(session, parsed_json, now)
                elif suffix == "ems/ack":
                    await self._handle_ack(session, parsed_json, now)
                elif suffix == "config/state":
                    await self._handle_config_state(session, parsed_json, now, source="mqtt_config_state")
                elif suffix == "diagnostics":
                    await self._handle_diagnostics(session, parsed_json, now)
                elif suffix == "system/manual":
                    await self._handle_manual(session, parsed_json, now)
                elif suffix == "system/ha_inputs":
                    await self._handle_ha_inputs(session, parsed_json, now)
                elif suffix.startswith("input/") or suffix == "forecast/solar":
                    kind = suffix.split("/", 1)[1] if suffix.startswith("input/") else "forecast_solar"
                    await self._handle_external_input(session, kind, parsed_json, now, retain=retain)
                elif topic.startswith(self.sem_prefix):
                    await self._handle_sem_mqtt(session, suffix, parsed_json, now)

                MQTT_MESSAGES_TOTAL.labels(suffix=suffix, valid="true").inc()
            except Exception as exc:
                logger.warning("Schema validation/ingestion warning for %s: %s", suffix, exc)
                raw_msg.schema_valid = False
                raw_msg.validation_error = str(exc)[:250]
                MQTT_MESSAGES_TOTAL.labels(suffix=suffix, valid="false").inc()

            await session.commit()

        await self._notify_live(suffix)

    async def ingest_rest_telemetry(self, data: dict[str, Any], received_at: datetime | None = None) -> None:
        now = received_at or datetime.now(timezone.utc)
        async with self.session_factory() as session:
            await self._handle_telemetry(
                session,
                data,
                now,
                provenance="rest_bootstrap",
                source_topic="rest:/api/v1/telemetry",
                raw_msg_id=None,
            )
            await session.commit()
        await self._notify_live("telemetry")

    async def ingest_rest_decisions(self, items: list[dict[str, Any]], received_at: datetime | None = None) -> int:
        now = received_at or datetime.now(timezone.utc)
        inserted = 0
        async with self.session_factory() as session:
            for item in sorted(items, key=lambda r: int(r.get("seq", 0))):
                if await self._handle_decision(session, item, now, from_backfill=True):
                    inserted += 1
            await session.commit()
        if inserted:
            await self._notify_live("ems/decision")
        return inserted

    async def ingest_rest_health_bundle(
        self,
        *,
        health: dict[str, Any] | None = None,
        config: dict[str, Any] | None = None,
        manual: dict[str, Any] | None = None,
        ha_inputs: dict[str, Any] | None = None,
        time_info: dict[str, Any] | None = None,
    ) -> None:
        now = datetime.now(timezone.utc)
        async with self.session_factory() as session:
            if health is not None:
                await self._upsert_health(
                    session,
                    "esp32",
                    "online",
                    {
                        "firmware": health.get("firmware"),
                        "mode": health.get("mode", "SHADOW"),
                        "uptime_ms": health.get("uptime_ms"),
                        "rssi_dbm": health.get("rssi_dbm"),
                        "heap": health.get("heap"),
                        "ota_slot": health.get("ota_slot"),
                        "executed": False,
                        "modbus_write_count": 0,
                    },
                    now,
                )
                await self._upsert_device(
                    session,
                    firmware=health.get("firmware"),
                    mode=health.get("mode", "SHADOW"),
                    seen_at=now,
                )
            if time_info is not None:
                ntp_status = "online" if time_info.get("time_valid") else "degraded"
                await self._upsert_health(session, "ntp", ntp_status, time_info, now)
            if config is not None:
                await self._handle_config_state(session, config, now, source="rest_ems_config")
            if manual is not None:
                await self._handle_manual(session, manual, now)
            if ha_inputs is not None:
                await self._handle_ha_inputs(session, ha_inputs, now)

            await self._upsert_health(session, "collector", "online", {"device_id": self.device_id}, now)
            await self._upsert_health(session, "database", "online", {"engine": "timescaledb"}, now)
            await session.commit()

    async def update_component_health(self, component: str, status: str, detail: dict[str, Any]) -> None:
        now = datetime.now(timezone.utc)
        async with self.session_factory() as session:
            await self._upsert_health(session, component, status, detail, now)
            await session.commit()

    async def ingest_sem_snapshot(self, data: dict[str, Any], received_at: datetime | None = None) -> None:
        now = received_at or datetime.now(timezone.utc)
        async with self.session_factory() as session:
            await self._handle_sem_mqtt(session, "sem/snapshot", data, now)
            await session.commit()
        await self._notify_live("sem")

    async def _notify_live(self, event_kind: str) -> None:
        try:
            async with self.session_factory() as session:
                payload = json.dumps({"kind": event_kind, "device_id": self.device_id})
                await session.execute(text("SELECT pg_notify('afore_live', :p)"), {"p": payload})
                await session.commit()
        except Exception as exc:
            logger.debug("NOTIFY afore_live skipped: %s", exc)

    def _resolve_boot_id(
        self,
        *,
        explicit_boot_id: str | None,
        seq: int,
        uptime_ms: int,
        ts: datetime,
        from_backfill: bool = False,
    ) -> tuple[str, bool]:
        """Determines active boot_id and detects ESP reboots / sequence resets."""
        if explicit_boot_id:
            rebooted = (not from_backfill) and self.state.boot_id is not None and explicit_boot_id != self.state.boot_id
            if not from_backfill:
                self.state.boot_id = explicit_boot_id
            return explicit_boot_id, rebooted

        if from_backfill and self.state.boot_id is not None:
            return self.state.boot_id, False

        rebooted = False
        if self.state.boot_id is None:
            boot_epoch = int(ts.timestamp() - (uptime_ms / 1000.0))
            self.state.boot_id = f"boot-{boot_epoch}-{uuid.uuid4().hex[:6]}"
        elif (
            (self.state.last_seq is not None and seq < self.state.last_seq)
            or (self.state.last_uptime_ms is not None and uptime_ms + 5000 < self.state.last_uptime_ms)
        ):
            rebooted = True
            boot_epoch = int(ts.timestamp() - (uptime_ms / 1000.0))
            self.state.boot_id = f"boot-{boot_epoch}-{uuid.uuid4().hex[:6]}"
        return self.state.boot_id, rebooted

    async def _upsert_device(
        self,
        session: AsyncSession,
        *,
        firmware: str | None = None,
        mode: str = "SHADOW",
        boot_id: str | None = None,
        last_seq: int | None = None,
        last_uptime_ms: int | None = None,
        seen_at: datetime | None = None,
    ) -> None:
        values: dict[str, Any] = {
            "device_id": self.device_id,
            "operating_mode": mode,
            "last_seen_at": seen_at or datetime.now(timezone.utc),
        }
        if firmware:
            values["firmware_version"] = firmware
        if boot_id:
            values["current_boot_id"] = boot_id
        if last_seq is not None:
            values["last_seq"] = last_seq
        if last_uptime_ms is not None:
            values["last_uptime_ms"] = last_uptime_ms

        stmt = insert(Device).values(**values)
        update_cols = {k: v for k, v in values.items() if k != "device_id"}
        await session.execute(stmt.on_conflict_do_update(index_elements=["device_id"], set_=update_cols))

    async def _upsert_health(
        self, session: AsyncSession, component: str, status: str, detail: dict[str, Any], recorded_at: datetime
    ) -> None:
        stmt = insert(SystemHealth).values(
            device_id=self.device_id,
            component=component,
            status=status,
            detail_json=detail,
            recorded_at=recorded_at,
        )
        stmt = stmt.on_conflict_do_update(
            constraint="uq_sys_health_dev_comp",
            set_={"status": status, "detail_json": detail, "recorded_at": recorded_at},
        )
        await session.execute(stmt)

    async def _emit_event(
        self,
        session: AsyncSession,
        *,
        occurred_at: datetime,
        event_type: str,
        severity: str,
        title: str,
        detail: str | None = None,
        metadata: dict[str, Any] | None = None,
    ) -> None:
        session.add(
            EmsEvent(
                occurred_at=occurred_at,
                device_id=self.device_id,
                boot_id=self.state.boot_id,
                event_type=event_type,
                severity=severity,
                title=title,
                detail=detail,
                metadata_json=metadata or {},
            )
        )

    async def _handle_availability(self, session: AsyncSession, status_text: str, now: datetime) -> None:
        online = status_text.lower() == "online"
        await self._upsert_health(
            session,
            "esp32",
            "online" if online else "offline",
            {"availability": status_text},
            now,
        )
        await self._emit_event(
            session,
            occurred_at=now,
            event_type="MQTT_ONLINE" if online else "MQTT_OFFLINE",
            severity="info" if online else "warning",
            title=f"ESP32 availability: {status_text}",
            detail=f"LWT/availability topic reported '{status_text}'",
        )

    async def _handle_telemetry(
        self,
        session: AsyncSession,
        data: dict[str, Any],
        now: datetime,
        *,
        provenance: str,
        source_topic: str,
        raw_msg_id: int | None,
    ) -> None:
        tel = TelemetryPayload.model_validate(data)
        src_ts = tel.timestamp if (tel.timestamp and tel.time_valid is not False) else now
        uptime_ms = tel.uptime_ms or 0

        stmt = (
            insert(TelemetrySample)
            .values(
                source_timestamp=src_ts,
                device_id=self.device_id,
                uptime_ms=uptime_ms,
                received_at=now,
                boot_id=self.state.boot_id or "unknown",
                time_source=tel.time_source or "UNSYNCED",
                time_valid=bool(tel.time_valid),
                soc=tel.soc,
                battery_w=tel.battery_w,
                battery_v=tel.battery_v,
                battery_a=tel.battery_a,
                pv_w=tel.pv_w,
                pv1_v=tel.pv1_v,
                pv1_a=tel.pv1_a,
                pv1_w=tel.pv1_w,
                pv2_v=tel.pv2_v,
                pv2_a=tel.pv2_a,
                pv2_w=tel.pv2_w,
                grid_w=tel.grid_w,
                grid_v=tel.grid_v,
                hz=tel.hz,
                load_w=tel.load_w,
                inverter_w=tel.inverter_w,
                temp_c=tel.temp_c,
                reads=tel.reads,
                errors=tel.errors,
                fault=tel.fault,
                telemetry_valid=tel.telemetry_valid,
                shadow=True,
                aux1=tel.aux1,
                modbus_write_count=0,
                mqtt_connected=tel.mqtt,
                quality_json={k: v.model_dump() for k, v in tel.quality.items()},
                provenance=provenance,
                source_topic=source_topic,
                raw_message_id=raw_msg_id,
            )
            .on_conflict_do_nothing(index_elements=["source_timestamp", "device_id", "uptime_ms"])
        )
        res = await session.execute(stmt)

        if res.rowcount and self.state.last_telemetry_ts and src_ts > self.state.last_telemetry_ts:
            dt = (src_ts - self.state.last_telemetry_ts).total_seconds()
            acc = EnergyAccumulator(max_gap_s=self.max_energy_gap_s)
            acc.add_interval(
                dt,
                pv_w=tel.pv_w,
                load_w=tel.load_w,
                grid_w=tel.grid_w,
                battery_w=tel.battery_w,
            )
            totals = acc.totals()
            await session.execute(
                insert(EnergySample)
                .values(
                    bucket_start=self.state.last_telemetry_ts,
                    device_id=self.device_id,
                    bucket_end=src_ts,
                    pv_wh=totals.pv_wh,
                    load_wh=totals.load_wh,
                    grid_import_wh=totals.grid_import_wh,
                    grid_export_wh=totals.grid_export_wh,
                    battery_charge_wh=totals.battery_charge_wh,
                    battery_discharge_wh=totals.battery_discharge_wh,
                    self_consumed_pv_wh=totals.self_consumed_pv_wh,
                    valid_seconds=totals.valid_seconds,
                    gap_seconds=totals.gap_seconds,
                    provenance="derived",
                )
                .on_conflict_do_nothing()
            )

        if self.state.last_telemetry_ts is None or src_ts >= self.state.last_telemetry_ts:
            self.state.last_telemetry_ts = src_ts
            self.state.last_telemetry_payload = tel
            TELEMETRY_LAST_TS.set(src_ts.timestamp())

        modbus_status = "degraded" if tel.fault else ("online" if tel.telemetry_valid else "degraded")
        await self._upsert_health(
            session,
            "modbus",
            modbus_status,
            {"reads": tel.reads, "errors": tel.errors, "fault": tel.fault, "modbus_write_count": 0},
            now,
        )
        await self._upsert_health(
            session,
            "mqtt",
            "online" if tel.mqtt is not False else "offline",
            {"broker_connected": tel.mqtt},
            now,
        )
        await self._upsert_health(
            session,
            "ntp",
            "online" if tel.time_valid else "degraded",
            {"time_source": tel.time_source, "time_valid": tel.time_valid},
            now,
        )
        await self._upsert_device(session, seen_at=now)

    async def _handle_decision(
        self,
        session: AsyncSession,
        data: dict[str, Any],
        now: datetime,
        *,
        from_backfill: bool = False,
    ) -> bool:
        dec = DecisionAuditPayload.model_validate(data)
        src_ts = dec.timestamp if (dec.timestamp and dec.time_valid) else now
        boot_id, rebooted = self._resolve_boot_id(
            explicit_boot_id=dec.boot_id,
            seq=dec.seq,
            uptime_ms=dec.uptime_ms,
            ts=src_ts,
            from_backfill=from_backfill,
        )

        if rebooted:
            COLLECTOR_REBOOTS_TOTAL.inc()
            await self._emit_event(
                session,
                occurred_at=src_ts,
                event_type="ESP_REBOOT",
                severity="warning",
                title="Riavvio ESP32 rilevato",
                detail=f"Nuova sessione boot {boot_id} (seq {dec.seq}, uptime {dec.uptime_ms} ms)",
                metadata={"boot_id": boot_id, "seq": dec.seq, "uptime_ms": dec.uptime_ms},
            )
            self.state.last_seq = None

        if not from_backfill and self.state.last_seq is not None and dec.seq > self.state.last_seq + 1:
            missing = dec.seq - self.state.last_seq - 1
            # ESP32 ticks at 1 Hz while MQTT telemetry_s publishes every 5-30s;
            # sync intermediate records silently via bounded REST backfill unless
            # the jump exceeds the 256-slot PSRAM ring buffer (unrecoverable gap).
            if missing <= 250:
                self.state.needs_backfill = True
            if missing > 250:
                COLLECTOR_GAPS_TOTAL.inc()
                await self._emit_event(
                    session,
                    occurred_at=src_ts,
                    event_type="DECISION_GAP",
                    severity="warning",
                    title=f"Gap sequenza decisioni ({missing} record)",
                    detail=f"Sequenza passata da #{self.state.last_seq} a #{dec.seq} oltre il ring PSRAM",
                    metadata={"prev_seq": self.state.last_seq, "curr_seq": dec.seq, "missing": missing},
                )

        stmt = (
            insert(EmsDecision)
            .values(
                source_timestamp=src_ts,
                device_id=self.device_id,
                boot_id=boot_id,
                seq=dec.seq,
                received_at=now,
                uptime_ms=dec.uptime_ms,
                time_source=dec.time_source,
                time_valid=dec.time_valid,
                data_source=dec.source,
                strategy=dec.strategy,
                configured_strategy=dec.configured,
                previous_strategy=dec.previous,
                state=dec.state,
                transition=dec.transition,
                soc=dec.soc,
                grid_w=dec.grid_w,
                battery_w=dec.battery_w,
                load_w=dec.load_w,
                pv_w=dec.pv_w,
                candidate_w=dec.candidate_w,
                limited_w=dec.limited_w,
                simulated_w=dec.simulated_w,
                safety_min_w=dec.safety_min_w,
                safety_max_w=dec.safety_max_w,
                missing_wh=dec.missing_wh,
                average_w=dec.average_w,
                unreachable=dec.unreachable,
                safety_reason=dec.safety_reason,
                reason=dec.reason,
                executed=False,
                applied=None,
                aux1=dec.aux1,
                modbus_write_count=0,
                quality_json=dec.quality,
                ha_inputs_json=dec.ha_inputs.model_dump(mode="json") if dec.ha_inputs else None,
                raw_payload_json=data,
            )
            .on_conflict_do_nothing(index_elements=["source_timestamp", "device_id", "boot_id", "seq"])
        )
        res = await session.execute(stmt)
        inserted = bool(res.rowcount)

        if inserted:
            await self._update_strategy_interval(session, dec, boot_id, src_ts)
            if not from_backfill:
                await self._detect_decision_events(session, dec, src_ts)

        if self.state.last_seq is None or dec.seq > self.state.last_seq:
            self.state.last_seq = dec.seq
            self.state.last_uptime_ms = dec.uptime_ms
            self.state.last_decision_payload = dec

        await self._upsert_device(
            session,
            boot_id=boot_id,
            last_seq=self.state.last_seq,
            last_uptime_ms=self.state.last_uptime_ms,
            seen_at=now,
        )

        # If we already have a recent SEM snapshot, emit comparison point
        if inserted and not from_backfill and self.state.last_sem_snapshot is not None:
            await self._record_sem_comparison(session, src_ts, dec, self.state.last_sem_snapshot)

        return inserted

    async def _update_strategy_interval(
        self, session: AsyncSession, dec: DecisionAuditPayload, boot_id: str, ts: datetime
    ) -> None:
        if (
            self.state.active_interval_id is not None
            and self.state.active_interval_strategy == dec.strategy
            and self.state.active_interval_state == dec.state
            and self.state.active_interval_start is not None
        ):
            interval = await session.get(EmsStrategyInterval, self.state.active_interval_id)
            if interval:
                interval.ended_at = ts
                interval.duration_s = max(0, int((ts - interval.started_at).total_seconds()))
                interval.decision_count += 1
                self.state.active_interval_count = interval.decision_count
                return

        interval = EmsStrategyInterval(
            device_id=self.device_id,
            boot_id=boot_id,
            strategy=dec.strategy,
            configured_strategy=dec.configured,
            state=dec.state,
            started_at=ts,
            ended_at=ts,
            duration_s=0,
            decision_count=1,
        )
        session.add(interval)
        await session.flush()
        self.state.active_interval_id = interval.id
        self.state.active_interval_strategy = dec.strategy
        self.state.active_interval_state = dec.state
        self.state.active_interval_start = ts
        self.state.active_interval_count = 1

    async def _detect_decision_events(
        self, session: AsyncSession, dec: DecisionAuditPayload, ts: datetime
    ) -> None:
        if self.state.last_strategy is not None and dec.strategy != self.state.last_strategy:
            await self._emit_event(
                session,
                occurred_at=ts,
                event_type="STRATEGY_CHANGE",
                severity="info",
                title=f"Strategia: {self.state.last_strategy} -> {dec.strategy}",
                detail=f"Transizione: {dec.transition} · Motivo: {dec.reason}",
                metadata={"from": self.state.last_strategy, "to": dec.strategy, "seq": dec.seq},
            )
        self.state.last_strategy = dec.strategy

        if dec.transition in {"TARGET_REACHED", "DEADLINE_EXPIRED", "MAINTENANCE_COMPLETE", "MAINTENANCE_FAULT_ABORT"}:
            await self._emit_event(
                session,
                occurred_at=ts,
                event_type=dec.transition,
                severity="warning" if "ABORT" in dec.transition or "EXPIRED" in dec.transition else "info",
                title=f"Sessione {dec.configured}: {dec.transition}",
                detail=f"Ritorno a {dec.strategy} (SOC {dec.soc}%)",
                metadata={"seq": dec.seq, "soc": dec.soc},
            )

        if dec.unreachable:
            await self._emit_event(
                session,
                occurred_at=ts,
                event_type="TARGET_UNREACHABLE",
                severity="warning",
                title="Target SOC irraggiungibile con i limiti attuali",
                detail=f"Richiesti {dec.average_w} W medi, limite {dec.safety_max_w} W",
                metadata={"seq": dec.seq, "average_w": dec.average_w, "safety_max_w": dec.safety_max_w},
            )

        if dec.safety_reason != self.state.last_safety_reason:
            if dec.safety_reason in {"SOC_RESERVE", "SOC_CEILING"}:
                await self._emit_event(
                    session,
                    occurred_at=ts,
                    event_type="SOC_LIMIT",
                    severity="warning",
                    title=f"Intervento Safety: {dec.safety_reason}",
                    detail=f"SOC {dec.soc}% · Candidato {dec.candidate_w} W -> Simulato {dec.simulated_w} W",
                    metadata={"seq": dec.seq, "safety_reason": dec.safety_reason},
                )
            elif dec.safety_reason in {"POWER_LIMIT", "VOLTAGE_LIMIT"}:
                await self._emit_event(
                    session,
                    occurred_at=ts,
                    event_type="POWER_LIMIT",
                    severity="warning",
                    title=f"Limite Safety applicato: {dec.safety_reason}",
                    detail=f"Candidato {dec.candidate_w} W -> Limitato {dec.limited_w} W",
                    metadata={"seq": dec.seq, "safety_reason": dec.safety_reason},
                )
            elif dec.safety_reason == "STALE_TELEMETRY":
                await self._emit_event(
                    session,
                    occurred_at=ts,
                    event_type="TELEMETRY_STALE",
                    severity="error",
                    title="Telemetria scaduta -> SAFE_MODE",
                    detail="Ingresso automatico in SAFE_MODE per stale telemetry",
                    metadata={"seq": dec.seq},
                )
            self.state.last_safety_reason = dec.safety_reason

        if dec.ha_inputs is not None:
            ha_on = dec.ha_inputs.ha_online
            if self.state.last_ha_online is not None and ha_on != self.state.last_ha_online:
                await self._emit_event(
                    session,
                    occurred_at=ts,
                    event_type="HA_ONLINE" if ha_on else "HA_OFFLINE",
                    severity="info" if ha_on else "warning",
                    title="Home Assistant Online" if ha_on else "Home Assistant Offline",
                    detail="Stato heartbeat HA visto dall'ESP32",
                )
            self.state.last_ha_online = ha_on

    async def _handle_ack(self, session: AsyncSession, data: dict[str, Any], now: datetime) -> None:
        ack = AuxEventPayload.model_validate(data)
        ts = ack.timestamp or now
        if ack.aux1 is not None or "aux" in str(ack.message_id or "").lower():
            session.add(
                AuxEvent(
                    occurred_at=ts,
                    device_id=self.device_id,
                    origin="MQTT",
                    action=ack.message_id,
                    outcome=ack.reason,
                    aux_state=ack.aux1,
                    physical_feedback=None,
                    raw_json=data,
                )
            )
            await self._emit_event(
                session,
                occurred_at=ts,
                event_type="AUX_COMMAND",
                severity="info" if ack.accepted else "warning",
                title=f"Comando AUX / MQTT ACK: {ack.reason}",
                detail=f"AUX1={ack.aux1} · accepted={ack.accepted} · executed=false",
                metadata=data,
            )

    async def _handle_config_state(
        self, session: AsyncSession, data: dict[str, Any], now: datetime, *, source: str
    ) -> None:
        cfg = ConfigStatePayload.model_validate(data)
        if cfg.revision is not None and cfg.revision != self.state.last_config_revision:
            session.add(
                ConfigurationRevision(
                    recorded_at=now,
                    device_id=self.device_id,
                    source=source,
                    revision=cfg.revision,
                    strategy_name=cfg.strategy_name,
                    config_json=data,
                )
            )
            if self.state.last_config_revision is not None:
                await self._emit_event(
                    session,
                    occurred_at=now,
                    event_type="CONFIG_CHANGE",
                    severity="info",
                    title=f"Configurazione ESP aggiornata (rev {cfg.revision})",
                    detail=f"Strategia configurata: {cfg.strategy_name or cfg.strategy}",
                    metadata={"revision": cfg.revision, "source": source},
                )
            self.state.last_config_revision = cfg.revision

    async def _handle_diagnostics(self, session: AsyncSession, data: dict[str, Any], now: datetime) -> None:
        diag = DiagnosticsPayload.model_validate(data)
        await self._upsert_health(
            session,
            "esp32",
            "online",
            {
                "firmware": diag.firmware,
                "rssi_dbm": diag.rssi,
                "heap": diag.heap,
                "psram": diag.psram,
                "uptime_ms": diag.uptime_ms,
                "reset_reason": diag.reset_reason,
                "reads": diag.reads,
                "errors": diag.errors,
                "modbus_write_count": 0,
            },
            now,
        )
        await self._upsert_device(session, firmware=diag.firmware, seen_at=now)

    async def _handle_manual(self, session: AsyncSession, data: dict[str, Any], now: datetime) -> None:
        man = ManualStatePayload.model_validate(data)
        await self._upsert_health(
            session,
            "aux1",
            "online" if man.aux1 == "ON" else "off",
            man.model_dump(mode="json"),
            now,
        )

    async def _handle_ha_inputs(self, session: AsyncSession, data: dict[str, Any], now: datetime) -> None:
        ha_online = bool(data.get("ha_online"))
        await self._upsert_health(
            session,
            "home_assistant",
            "online" if ha_online else "offline",
            data,
            now,
        )
        for kind in ("forecast_solar", "forecast_ratio", "weather", "load_profile", "heartbeat"):
            entry = data.get(kind)
            if isinstance(entry, dict):
                session.add(
                    ExternalInput(
                        received_at=now,
                        source_timestamp=parse_utc_timestamp(entry.get("timestamp")),
                        valid_until=parse_utc_timestamp(entry.get("expires_at") or entry.get("valid_until")),
                        device_id=self.device_id,
                        input_kind=kind,
                        status=str(entry.get("status", "unavailable")),
                        age_s=entry.get("age_s"),
                        used=bool(entry.get("used", False)),
                        quality=entry.get("quality"),
                        payload_json=entry,
                    )
                )

    async def _handle_external_input(
        self, session: AsyncSession, kind: str, data: dict[str, Any], now: datetime, *, retain: bool
    ) -> None:
        env = ExternalInputEnvelope.model_validate(data)
        session.add(
            ExternalInput(
                received_at=now,
                source_timestamp=env.timestamp,
                valid_until=env.valid_until,
                device_id=self.device_id,
                input_kind=kind,
                status="retained_ignored" if retain else "valid",
                used=False,
                quality=env.quality,
                payload_json=env.payload or env.data or data,
            )
        )

    async def _handle_sem_mqtt(
        self, session: AsyncSession, suffix: str, data: dict[str, Any], now: datetime
    ) -> None:
        sem = SemSnapshotPayload.model_validate(data)
        self.state.last_sem_snapshot = sem
        ts = sem.timestamp or now
        session.add(
            ExternalInput(
                received_at=now,
                source_timestamp=ts,
                device_id=self.device_id,
                input_kind=f"sem:{suffix.split('/')[-1]}",
                status="valid",
                used=False,
                quality="observed",
                payload_json=data,
            )
        )
        if self.state.last_decision_payload is not None:
            await self._record_sem_comparison(session, ts, self.state.last_decision_payload, sem)

    async def _record_sem_comparison(
        self,
        session: AsyncSession,
        ts: datetime,
        dec: DecisionAuditPayload,
        sem: SemSnapshotPayload,
    ) -> None:
        batt_v = self.state.last_telemetry_payload.battery_v if self.state.last_telemetry_payload else None
        pt = compare_sem_and_esp(
            compared_at=ts,
            sem_decision=sem.sem_decision,
            sem_amps=sem.sem_amps,
            solarman_mode=sem.solarman_mode,
            esp_strategy=dec.strategy,
            esp_simulated_w=dec.simulated_w,
            actual_battery_w=dec.battery_w,
            actual_grid_w=dec.grid_w,
            actual_soc=dec.soc,
            battery_voltage_v=batt_v,
            thresholds=self.sem_thresholds,
        )
        stmt = (
            insert(SemComparison)
            .values(
                compared_at=pt.compared_at,
                device_id=self.device_id,
                sem_decision=pt.sem_decision,
                sem_mapped_strategy=pt.sem_mapped_strategy,
                sem_amps=pt.sem_amps,
                sem_target_w=pt.sem_target_w,
                esp_strategy=pt.esp_strategy,
                esp_simulated_w=pt.esp_simulated_w,
                actual_battery_w=pt.actual_battery_w,
                actual_grid_w=pt.actual_grid_w,
                actual_soc=pt.actual_soc,
                comparable=pt.comparable,
                concordant=pt.concordant,
                direction_match=pt.direction_match,
                strategy_match=pt.strategy_match,
                watt_diff=pt.watt_diff,
                reason=pt.reason,
            )
            .on_conflict_do_nothing()
        )
        await session.execute(stmt)
