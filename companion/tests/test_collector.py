from __future__ import annotations

from datetime import datetime, timedelta, timezone
import json
import os
import pytest
from sqlalchemy import delete, select
from sqlalchemy.ext.asyncio import async_sessionmaker, create_async_engine

from companion_collector.ingestor import IngestionEngine
from companion_db.models import EmsDecision, EmsEvent, MqttMessage, TelemetrySample


@pytest.mark.asyncio
async def test_collector_ingestion_dedup_out_of_order_and_reboot():
    dsn = os.getenv("TEST_DATABASE_URL")
    if not dsn:
        pytest.skip("Set TEST_DATABASE_URL (e.g. postgresql+asyncpg://...) to run DB integration test")

    engine = create_async_engine(dsn)
    session_factory = async_sessionmaker(engine, expire_on_commit=False)
    test_dev = "test-collector-dev"
    ingestor = IngestionEngine(session_factory, device_id=test_dev)

    t0 = datetime(2026, 11, 19, 10, 0, 0, tzinfo=timezone.utc)
    tel_payload = {
        "soc": 42,
        "pv_w": 1500,
        "load_w": 500,
        "battery_w": 900,
        "grid_w": -100,
        "temp_c": None,
        "telemetry_valid": True,
        "modbus_write_count": 0,
        "timestamp": t0.isoformat(),
        "uptime_ms": 10000,
        "time_valid": True,
        "time_source": "NTP",
    }

    try:
        # Duplicate telemetry ingestion must deduplicate cleanly
        await ingestor.ingest_mqtt_message("afore/afore-ems/v1/telemetry", json.dumps(tel_payload).encode(), received_at=t0)
        await ingestor.ingest_mqtt_message("afore/afore-ems/v1/telemetry", json.dumps(tel_payload).encode(), received_at=t0)

        # Out-of-order earlier sample
        t_earlier = t0 - timedelta(seconds=30)
        await ingestor.ingest_mqtt_message(
            "afore/afore-ems/v1/telemetry",
            json.dumps({**tel_payload, "timestamp": t_earlier.isoformat(), "uptime_ms": 5000}).encode(),
            received_at=t0,
        )

        # Malformed MQTT JSON must be logged as schema_valid=False without crashing collector
        await ingestor.ingest_mqtt_message("afore/afore-ems/v1/telemetry", b"{broken json", received_at=t0)

        base_dec = {
            "schema": 1,
            "source": "MEASURED",
            "seq": 10,
            "timestamp": t0.isoformat(),
            "uptime_ms": 10000,
            "time_source": "NTP",
            "time_valid": True,
            "strategy": "SELF_USE",
            "configured": "SELF_USE",
            "previous": "SELF_USE",
            "state": "RUN",
            "transition": "UNCHANGED",
            "soc": 42,
            "grid_w": -100,
            "battery_w": 900,
            "load_w": 500,
            "pv_w": 1500,
            "candidate_w": 1000,
            "limited_w": 1000,
            "simulated_w": 1000,
            "safety_min_w": -1000,
            "safety_max_w": 1000,
            "safety_reason": "NONE",
            "reason": "GRID_BALANCE",
            "executed": False,
            "applied": None,
            "modbus_write_count": 0,
        }
        await ingestor.ingest_mqtt_message("afore/afore-ems/v1/ems/decision", json.dumps(base_dec).encode(), received_at=t0)
        # Gap: seq jumps from 10 to 15
        await ingestor.ingest_mqtt_message(
            "afore/afore-ems/v1/ems/decision",
            json.dumps({**base_dec, "seq": 15, "timestamp": (t0 + timedelta(seconds=25)).isoformat(), "uptime_ms": 35000}).encode(),
            received_at=t0 + timedelta(seconds=25),
        )
        # Reboot: seq resets to 1 and uptime_ms resets to 2000
        await ingestor.ingest_mqtt_message(
            "afore/afore-ems/v1/ems/decision",
            json.dumps({**base_dec, "seq": 1, "timestamp": (t0 + timedelta(seconds=40)).isoformat(), "uptime_ms": 2000}).encode(),
            received_at=t0 + timedelta(seconds=40),
        )

        async with session_factory() as session:
            tels = (
                await session.execute(select(TelemetrySample).where(TelemetrySample.device_id == test_dev))
            ).scalars().all()
            assert len(tels) == 2
            assert all(t.temp_c is None for t in tels)

            decs = (
                await session.execute(select(EmsDecision).where(EmsDecision.device_id == test_dev))
            ).scalars().all()
            assert len(decs) == 3

            events = (
                await session.execute(select(EmsEvent).where(EmsEvent.device_id == test_dev))
            ).scalars().all()
            types = {e.event_type for e in events}
            assert "DECISION_GAP" in types
            assert "ESP_REBOOT" in types

            bad_msgs = (
                await session.execute(
                    select(MqttMessage).where(MqttMessage.schema_valid.is_(False), MqttMessage.received_at == t0)
                )
            ).scalars().all()
            assert len(bad_msgs) >= 1
    finally:
        async with session_factory() as session:
            await session.execute(delete(TelemetrySample).where(TelemetrySample.device_id == test_dev))
            await session.execute(delete(EmsDecision).where(EmsDecision.device_id == test_dev))
            await session.execute(delete(EmsEvent).where(EmsEvent.device_id == test_dev))
            await session.commit()
        await engine.dispose()
