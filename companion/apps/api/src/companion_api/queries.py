from __future__ import annotations

from datetime import datetime, timedelta, timezone
from typing import Any
from zoneinfo import ZoneInfo

from sqlalchemy import Integer, desc, func, select, text
from sqlalchemy.ext.asyncio import AsyncSession

from companion_db.models import (
    AuxEvent,
    CompanionSetting,
    ConfigurationRevision,
    Device,
    EmsDecision,
    EmsEvent,
    EmsStrategyInterval,
    EnergySample,
    ExternalInput,
    SemComparison,
    SystemHealth,
    TelemetrySample,
)
from energy_domain import (
    EnergyTotals,
    SemComparisonThresholds,
    compute_kpis,
    compute_setpoint_churn,
    summarize_comparisons,
)
from energy_domain.sem_compare import ComparisonPoint


def resolve_window(
    range_name: str,
    *,
    tz_name: str = "Europe/Rome",
    start_iso: str | None = None,
    end_iso: str | None = None,
    now_utc: datetime | None = None,
) -> tuple[datetime, datetime, str]:
    now = now_utc or datetime.now(timezone.utc)
    tz = ZoneInfo(tz_name)
    local_now = now.astimezone(tz)
    today_start = local_now.replace(hour=0, minute=0, second=0, microsecond=0).astimezone(timezone.utc)

    if range_name == "today":
        return today_start, now, "1m"
    if range_name == "yesterday":
        y_start = (local_now.replace(hour=0, minute=0, second=0, microsecond=0) - timedelta(days=1)).astimezone(timezone.utc)
        return y_start, today_start, "1m"
    if range_name == "7d":
        return now - timedelta(days=7), now, "15m"
    if range_name == "30d":
        return now - timedelta(days=30), now, "1h"
    if range_name == "12m":
        return now - timedelta(days=365), now, "1d"
    if range_name == "custom" and start_iso and end_iso:
        s = datetime.fromisoformat(start_iso.replace("Z", "+00:00"))
        e = datetime.fromisoformat(end_iso.replace("Z", "+00:00"))
        s = s if s.tzinfo else s.replace(tzinfo=timezone.utc)
        e = e if e.tzinfo else e.replace(tzinfo=timezone.utc)
        span_h = (e - s).total_seconds() / 3600.0
        bucket = "1m" if span_h <= 48 else ("15m" if span_h <= 24 * 10 else ("1h" if span_h <= 24 * 60 else "1d"))
        return s, e, bucket
    return today_start, now, "1m"


def _telemetry_to_dict(row: TelemetrySample | None, now: datetime) -> dict[str, Any] | None:
    if row is None:
        return None
    age_s = max(0.0, round((now - row.source_timestamp).total_seconds(), 1))
    return {
        "source_timestamp": row.source_timestamp.isoformat(),
        "received_at": row.received_at.isoformat() if row.received_at else None,
        "age_s": age_s,
        "stale": age_s > 15.0,
        "boot_id": row.boot_id,
        "uptime_ms": row.uptime_ms,
        "time_source": row.time_source,
        "time_valid": row.time_valid,
        "soc": row.soc,
        "battery_w": row.battery_w,
        "battery_v": row.battery_v,
        "battery_a": row.battery_a,
        "pv_w": row.pv_w,
        "pv1_v": row.pv1_v,
        "pv1_a": row.pv1_a,
        "pv1_w": row.pv1_w,
        "pv2_v": row.pv2_v,
        "pv2_a": row.pv2_a,
        "pv2_w": row.pv2_w,
        "grid_w": row.grid_w,
        "grid_v": row.grid_v,
        "hz": row.hz,
        "load_w": row.load_w,
        "inverter_w": row.inverter_w,
        "temp_c": row.temp_c,
        "reads": row.reads,
        "errors": row.errors,
        "fault": row.fault,
        "telemetry_valid": row.telemetry_valid,
        "shadow": True,
        "aux1": row.aux1,
        "modbus_write_count": 0,
        "mqtt_connected": row.mqtt_connected,
        "quality": row.quality_json,
        "provenance": row.provenance,
    }


def _decision_to_dict(row: EmsDecision | None) -> dict[str, Any] | None:
    if row is None:
        return None
    return {
        "id": f"{row.boot_id}:{row.seq}",
        "seq": row.seq,
        "boot_id": row.boot_id,
        "source_timestamp": row.source_timestamp.isoformat(),
        "received_at": row.received_at.isoformat() if row.received_at else None,
        "uptime_ms": row.uptime_ms,
        "time_source": row.time_source,
        "time_valid": row.time_valid,
        "data_source": row.data_source,
        "strategy": row.strategy,
        "configured_strategy": row.configured_strategy,
        "previous_strategy": row.previous_strategy,
        "state": row.state,
        "transition": row.transition,
        "soc": row.soc,
        "grid_w": row.grid_w,
        "battery_w": row.battery_w,
        "load_w": row.load_w,
        "pv_w": row.pv_w,
        "candidate_w": row.candidate_w,
        "limited_w": row.limited_w,
        "simulated_w": row.simulated_w,
        "safety_min_w": row.safety_min_w,
        "safety_max_w": row.safety_max_w,
        "missing_wh": row.missing_wh,
        "average_w": row.average_w,
        "unreachable": row.unreachable,
        "safety_reason": row.safety_reason,
        "reason": row.reason,
        "executed": False,
        "applied": None,
        "aux1": row.aux1,
        "modbus_write_count": 0,
        "quality": row.quality_json,
        "ha_inputs": row.ha_inputs_json,
    }


async def get_system_status(session: AsyncSession, device_id: str) -> dict[str, Any]:
    rows = (
        await session.execute(select(SystemHealth).where(SystemHealth.device_id == device_id))
    ).scalars().all()
    by_comp = {r.component: {"status": r.status, "recorded_at": r.recorded_at.isoformat(), "detail": r.detail_json} for r in rows}

    # Ensure all 8 expected components are always represented without fabricating online status
    expected = ["esp32", "mqtt", "home_assistant", "modbus", "ntp", "collector", "database", "aux1"]
    components: dict[str, Any] = {}
    for comp in expected:
        if comp in by_comp:
            components[comp] = by_comp[comp]
        elif comp == "database":
            components[comp] = {"status": "online", "recorded_at": datetime.now(timezone.utc).isoformat(), "detail": {"engine": "timescaledb"}}
        else:
            components[comp] = {"status": "unknown", "recorded_at": None, "detail": {}}

    device = await session.get(Device, device_id)
    return {
        "device_id": device_id,
        "operating_mode": "SHADOW",
        "executed": False,
        "modbus_write_count": 0,
        "firmware_version": device.firmware_version if device else None,
        "last_seen_at": device.last_seen_at.isoformat() if (device and device.last_seen_at) else None,
        "components": components,
    }


async def query_energy_kpi(
    session: AsyncSession,
    device_id: str,
    start: datetime,
    end: datetime,
    *,
    battery_capacity_wh: float = 10240.0,
) -> dict[str, Any]:
    row = (
        await session.execute(
            select(
                func.sum(EnergySample.pv_wh).label("pv_wh"),
                func.sum(EnergySample.load_wh).label("load_wh"),
                func.sum(EnergySample.grid_import_wh).label("grid_import_wh"),
                func.sum(EnergySample.grid_export_wh).label("grid_export_wh"),
                func.sum(EnergySample.battery_charge_wh).label("battery_charge_wh"),
                func.sum(EnergySample.battery_discharge_wh).label("battery_discharge_wh"),
                func.sum(EnergySample.self_consumed_pv_wh).label("self_consumed_pv_wh"),
                func.sum(EnergySample.valid_seconds).label("valid_seconds"),
                func.sum(EnergySample.gap_seconds).label("gap_seconds"),
            ).where(
                EnergySample.device_id == device_id,
                EnergySample.bucket_start >= start,
                EnergySample.bucket_start < end,
            )
        )
    ).one()

    valid_s = float(row.valid_seconds or 0.0)
    gap_s = float(row.gap_seconds or 0.0)
    pv_wh = float(row.pv_wh) if row.pv_wh is not None else None
    load_wh = float(row.load_wh) if row.load_wh is not None else None
    imp_wh = float(row.grid_import_wh) if row.grid_import_wh is not None else None
    exp_wh = float(row.grid_export_wh) if row.grid_export_wh is not None else None
    chg_wh = float(row.battery_charge_wh) if row.battery_charge_wh is not None else None
    dis_wh = float(row.battery_discharge_wh) if row.battery_discharge_wh is not None else None
    sc_wh = float(row.self_consumed_pv_wh) if row.self_consumed_pv_wh is not None else None

    losses_wh: float | None = None
    if all(v is not None for v in (pv_wh, load_wh, imp_wh, exp_wh, chg_wh, dis_wh)):
        inflow = (pv_wh or 0.0) + (imp_wh or 0.0) + (dis_wh or 0.0)
        outflow = (load_wh or 0.0) + (exp_wh or 0.0) + (chg_wh or 0.0)
        losses_wh = round(max(0.0, inflow - outflow), 3)

    totals = EnergyTotals(
        pv_wh=pv_wh,
        load_wh=load_wh,
        grid_import_wh=imp_wh,
        grid_export_wh=exp_wh,
        battery_charge_wh=chg_wh,
        battery_discharge_wh=dis_wh,
        self_consumed_pv_wh=sc_wh,
        estimated_losses_wh=losses_wh,
        valid_seconds=valid_s,
        gap_seconds=gap_s,
    )
    window_s = max(1.0, (end - start).total_seconds())
    kpi = compute_kpis(totals, battery_capacity_wh=battery_capacity_wh, expected_window_seconds=window_s)
    return {
        "pv_kwh": kpi.pv_kwh,
        "load_kwh": kpi.load_kwh,
        "grid_import_kwh": kpi.grid_import_kwh,
        "grid_export_kwh": kpi.grid_export_kwh,
        "battery_charge_kwh": kpi.battery_charge_kwh,
        "battery_discharge_kwh": kpi.battery_discharge_kwh,
        "self_consumed_pv_kwh": kpi.self_consumed_pv_kwh,
        "estimated_losses_kwh": kpi.estimated_losses_kwh,
        "self_consumption_pct": kpi.self_consumption_pct,
        "self_sufficiency_pct": kpi.self_sufficiency_pct,
        "equivalent_cycles": kpi.equivalent_cycles,
        "battery_throughput_kwh": kpi.battery_throughput_kwh,
        "data_availability_pct": kpi.data_availability_pct,
        "valid_seconds": kpi.valid_seconds,
        "gap_seconds": kpi.gap_seconds,
        "formulas": {
            "self_consumption": "FV consumato localmente (inclusa carica batteria da FV) / Produzione FV",
            "self_sufficiency": "(Consumo totale - Importazione rete) / Consumo totale",
            "equivalent_cycles": "(Energia caricata + Energia scaricata) / (2 * Capacità utile)",
        },
    }


async def query_hourly_sparklines(
    session: AsyncSession, device_id: str, start: datetime, end: datetime, tz_name: str
) -> list[dict[str, Any]]:
    rows = (
        await session.execute(
            text(
                """
                SELECT
                    date_trunc('hour', bucket_start AT TIME ZONE :tz) AS hr,
                    SUM(pv_wh) / 1000.0 AS pv_kwh,
                    SUM(load_wh) / 1000.0 AS load_kwh,
                    SUM(battery_charge_wh) / 1000.0 AS chg_kwh,
                    SUM(grid_import_wh) / 1000.0 AS imp_kwh
                FROM energy_samples
                WHERE device_id = :dev AND bucket_start >= :s AND bucket_start < :e
                GROUP BY hr
                ORDER BY hr ASC
                """
            ),
            {"tz": tz_name, "dev": device_id, "s": start, "e": end},
        )
    ).fetchall()
    return [
        {
            "hour": r.hr.strftime("%H:%M") if r.hr else "",
            "pv_kwh": round(float(r.pv_kwh), 3) if r.pv_kwh is not None else None,
            "load_kwh": round(float(r.load_kwh), 3) if r.load_kwh is not None else None,
            "chg_kwh": round(float(r.chg_kwh), 3) if r.chg_kwh is not None else None,
            "imp_kwh": round(float(r.imp_kwh), 3) if r.imp_kwh is not None else None,
        }
        for r in rows
    ]


async def get_live_payload(
    session: AsyncSession,
    device_id: str,
    *,
    tz_name: str = "Europe/Rome",
    battery_capacity_wh: float = 10240.0,
) -> dict[str, Any]:
    now = datetime.now(timezone.utc)
    tel_row = (
        await session.execute(
            select(TelemetrySample)
            .where(TelemetrySample.device_id == device_id)
            .order_by(desc(TelemetrySample.source_timestamp))
            .limit(1)
        )
    ).scalar_one_or_none()
    dec_row = (
        await session.execute(
            select(EmsDecision)
            .where(EmsDecision.device_id == device_id)
            .order_by(desc(EmsDecision.source_timestamp), desc(EmsDecision.seq))
            .limit(1)
        )
    ).scalar_one_or_none()
    cfg_row = (
        await session.execute(
            select(ConfigurationRevision)
            .where(ConfigurationRevision.device_id == device_id)
            .order_by(desc(ConfigurationRevision.recorded_at))
            .limit(1)
        )
    ).scalar_one_or_none()

    today_s, today_e, _ = resolve_window("today", tz_name=tz_name, now_utc=now)
    yest_s, yest_e, _ = resolve_window("yesterday", tz_name=tz_name, now_utc=now)
    kpi_today = await query_energy_kpi(session, device_id, today_s, today_e, battery_capacity_wh=battery_capacity_wh)
    kpi_yesterday = await query_energy_kpi(session, device_id, yest_s, yest_e, battery_capacity_wh=battery_capacity_wh)
    sparklines = await query_hourly_sparklines(session, device_id, today_s, today_e, tz_name)

    sys_status = await get_system_status(session, device_id)
    recent_events = (
        await session.execute(
            select(EmsEvent)
            .where(EmsEvent.device_id == device_id)
            .order_by(desc(EmsEvent.occurred_at))
            .limit(6)
        )
    ).scalars().all()

    return {
        "generated_at": now.isoformat(),
        "timezone": tz_name,
        "operating_mode": "SHADOW",
        "banner": {
            "title": "SHADOW – Decisioni simulate",
            "subtitle": "Nessun comando applicato all'inverter",
            "executed": False,
            "modbus_write_count": 0,
        },
        "telemetry": _telemetry_to_dict(tel_row, now),
        "decision": _decision_to_dict(dec_row),
        "config": cfg_row.config_json if cfg_row else None,
        "kpi_today": kpi_today,
        "kpi_yesterday": kpi_yesterday,
        "sparklines": sparklines,
        "system": sys_status,
        "recent_events": [
            {
                "id": e.id,
                "occurred_at": e.occurred_at.isoformat(),
                "event_type": e.event_type,
                "severity": e.severity,
                "title": e.title,
                "detail": e.detail,
            }
            for e in recent_events
        ],
    }


async def query_history(
    session: AsyncSession,
    device_id: str,
    start: datetime,
    end: datetime,
    bucket: str,
) -> dict[str, Any]:
    bucket_delta = {
        "1m": timedelta(minutes=1),
        "5m": timedelta(minutes=5),
        "15m": timedelta(minutes=15),
        "1h": timedelta(hours=1),
        "1d": timedelta(days=1),
    }.get(bucket, timedelta(minutes=1))

    rows = (
        await session.execute(
            text(
                """
                WITH buckets AS (
                    SELECT generate_series(
                        time_bucket(:interval, CAST(:s AS timestamptz)),
                        time_bucket(:interval, CAST(:e AS timestamptz)),
                        :interval
                    ) AS bucket
                ),
                tel AS (
                    SELECT
                        time_bucket(:interval, source_timestamp) AS bucket,
                        AVG(pv_w) AS pv_w,
                        AVG(load_w) AS load_w,
                        AVG(battery_w) AS battery_w,
                        AVG(grid_w) AS grid_w,
                        AVG(soc) AS soc,
                        COUNT(*) AS sample_count
                    FROM telemetry_samples
                    WHERE device_id = :dev AND source_timestamp >= :s AND source_timestamp <= :e
                    GROUP BY 1
                ),
                dec AS (
                    SELECT
                        time_bucket(:interval, source_timestamp) AS bucket,
                        AVG(simulated_w) AS simulated_w,
                        AVG(safety_min_w) AS safety_min_w,
                        AVG(safety_max_w) AS safety_max_w,
                        MAX(strategy) AS strategy,
                        MAX(safety_reason) AS safety_reason
                    FROM ems_decisions
                    WHERE device_id = :dev AND source_timestamp >= :s AND source_timestamp <= :e
                    GROUP BY 1
                ),
                sem AS (
                    SELECT
                        time_bucket(:interval, compared_at) AS bucket,
                        MAX(sem_decision) AS sem_decision,
                        AVG(sem_target_w) AS sem_target_w
                    FROM sem_comparisons
                    WHERE device_id = :dev AND compared_at >= :s AND compared_at <= :e
                    GROUP BY 1
                )
                SELECT
                    b.bucket,
                    tel.pv_w,
                    tel.load_w,
                    tel.battery_w,
                    tel.grid_w,
                    tel.soc,
                    COALESCE(tel.sample_count, 0) AS sample_count,
                    dec.simulated_w,
                    dec.safety_min_w,
                    dec.safety_max_w,
                    dec.strategy,
                    dec.safety_reason,
                    sem.sem_decision,
                    sem.sem_target_w
                FROM buckets b
                LEFT JOIN tel ON tel.bucket = b.bucket
                LEFT JOIN dec ON dec.bucket = b.bucket
                LEFT JOIN sem ON sem.bucket = b.bucket
                ORDER BY b.bucket ASC
                """
            ),
            {"interval": bucket_delta, "dev": device_id, "s": start, "e": end},
        )
    ).fetchall()

    series = []
    missing_buckets = 0
    for r in rows:
        has_data = int(r.sample_count or 0) > 0
        if not has_data:
            missing_buckets += 1
        series.append(
            {
                "timestamp": r.bucket.isoformat(),
                "pv_w": round(float(r.pv_w), 1) if r.pv_w is not None else None,
                "load_w": round(float(r.load_w), 1) if r.load_w is not None else None,
                "battery_w": round(float(r.battery_w), 1) if r.battery_w is not None else None,
                "grid_w": round(float(r.grid_w), 1) if r.grid_w is not None else None,
                "soc": round(float(r.soc), 1) if r.soc is not None else None,
                "simulated_w": round(float(r.simulated_w), 1) if r.simulated_w is not None else None,
                "safety_min_w": round(float(r.safety_min_w), 1) if r.safety_min_w is not None else None,
                "safety_max_w": round(float(r.safety_max_w), 1) if r.safety_max_w is not None else None,
                "strategy": r.strategy,
                "safety_reason": r.safety_reason,
                "sem_decision": r.sem_decision,
                "sem_target_w": round(float(r.sem_target_w), 1) if r.sem_target_w is not None else None,
                "quality": "valid" if has_data else "missing",
            }
        )

    return {
        "start": start.isoformat(),
        "end": end.isoformat(),
        "bucket": bucket,
        "total_buckets": len(series),
        "missing_buckets": missing_buckets,
        "series": series,
    }


async def query_decisions_and_intervals(
    session: AsyncSession,
    device_id: str,
    start: datetime,
    end: datetime,
    *,
    strategy: str | None = None,
    safety_only: bool = False,
    limit: int = 200,
    offset: int = 0,
) -> dict[str, Any]:
    q = select(EmsDecision).where(
        EmsDecision.device_id == device_id,
        EmsDecision.source_timestamp >= start,
        EmsDecision.source_timestamp <= end,
    )
    if strategy:
        q = q.where(EmsDecision.strategy == strategy)
    if safety_only:
        q = q.where(EmsDecision.safety_reason != "NONE")

    total = (await session.execute(select(func.count()).select_from(q.subquery()))).scalar_one()
    rows = (
        await session.execute(
            q.order_by(desc(EmsDecision.source_timestamp), desc(EmsDecision.seq)).limit(limit).offset(offset)
        )
    ).scalars().all()

    intervals = (
        await session.execute(
            select(EmsStrategyInterval)
            .where(
                EmsStrategyInterval.device_id == device_id,
                EmsStrategyInterval.ended_at >= start,
                EmsStrategyInterval.started_at <= end,
            )
            .order_by(EmsStrategyInterval.started_at.asc())
        )
    ).scalars().all()

    return {
        "total": int(total),
        "limit": limit,
        "offset": offset,
        "decisions": [_decision_to_dict(r) for r in rows],
        "intervals": [
            {
                "id": iv.id,
                "boot_id": iv.boot_id,
                "strategy": iv.strategy,
                "configured_strategy": iv.configured_strategy,
                "state": iv.state,
                "started_at": iv.started_at.isoformat(),
                "ended_at": iv.ended_at.isoformat(),
                "duration_s": iv.duration_s,
                "decision_count": iv.decision_count,
            }
            for iv in intervals
        ],
    }


async def query_comparison_summary(
    session: AsyncSession,
    device_id: str,
    start: datetime,
    end: datetime,
    *,
    watt_threshold: float = 100.0,
) -> dict[str, Any]:
    rows = (
        await session.execute(
            select(SemComparison)
            .where(
                SemComparison.device_id == device_id,
                SemComparison.compared_at >= start,
                SemComparison.compared_at <= end,
            )
            .order_by(SemComparison.compared_at.asc())
        )
    ).scalars().all()

    # Also count safety interventions and ESP setpoint churn in the same window
    safety_count = int(
        (
            await session.execute(
                select(func.count())
                .select_from(EmsDecision)
                .where(
                    EmsDecision.device_id == device_id,
                    EmsDecision.source_timestamp >= start,
                    EmsDecision.source_timestamp <= end,
                    EmsDecision.safety_reason != "NONE",
                )
            )
        ).scalar_one()
    )
    esp_pts = (
        await session.execute(
            select(EmsDecision.source_timestamp, EmsDecision.simulated_w)
            .where(
                EmsDecision.device_id == device_id,
                EmsDecision.source_timestamp >= start,
                EmsDecision.source_timestamp <= end,
            )
            .order_by(EmsDecision.source_timestamp.asc())
        )
    ).fetchall()
    churn = compute_setpoint_churn([(r[0], r[1]) for r in esp_pts], delta_threshold_w=watt_threshold)

    # Recompute concordance dynamically against requested threshold if non-default
    pts: list[ComparisonPoint] = []
    for r in rows:
        conc = r.concordant
        if r.comparable and r.watt_diff is not None:
            conc = bool(r.strategy_match and (r.direction_match is not False) and r.watt_diff <= watt_threshold)
        pts.append(
            ComparisonPoint(
                compared_at=r.compared_at,
                sem_decision=r.sem_decision,
                sem_mapped_strategy=r.sem_mapped_strategy,
                sem_amps=r.sem_amps,
                sem_target_w=r.sem_target_w,
                esp_strategy=r.esp_strategy,
                esp_simulated_w=r.esp_simulated_w,
                actual_battery_w=r.actual_battery_w,
                actual_grid_w=r.actual_grid_w,
                actual_soc=r.actual_soc,
                comparable=r.comparable,
                concordant=conc,
                direction_match=r.direction_match,
                strategy_match=r.strategy_match,
                watt_diff=r.watt_diff,
                reason=r.reason,
            )
        )

    agg = summarize_comparisons(
        pts,
        safety_interventions=safety_count,
        setpoint_oscillations_per_hour=churn,
    )
    return {
        "disclaimer": "SEM è il controllore reale attuale. L'ESP produce decisioni simulate (SHADOW) per analisi e validazione.",
        "thresholds": {
            "watt_threshold": watt_threshold,
            "criteria": [
                "Stessa direzione (carica / scarica / neutro entro ±20 W)",
                f"Differenza assoluta setpoint ≤ {int(watt_threshold)} W",
                "Stessa scelta strategica quando confrontabile",
            ],
        },
        "kpi": {
            "total_points": agg.total_points,
            "comparable_points": agg.comparable_points,
            "concordant_points": agg.concordant_points,
            "divergent_points": agg.divergent_points,
            "concordance_pct": agg.concordance_pct,
            "mean_watt_diff": agg.mean_watt_diff,
            "p95_watt_diff": agg.p95_watt_diff,
            "false_positives": agg.false_positives,
            "missed_opportunities": agg.missed_opportunities,
            "safety_interventions": agg.safety_interventions,
            "setpoint_oscillations_per_hour": agg.setpoint_oscillations_per_hour,
            "strategy_share": agg.strategy_share,
        },
        "series": [
            {
                "timestamp": p.compared_at.isoformat(),
                "sem_decision": p.sem_decision,
                "sem_mapped_strategy": p.sem_mapped_strategy,
                "sem_amps": p.sem_amps,
                "sem_target_w": p.sem_target_w,
                "esp_strategy": p.esp_strategy,
                "esp_simulated_w": p.esp_simulated_w,
                "actual_battery_w": p.actual_battery_w,
                "actual_grid_w": p.actual_grid_w,
                "actual_soc": p.actual_soc,
                "comparable": p.comparable,
                "concordant": p.concordant,
                "watt_diff": p.watt_diff,
                "reason": p.reason,
            }
            for p in pts[-500:]
        ],
    }


async def query_battery_summary(
    session: AsyncSession,
    device_id: str,
    start: datetime,
    end: datetime,
    *,
    battery_capacity_wh: float = 10240.0,
) -> dict[str, Any]:
    now = datetime.now(timezone.utc)
    latest = (
        await session.execute(
            select(TelemetrySample)
            .where(TelemetrySample.device_id == device_id)
            .order_by(desc(TelemetrySample.source_timestamp))
            .limit(1)
        )
    ).scalar_one_or_none()
    cfg = (
        await session.execute(
            select(ConfigurationRevision)
            .where(ConfigurationRevision.device_id == device_id)
            .order_by(desc(ConfigurationRevision.recorded_at))
            .limit(1)
        )
    ).scalar_one_or_none()

    agg = (
        await session.execute(
            select(
                func.min(TelemetrySample.soc).label("soc_min"),
                func.max(TelemetrySample.soc).label("soc_max"),
                func.avg(TelemetrySample.soc).label("soc_avg"),
                func.max(TelemetrySample.battery_w).label("max_charge_w"),
                func.min(TelemetrySample.battery_w).label("min_batt_w"),
            ).where(
                TelemetrySample.device_id == device_id,
                TelemetrySample.source_timestamp >= start,
                TelemetrySample.source_timestamp <= end,
            )
        )
    ).one()

    kpi = await query_energy_kpi(session, device_id, start, end, battery_capacity_wh=battery_capacity_wh)

    soc_history = (
        await session.execute(
            text(
                """
                SELECT
                    time_bucket('5 minutes', source_timestamp) AS bucket,
                    AVG(soc) AS soc,
                    AVG(battery_w) AS battery_w,
                    AVG(battery_v) AS battery_v,
                    AVG(battery_a) AS battery_a
                FROM telemetry_samples
                WHERE device_id = :dev AND source_timestamp >= :s AND source_timestamp <= :e
                GROUP BY 1
                ORDER BY 1 ASC
                """
            ),
            {"dev": device_id, "s": start, "e": end},
        )
    ).fetchall()

    sessions = (
        await session.execute(
            select(EmsStrategyInterval)
            .where(
                EmsStrategyInterval.device_id == device_id,
                EmsStrategyInterval.strategy.in_(["TARGET_SOC", "MAINTENANCE"]),
                EmsStrategyInterval.ended_at >= start,
                EmsStrategyInterval.started_at <= end,
            )
            .order_by(desc(EmsStrategyInterval.started_at))
            .limit(30)
        )
    ).scalars().all()

    reserve_events = (
        await session.execute(
            select(EmsEvent)
            .where(
                EmsEvent.device_id == device_id,
                EmsEvent.event_type == "SOC_LIMIT",
                EmsEvent.occurred_at >= start,
                EmsEvent.occurred_at <= end,
            )
            .order_by(desc(EmsEvent.occurred_at))
            .limit(30)
        )
    ).scalars().all()

    cfg_json = cfg.config_json if cfg else {}
    return {
        "specifications": {
            "model": {"value": "HA / Solarman Battery Profile", "provenance": "configured"},
            "capacity_wh": {"value": cfg_json.get("capacity_wh", 10240), "unit": "Wh", "provenance": "configured"},
            "nominal_v": {"value": round((cfg_json.get("nominal_mv", 51200) / 1000.0), 1), "unit": "V", "provenance": "configured"},
            "efficiency_pct": {"value": cfg_json.get("efficiency_pct", 90), "unit": "%", "provenance": "provisional"},
            "soh_pct": {"value": None, "unit": "%", "provenance": "unavailable"},
            "battery_temp_c": {"value": None, "unit": "°C", "provenance": "unavailable"},
        },
        "live": {
            "soc": {"value": latest.soc if latest else None, "unit": "%", "provenance": "measured" if latest and latest.soc is not None else "unavailable"},
            "power_w": {"value": latest.battery_w if latest else None, "unit": "W", "provenance": "measured" if latest and latest.battery_w is not None else "unavailable"},
            "voltage_v": {"value": latest.battery_v if latest else None, "unit": "V", "provenance": "measured" if latest and latest.battery_v is not None else "unavailable"},
            "current_a": {"value": latest.battery_a if latest else None, "unit": "A", "provenance": "measured" if latest and latest.battery_a is not None else "unavailable"},
            "updated_at": latest.source_timestamp.isoformat() if latest else None,
            "age_s": round((now - latest.source_timestamp).total_seconds(), 1) if latest else None,
        },
        "limits": {
            "configured": {
                "soc_min": cfg_json.get("soc_min", 20),
                "soc_max": cfg_json.get("soc_max", 100),
                "charge_w": cfg_json.get("charge_w", 1000),
                "discharge_w": cfg_json.get("discharge_w", 1000),
                "charge_a": cfg_json.get("charge_a", 50),
                "discharge_a": cfg_json.get("discharge_a", 50),
            },
            "effective_source": "SHADOW Safety Engine",
        },
        "stats": {
            "soc_min": round(float(agg.soc_min), 1) if agg.soc_min is not None else None,
            "soc_max": round(float(agg.soc_max), 1) if agg.soc_max is not None else None,
            "soc_avg": round(float(agg.soc_avg), 1) if agg.soc_avg is not None else None,
            "charge_kwh": kpi["battery_charge_kwh"],
            "discharge_kwh": kpi["battery_discharge_kwh"],
            "throughput_kwh": kpi["battery_throughput_kwh"],
            "equivalent_cycles": kpi["equivalent_cycles"],
            "max_charge_w": round(float(agg.max_charge_w), 1) if agg.max_charge_w is not None and agg.max_charge_w > 0 else None,
            "max_discharge_w": round(abs(float(agg.min_batt_w)), 1) if agg.min_batt_w is not None and agg.min_batt_w < 0 else None,
        },
        "history": [
            {
                "timestamp": r.bucket.isoformat(),
                "soc": round(float(r.soc), 1) if r.soc is not None else None,
                "battery_w": round(float(r.battery_w), 1) if r.battery_w is not None else None,
                "battery_v": round(float(r.battery_v), 2) if r.battery_v is not None else None,
                "battery_a": round(float(r.battery_a), 2) if r.battery_a is not None else None,
            }
            for r in soc_history
        ],
        "sessions": [
            {
                "id": s.id,
                "strategy": s.strategy,
                "state": s.state,
                "started_at": s.started_at.isoformat(),
                "ended_at": s.ended_at.isoformat(),
                "duration_s": s.duration_s,
            }
            for s in sessions
        ],
        "reserve_events": [
            {
                "id": e.id,
                "occurred_at": e.occurred_at.isoformat(),
                "title": e.title,
                "detail": e.detail,
            }
            for e in reserve_events
        ],
    }


async def query_analytics_report(
    session: AsyncSession,
    device_id: str,
    start: datetime,
    end: datetime,
    *,
    tz_name: str = "Europe/Rome",
    battery_capacity_wh: float = 10240.0,
    watt_threshold: float = 100.0,
) -> dict[str, Any]:
    kpi = await query_energy_kpi(session, device_id, start, end, battery_capacity_wh=battery_capacity_wh)
    comp = await query_comparison_summary(session, device_id, start, end, watt_threshold=watt_threshold)

    strategy_rows = (
        await session.execute(
            select(
                EmsDecision.strategy,
                func.count().label("cnt"),
                func.avg(EmsDecision.simulated_w).label("avg_sim_w"),
            )
            .where(
                EmsDecision.device_id == device_id,
                EmsDecision.source_timestamp >= start,
                EmsDecision.source_timestamp <= end,
            )
            .group_by(EmsDecision.strategy)
        )
    ).fetchall()

    quality_row = (
        await session.execute(
            select(
                func.count().label("samples"),
                func.sum(func.cast(TelemetrySample.fault, Integer)).label("fault_samples"),
                func.max(TelemetrySample.errors).label("max_errors"),
            ).where(
                TelemetrySample.device_id == device_id,
                TelemetrySample.source_timestamp >= start,
                TelemetrySample.source_timestamp <= end,
            )
        )
    ).one()

    events_rows = (
        await session.execute(
            select(EmsEvent.event_type, func.count().label("cnt"))
            .where(
                EmsEvent.device_id == device_id,
                EmsEvent.occurred_at >= start,
                EmsEvent.occurred_at <= end,
            )
            .group_by(EmsEvent.event_type)
        )
    ).fetchall()

    ha_valid_cnt = int(
        (
            await session.execute(
                select(func.count())
                .select_from(ExternalInput)
                .where(
                    ExternalInput.device_id == device_id,
                    ExternalInput.received_at >= start,
                    ExternalInput.received_at <= end,
                    ExternalInput.status == "valid",
                )
            )
        ).scalar_one()
    )
    ha_stale_cnt = int(
        (
            await session.execute(
                select(func.count())
                .select_from(ExternalInput)
                .where(
                    ExternalInput.device_id == device_id,
                    ExternalInput.received_at >= start,
                    ExternalInput.received_at <= end,
                    ExternalInput.status == "stale",
                )
            )
        ).scalar_one()
    )

    event_counts = {r.event_type: int(r.cnt) for r in events_rows}
    return {
        "period": {"start": start.isoformat(), "end": end.isoformat(), "timezone": tz_name},
        "operating_mode": "SHADOW",
        "energy": kpi,
        "ems": {
            "strategies": [
                {
                    "strategy": r.strategy,
                    "decisions": int(r.cnt),
                    "avg_simulated_w": round(float(r.avg_sim_w), 1) if r.avg_sim_w is not None else None,
                }
                for r in strategy_rows
            ],
            "safety_interventions": comp["kpi"]["safety_interventions"],
            "targets_reached": event_counts.get("TARGET_REACHED", 0),
            "targets_expired": event_counts.get("DEADLINE_EXPIRED", 0),
            "targets_unreachable": event_counts.get("TARGET_UNREACHABLE", 0),
            "setpoint_oscillations_per_hour": comp["kpi"]["setpoint_oscillations_per_hour"],
        },
        "comparison": comp["kpi"],
        "data_quality": {
            "telemetry_samples": int(quality_row.samples or 0),
            "availability_pct": kpi["data_availability_pct"],
            "gap_seconds": kpi["gap_seconds"],
            "fault_samples": int(quality_row.fault_samples or 0),
            "max_modbus_errors": int(quality_row.max_errors) if quality_row.max_errors is not None else None,
            "ha_valid_inputs": ha_valid_cnt,
            "ha_stale_inputs": ha_stale_cnt,
            "esp_reboots": event_counts.get("ESP_REBOOT", 0),
            "decision_gaps": event_counts.get("DECISION_GAP", 0),
        },
    }


async def load_companion_settings(session: AsyncSession) -> dict[str, Any]:
    rows = (await session.execute(select(CompanionSetting))).scalars().all()
    return {r.key: r.value_json for r in rows}
