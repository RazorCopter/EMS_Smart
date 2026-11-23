"""Seeds realistic SHADOW telemetry, decisions, events, and SEM comparisons for dev/testing."""
from __future__ import annotations

import asyncio
from datetime import datetime, timedelta, timezone
import math
import os
import random

from sqlalchemy.ext.asyncio import async_sessionmaker, create_async_engine

from companion_collector.ingestor import IngestionEngine


async def run_seed() -> None:
    dsn = os.getenv(
        "DATABASE_URL",
        "postgresql+asyncpg://afore_companion:afore_companion@localhost:15432/afore_companion",
    )
    db_engine = create_async_engine(dsn)
    session_factory = async_sessionmaker(db_engine, expire_on_commit=False)
    ingestor = IngestionEngine(session_factory, device_id="afore-ems")
    await ingestor.bootstrap_from_db()

    now = datetime.now(timezone.utc).replace(second=0, microsecond=0)
    start = now - timedelta(hours=18)
    rng = random.Random(42)

    seq = 1
    soc = 26.0
    step_s = 60
    total_steps = int((now - start).total_seconds() // step_s)

    for idx in range(total_steps):
        ts = start + timedelta(seconds=idx * step_s)
        hour = (ts.hour + 1) % 24  # approx Europe/Rome
        # Explicit 8-minute telemetry gap around hour 14 to test gap visualization
        if hour == 14 and 20 <= ts.minute < 28:
            continue

        solar_curve = max(0.0, math.sin(((hour + ts.minute / 60.0) - 6.5) / 11.0 * math.pi)) if 6 <= hour <= 18 else 0.0
        pv_w = round(3400.0 * (solar_curve ** 1.2) + (rng.uniform(-40, 40) if solar_curve > 0 else 0.0), 1)
        pv_w = max(0.0, pv_w)
        load_w = round(420.0 + 380.0 * (1 if 7 <= hour <= 22 else 0.2) + rng.uniform(-35, 65), 1)

        if 10 <= hour < 13:
            strategy = "CHARGE_FIRST"
            sem_dec = "ChgFst attivo"
            sem_amps = 50.0
            solarman_mode = "ChgFst"
        elif 13 <= hour < 15:
            strategy = "TARGET_SOC"
            sem_dec = "SelfUse (oggi basta)"
            sem_amps = 25.0
            solarman_mode = "SelfUse"
        else:
            strategy = "SELF_USE"
            sem_dec = "SelfUse (oggi basta)"
            sem_amps = 15.0
            solarman_mode = "SelfUse"

        surplus = pv_w - load_w
        if soc <= 20 and surplus < 0:
            batt_w = 0.0
            safety_reason = "SOC_RESERVE"
            reason = "SAFETY_LIMIT"
            cand_w = round(surplus)
            sim_w = 0
        else:
            batt_w = round(max(-1000.0, min(1000.0, surplus)), 1)
            safety_reason = "POWER_LIMIT" if abs(surplus) > 1000 else "NONE"
            reason = "SAFETY_LIMIT" if abs(surplus) > 1000 else ("CHARGE_REQUEST" if batt_w > 0 else "GRID_BALANCE")
            cand_w = round(surplus)
            sim_w = round(max(-1000.0, min(1000.0, surplus)))

        soc = round(max(20.0, min(98.0, soc + (batt_w * (step_s / 3600.0)) / 10240.0 * 100.0)), 1)
        grid_w = round(load_w + batt_w - pv_w, 1)

        tel_payload = {
            "quality": {
                "soc": {"age_ms": 950, "source": "measured", "state": "valid"},
                "pv_w": {"age_ms": 1100, "source": "measured", "state": "valid"},
                "load_w": {"age_ms": 1100, "source": "derived", "state": "valid"},
                "grid_w": {"age_ms": 1100, "source": "measured", "state": "valid"},
                "battery_w": {"age_ms": 950, "source": "measured", "state": "valid"},
            },
            "soc": soc,
            "battery_w": batt_w,
            "battery_v": round(51.4 + (soc / 100.0) * 3.0, 2),
            "battery_a": round(batt_w / 52.5, 2),
            "pv_w": pv_w,
            "pv1_w": round(pv_w * 0.51, 1),
            "pv2_w": round(pv_w * 0.49, 1),
            "grid_w": grid_w,
            "grid_v": 232.8,
            "hz": 50.0,
            "load_w": load_w,
            "inverter_w": round(pv_w - batt_w, 1),
            "temp_c": round(26.5 + (pv_w / 3500.0) * 9.0, 1),
            "reads": 100 + idx * 12,
            "errors": 0,
            "fault": False,
            "telemetry_valid": True,
            "shadow": True,
            "aux1": "OFF",
            "modbus_write_count": 0,
            "mqtt": True,
            "timestamp": ts.isoformat().replace("+00:00", "Z"),
            "uptime_ms": (idx + 1) * step_s * 1000,
            "time_source": "NTP",
            "time_valid": True,
        }
        await ingestor.ingest_rest_telemetry(tel_payload, received_at=ts)

        dec_payload = {
            "schema": 1,
            "source": "MEASURED",
            "seq": seq,
            "timestamp": ts.isoformat().replace("+00:00", "Z"),
            "uptime_ms": (idx + 1) * step_s * 1000,
            "time_source": "NTP",
            "time_valid": True,
            "strategy": strategy,
            "configured": "SELF_USE",
            "previous": "SELF_USE",
            "state": "RUN",
            "transition": "UNCHANGED",
            "soc": soc,
            "grid_w": grid_w,
            "quality": {"soc": "valid", "grid": "valid"},
            "battery_w": batt_w,
            "load_w": load_w,
            "pv_w": pv_w,
            "safety_reason": safety_reason,
            "limited_w": sim_w,
            "candidate_w": cand_w,
            "simulated_w": sim_w,
            "safety_min_w": 0 if soc <= 20 else -1000,
            "safety_max_w": 1000,
            "missing_wh": None,
            "average_w": None,
            "unreachable": False,
            "reason": reason,
            "executed": False,
            "applied": None,
            "aux1": "OFF",
            "modbus_write_count": 0,
            "ha_inputs": {"ha_online": True},
        }
        await ingestor.ingest_rest_decisions([dec_payload], received_at=ts)
        # Re-run live decision handler path on last few items for interval & comparison demo
        if idx % 3 == 0:
            await ingestor.ingest_sem_snapshot(
                {
                    "timestamp": ts.isoformat().replace("+00:00", "Z"),
                    "sem_decision": sem_dec,
                    "sem_amps": sem_amps,
                    "solarman_mode": solarman_mode,
                    "sem_automation_enabled": True,
                    "soc": soc,
                },
                received_at=ts,
            )
        seq += 1

    await db_engine.dispose()
    print(f"Seeded {seq - 1} demo records up to {now.isoformat()}")


if __name__ == "__main__":
    asyncio.run(run_seed())
