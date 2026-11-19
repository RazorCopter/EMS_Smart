from __future__ import annotations

import asyncio
from contextlib import asynccontextmanager, suppress
import csv
from datetime import datetime, timezone
import io
import json
import logging
from typing import Any

import asyncpg
from fastapi import (
    Cookie,
    Depends,
    FastAPI,
    HTTPException,
    Query,
    Request,
    Response,
    WebSocket,
    WebSocketDisconnect,
    status,
)
from fastapi.responses import JSONResponse, StreamingResponse
import httpx
from prometheus_client import CONTENT_TYPE_LATEST, generate_latest
from pydantic import BaseModel, Field
from sqlalchemy import desc, select, text
from sqlalchemy.dialects.postgresql import insert
from sqlalchemy.ext.asyncio import AsyncSession, async_sessionmaker, create_async_engine

from companion_db.models import CompanionSetting, EmsDecision, EmsEvent, SystemHealth
from energy_domain import SecretRedactingFilter

from .auth import (
    CSRF_COOKIE_NAME,
    SESSION_COOKIE_NAME,
    SessionInfo,
    authenticate_user,
    ensure_bootstrap_users,
    login_rate_limiter,
    require_admin_csrf,
    require_viewer,
    sign_session,
    verify_session,
)
from .config import ApiSettings
from .pdf_export import build_simple_pdf
from .queries import (
    get_live_payload,
    get_system_status,
    load_companion_settings,
    query_analytics_report,
    query_battery_summary,
    query_comparison_summary,
    query_decisions_and_intervals,
    query_energy_kpi,
    query_history,
    resolve_window,
)

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s %(levelname)s [%(name)s] %(message)s",
)
for handler in logging.getLogger().handlers:
    handler.addFilter(SecretRedactingFilter())

logger = logging.getLogger("companion.api")


class LiveBroadcaster:
    """Broadcasts PostgreSQL NOTIFY afore_live events to connected WebSocket & SSE clients."""

    def __init__(self) -> None:
        self._subscribers: set[asyncio.Queue[str]] = set()

    def subscribe(self) -> asyncio.Queue[str]:
        q: asyncio.Queue[str] = asyncio.Queue(maxsize=64)
        self._subscribers.add(q)
        return q

    def unsubscribe(self, q: asyncio.Queue[str]) -> None:
        self._subscribers.discard(q)

    def publish(self, message: str) -> None:
        for q in list(self._subscribers):
            if q.full():
                with suppress(asyncio.QueueEmpty):
                    q.get_nowait()
            with suppress(asyncio.QueueFull):
                q.put_nowait(message)


async def pg_notify_listener(dsn: str, broadcaster: LiveBroadcaster) -> None:
    raw_dsn = dsn.replace("postgresql+asyncpg://", "postgresql://")
    while True:
        conn: asyncpg.Connection | None = None
        try:
            conn = await asyncpg.connect(raw_dsn)

            def _on_notify(_conn: Any, _pid: int, _channel: str, payload: str) -> None:
                broadcaster.publish(payload)

            await conn.add_listener("afore_live", _on_notify)
            logger.info("Listening on PostgreSQL NOTIFY afore_live")
            while True:
                await asyncio.sleep(30)
                await conn.execute("SELECT 1")
        except asyncio.CancelledError:
            raise
        except Exception as exc:
            logger.warning("NOTIFY listener interrupted (%s); retrying in 3s", exc)
            await asyncio.sleep(3)
        finally:
            if conn is not None:
                with suppress(Exception):
                    await conn.close()


class LoginRequest(BaseModel):
    username: str = Field(min_length=1, max_length=64)
    password: str = Field(min_length=1, max_length=128)


class SettingsPatchRequest(BaseModel):
    timezone: str | None = None
    locale: str | None = None
    theme: str | None = None
    units_power: str | None = None
    sem_concordance_watt_threshold: float | None = Field(default=None, ge=10.0, le=2000.0)
    retention_raw_mqtt_days: int | None = Field(default=None, ge=1, le=3650)
    retention_telemetry_days: int | None = Field(default=None, ge=1, le=3650)
    retention_agg_1m_days: int | None = Field(default=None, ge=7, le=3650)


@asynccontextmanager
async def lifespan(app: FastAPI):
    settings = ApiSettings()
    engine = create_async_engine(settings.database_url, pool_pre_ping=True, pool_size=10, max_overflow=10)
    session_factory = async_sessionmaker(engine, expire_on_commit=False)
    broadcaster = LiveBroadcaster()

    app.state.settings = settings
    app.state.db_engine = engine
    app.state.session_factory = session_factory
    app.state.broadcaster = broadcaster

    for attempt in range(1, 15):
        try:
            async with session_factory() as session:
                await ensure_bootstrap_users(
                    session,
                    admin_user=settings.companion_admin_user,
                    admin_password=settings.companion_admin_password,
                    viewer_user=settings.companion_viewer_user,
                    viewer_password=settings.companion_viewer_password,
                )
            break
        except Exception as exc:
            logger.info("Waiting for DB bootstrap (%d/15): %s", attempt, exc)
            await asyncio.sleep(2)

    listener_task = asyncio.create_task(pg_notify_listener(settings.database_url, broadcaster))
    try:
        yield
    finally:
        listener_task.cancel()
        with suppress(asyncio.CancelledError):
            await listener_task
        await engine.dispose()


app = FastAPI(
    title="Afore EMS Companion API",
    version="0.1.0",
    description="Read-only telemetry, history, decision audit, and SHADOW comparison API.",
    lifespan=lifespan,
)


async def get_db(request: Request) -> AsyncSession:
    async with request.app.state.session_factory() as session:
        yield session


@app.get("/api/v1/health")
async def health(request: Request, db: AsyncSession = Depends(get_db)) -> dict[str, Any]:
    db_ok = False
    try:
        await db.execute(text("SELECT 1"))
        db_ok = True
    except Exception:
        db_ok = False
    return {
        "status": "ok" if db_ok else "degraded",
        "database": "online" if db_ok else "offline",
        "operating_mode": "SHADOW",
        "read_only": True,
        "modbus_write_capable": False,
        "version": request.app.state.settings.companion_version,
        "timestamp": datetime.now(timezone.utc).isoformat(),
    }


@app.get("/metrics")
async def metrics() -> Response:
    return Response(content=generate_latest(), media_type=CONTENT_TYPE_LATEST)


@app.post("/api/v1/auth/login")
async def login(
    body: LoginRequest,
    request: Request,
    response: Response,
    db: AsyncSession = Depends(get_db),
) -> dict[str, Any]:
    client_ip = request.client.host if request.client else "unknown"
    login_rate_limiter.check(client_ip)

    user = await authenticate_user(db, body.username, body.password)
    if user is None:
        raise HTTPException(status_code=status.HTTP_401_UNAUTHORIZED, detail="Credenziali non valide")

    cfg: ApiSettings = request.app.state.settings
    token, csrf = sign_session(user.username, user.role, cfg.companion_secret_key, cfg.companion_session_ttl_hours)
    response.set_cookie(
        SESSION_COOKIE_NAME,
        token,
        httponly=True,
        samesite="lax",
        secure=cfg.companion_cookie_secure,
        max_age=cfg.companion_session_ttl_hours * 3600,
    )
    response.set_cookie(
        CSRF_COOKIE_NAME,
        csrf,
        httponly=False,
        samesite="lax",
        secure=cfg.companion_cookie_secure,
        max_age=cfg.companion_session_ttl_hours * 3600,
    )
    return {"username": user.username, "role": user.role, "csrf_token": csrf}


@app.post("/api/v1/auth/logout")
async def logout(response: Response) -> dict[str, bool]:
    response.delete_cookie(SESSION_COOKIE_NAME)
    response.delete_cookie(CSRF_COOKIE_NAME)
    return {"logged_out": True}


@app.get("/api/v1/auth/me")
async def auth_me(
    request: Request,
    session_cookie: str | None = Cookie(default=None, alias=SESSION_COOKIE_NAME),
) -> dict[str, Any]:
    info = verify_session(session_cookie, request.app.state.settings.companion_secret_key)
    if info is None:
        return {"authenticated": False, "username": None, "role": None, "csrf_token": None}
    return {"authenticated": True, "username": info.username, "role": info.role, "csrf_token": info.csrf_token}


@app.get("/api/v1/system/status")
async def system_status(
    request: Request,
    _user: SessionInfo = Depends(require_viewer),
    db: AsyncSession = Depends(get_db),
) -> dict[str, Any]:
    return await get_system_status(db, request.app.state.settings.device_id)


@app.get("/api/v1/live")
async def live(
    request: Request,
    _user: SessionInfo = Depends(require_viewer),
    db: AsyncSession = Depends(get_db),
) -> dict[str, Any]:
    cfg: ApiSettings = request.app.state.settings
    runtime = await load_companion_settings(db)
    tz_name = runtime.get("timezone", {}).get("value", cfg.companion_timezone)
    return await get_live_payload(
        db,
        cfg.device_id,
        tz_name=tz_name,
        battery_capacity_wh=cfg.battery_capacity_wh,
    )


@app.websocket("/api/v1/ws/live")
async def ws_live(websocket: WebSocket) -> None:
    cfg: ApiSettings = websocket.app.state.settings
    cookie = websocket.cookies.get(SESSION_COOKIE_NAME)
    if verify_session(cookie, cfg.companion_secret_key) is None:
        await websocket.close(code=4401)
        return

    await websocket.accept()
    q = websocket.app.state.broadcaster.subscribe()
    try:
        async with websocket.app.state.session_factory() as db:
            payload = await get_live_payload(db, cfg.device_id, tz_name=cfg.companion_timezone, battery_capacity_wh=cfg.battery_capacity_wh)
            await websocket.send_json({"type": "snapshot", "data": payload})

        while True:
            try:
                _event = await asyncio.wait_for(q.get(), timeout=15.0)
                async with websocket.app.state.session_factory() as db:
                    payload = await get_live_payload(db, cfg.device_id, tz_name=cfg.companion_timezone, battery_capacity_wh=cfg.battery_capacity_wh)
                await websocket.send_json({"type": "update", "data": payload})
            except asyncio.TimeoutError:
                await websocket.send_json({"type": "heartbeat", "timestamp": datetime.now(timezone.utc).isoformat()})
    except WebSocketDisconnect:
        pass
    finally:
        websocket.app.state.broadcaster.unsubscribe(q)


@app.get("/api/v1/stream/live")
async def sse_live(
    request: Request,
    _user: SessionInfo = Depends(require_viewer),
) -> StreamingResponse:
    cfg: ApiSettings = request.app.state.settings
    broadcaster: LiveBroadcaster = request.app.state.broadcaster
    q = broadcaster.subscribe()

    async def event_stream():
        try:
            async with request.app.state.session_factory() as db:
                snap = await get_live_payload(db, cfg.device_id, tz_name=cfg.companion_timezone, battery_capacity_wh=cfg.battery_capacity_wh)
                yield f"event: snapshot\ndata: {json.dumps(snap)}\n\n"
            while True:
                if await request.is_disconnected():
                    break
                try:
                    await asyncio.wait_for(q.get(), timeout=15.0)
                    async with request.app.state.session_factory() as db:
                        snap = await get_live_payload(db, cfg.device_id, tz_name=cfg.companion_timezone, battery_capacity_wh=cfg.battery_capacity_wh)
                    yield f"event: update\ndata: {json.dumps(snap)}\n\n"
                except asyncio.TimeoutError:
                    yield f"event: ping\ndata: {json.dumps({'ts': datetime.now(timezone.utc).isoformat()})}\n\n"
        finally:
            broadcaster.unsubscribe(q)

    return StreamingResponse(event_stream(), media_type="text/event-stream")


@app.get("/api/v1/history")
async def history(
    request: Request,
    range: str = Query(default="today", pattern="^(today|yesterday|7d|30d|12m|custom)$"),
    start: str | None = None,
    end: str | None = None,
    bucket: str = Query(default="auto", pattern="^(auto|1m|5m|15m|1h|1d)$"),
    _user: SessionInfo = Depends(require_viewer),
    db: AsyncSession = Depends(get_db),
) -> dict[str, Any]:
    cfg: ApiSettings = request.app.state.settings
    s, e, auto_bucket = resolve_window(range, tz_name=cfg.companion_timezone, start_iso=start, end_iso=end)
    chosen_bucket = auto_bucket if bucket == "auto" else bucket
    return await query_history(db, cfg.device_id, s, e, chosen_bucket)


@app.get("/api/v1/decisions")
async def decisions(
    request: Request,
    range: str = Query(default="today", pattern="^(today|yesterday|7d|30d|12m|custom)$"),
    start: str | None = None,
    end: str | None = None,
    strategy: str | None = None,
    safety_only: bool = False,
    limit: int = Query(default=200, ge=1, le=1000),
    offset: int = Query(default=0, ge=0),
    _user: SessionInfo = Depends(require_viewer),
    db: AsyncSession = Depends(get_db),
) -> dict[str, Any]:
    cfg: ApiSettings = request.app.state.settings
    s, e, _ = resolve_window(range, tz_name=cfg.companion_timezone, start_iso=start, end_iso=end)
    return await query_decisions_and_intervals(
        db,
        cfg.device_id,
        s,
        e,
        strategy=strategy,
        safety_only=safety_only,
        limit=limit,
        offset=offset,
    )


@app.get("/api/v1/decisions/{decision_id}")
async def decision_detail(
    decision_id: str,
    request: Request,
    _user: SessionInfo = Depends(require_viewer),
    db: AsyncSession = Depends(get_db),
) -> dict[str, Any]:
    dev = request.app.state.settings.device_id
    q = select(EmsDecision).where(EmsDecision.device_id == dev)
    if ":" in decision_id:
        boot_id, seq_str = decision_id.rsplit(":", 1)
        try:
            seq = int(seq_str)
        except ValueError as exc:
            raise HTTPException(status_code=400, detail="ID decisione non valido") from exc
        q = q.where(EmsDecision.boot_id == boot_id, EmsDecision.seq == seq)
    else:
        try:
            seq = int(decision_id)
        except ValueError as exc:
            raise HTTPException(status_code=400, detail="ID decisione non valido") from exc
        q = q.where(EmsDecision.seq == seq)

    row = (await db.execute(q.order_by(desc(EmsDecision.source_timestamp)).limit(1))).scalar_one_or_none()
    if row is None:
        raise HTTPException(status_code=404, detail="Decisione non trovata")

    return {
        "id": f"{row.boot_id}:{row.seq}",
        "seq": row.seq,
        "boot_id": row.boot_id,
        "source_timestamp": row.source_timestamp.isoformat(),
        "received_at": row.received_at.isoformat() if row.received_at else None,
        "uptime_ms": row.uptime_ms,
        "time_source": row.time_source,
        "time_valid": row.time_valid,
        "strategy": row.strategy,
        "configured_strategy": row.configured_strategy,
        "previous_strategy": row.previous_strategy,
        "state": row.state,
        "transition": row.transition,
        "telemetry": {
            "soc": row.soc,
            "pv_w": row.pv_w,
            "load_w": row.load_w,
            "battery_w": row.battery_w,
            "grid_w": row.grid_w,
            "quality": row.quality_json,
        },
        "ha_inputs": row.ha_inputs_json,
        "candidate_w": row.candidate_w,
        "limited_w": row.limited_w,
        "simulated_w": row.simulated_w,
        "safety_min_w": row.safety_min_w,
        "safety_max_w": row.safety_max_w,
        "safety_reason": row.safety_reason,
        "reason": row.reason,
        "executed": False,
        "applied": None,
        "modbus_write_count": 0,
        "aux1": row.aux1,
        "raw_payload": row.raw_payload_json,
    }


@app.get("/api/v1/events")
async def events(
    request: Request,
    range: str = Query(default="today", pattern="^(today|yesterday|7d|30d|12m|custom)$"),
    start: str | None = None,
    end: str | None = None,
    event_type: str | None = None,
    limit: int = Query(default=200, ge=1, le=1000),
    _user: SessionInfo = Depends(require_viewer),
    db: AsyncSession = Depends(get_db),
) -> dict[str, Any]:
    cfg: ApiSettings = request.app.state.settings
    s, e, _ = resolve_window(range, tz_name=cfg.companion_timezone, start_iso=start, end_iso=end)
    q = select(EmsEvent).where(
        EmsEvent.device_id == cfg.device_id,
        EmsEvent.occurred_at >= s,
        EmsEvent.occurred_at <= e,
    )
    if event_type:
        q = q.where(EmsEvent.event_type == event_type)
    rows = (await db.execute(q.order_by(desc(EmsEvent.occurred_at)).limit(limit))).scalars().all()
    return {
        "events": [
            {
                "id": ev.id,
                "occurred_at": ev.occurred_at.isoformat(),
                "boot_id": ev.boot_id,
                "event_type": ev.event_type,
                "severity": ev.severity,
                "title": ev.title,
                "detail": ev.detail,
                "metadata": ev.metadata_json,
            }
            for ev in rows
        ]
    }


@app.get("/api/v1/kpi")
async def kpi(
    request: Request,
    range: str = Query(default="today", pattern="^(today|yesterday|7d|30d|12m|custom)$"),
    start: str | None = None,
    end: str | None = None,
    _user: SessionInfo = Depends(require_viewer),
    db: AsyncSession = Depends(get_db),
) -> dict[str, Any]:
    cfg: ApiSettings = request.app.state.settings
    s, e, _ = resolve_window(range, tz_name=cfg.companion_timezone, start_iso=start, end_iso=end)
    energy = await query_energy_kpi(db, cfg.device_id, s, e, battery_capacity_wh=cfg.battery_capacity_wh)
    report = await query_analytics_report(db, cfg.device_id, s, e, tz_name=cfg.companion_timezone, battery_capacity_wh=cfg.battery_capacity_wh)
    return {"period": {"start": s.isoformat(), "end": e.isoformat()}, "energy": energy, "analytics": report}


@app.get("/api/v1/comparison")
async def comparison(
    request: Request,
    range: str = Query(default="today", pattern="^(today|yesterday|7d|30d|12m|custom)$"),
    start: str | None = None,
    end: str | None = None,
    watt_threshold: float | None = Query(default=None, ge=10.0, le=2000.0),
    _user: SessionInfo = Depends(require_viewer),
    db: AsyncSession = Depends(get_db),
) -> dict[str, Any]:
    cfg: ApiSettings = request.app.state.settings
    runtime = await load_companion_settings(db)
    saved_thresh = runtime.get("sem_concordance_watt_threshold", {}).get("value", cfg.sem_concordance_watt_threshold)
    threshold = watt_threshold if watt_threshold is not None else float(saved_thresh)
    s, e, _ = resolve_window(range, tz_name=cfg.companion_timezone, start_iso=start, end_iso=end)
    return await query_comparison_summary(db, cfg.device_id, s, e, watt_threshold=threshold)


@app.get("/api/v1/battery")
async def battery(
    request: Request,
    range: str = Query(default="today", pattern="^(today|yesterday|7d|30d|12m|custom)$"),
    start: str | None = None,
    end: str | None = None,
    _user: SessionInfo = Depends(require_viewer),
    db: AsyncSession = Depends(get_db),
) -> dict[str, Any]:
    cfg: ApiSettings = request.app.state.settings
    s, e, _ = resolve_window(range, tz_name=cfg.companion_timezone, start_iso=start, end_iso=end)
    return await query_battery_summary(db, cfg.device_id, s, e, battery_capacity_wh=cfg.battery_capacity_wh)


@app.get("/api/v1/reports")
async def reports(
    request: Request,
    range: str = Query(default="today", pattern="^(today|yesterday|7d|30d|12m|custom)$"),
    start: str | None = None,
    end: str | None = None,
    _user: SessionInfo = Depends(require_viewer),
    db: AsyncSession = Depends(get_db),
) -> dict[str, Any]:
    cfg: ApiSettings = request.app.state.settings
    s, e, _ = resolve_window(range, tz_name=cfg.companion_timezone, start_iso=start, end_iso=end)
    report = await query_analytics_report(
        db,
        cfg.device_id,
        s,
        e,
        tz_name=cfg.companion_timezone,
        battery_capacity_wh=cfg.battery_capacity_wh,
        watt_threshold=cfg.sem_concordance_watt_threshold,
    )
    sys_status = await get_system_status(db, cfg.device_id)
    report["system"] = sys_status
    return report


@app.get("/api/v1/export")
async def export_data(
    request: Request,
    format: str = Query(default="json", pattern="^(json|csv|pdf)$"),
    scope: str = Query(default="history", pattern="^(history|decisions|comparison|report)$"),
    range: str = Query(default="today", pattern="^(today|yesterday|7d|30d|12m|custom)$"),
    start: str | None = None,
    end: str | None = None,
    _user: SessionInfo = Depends(require_viewer),
    db: AsyncSession = Depends(get_db),
) -> Response:
    cfg: ApiSettings = request.app.state.settings
    s, e, bucket = resolve_window(range, tz_name=cfg.companion_timezone, start_iso=start, end_iso=end)

    if scope == "history":
        data = await query_history(db, cfg.device_id, s, e, bucket)
        rows = data["series"]
    elif scope == "decisions":
        data = await query_decisions_and_intervals(db, cfg.device_id, s, e, limit=1000)
        rows = data["decisions"]
    elif scope == "comparison":
        data = await query_comparison_summary(db, cfg.device_id, s, e, watt_threshold=cfg.sem_concordance_watt_threshold)
        rows = data["series"]
    else:
        data = await query_analytics_report(db, cfg.device_id, s, e, tz_name=cfg.companion_timezone, battery_capacity_wh=cfg.battery_capacity_wh)
        rows = [data]

    filename = f"afore-companion-{scope}-{range}.{format}"
    if format == "json":
        return JSONResponse(
            content={"operating_mode": "SHADOW", "executed": False, "scope": scope, "data": data},
            headers={"Content-Disposition": f'attachment; filename="{filename}"'},
        )

    if format == "csv":
        buf = io.StringIO()
        if rows and isinstance(rows[0], dict):
            flat_rows = rows
            if scope == "report":
                flat_rows = [
                    {
                        "start": data["period"]["start"],
                        "end": data["period"]["end"],
                        "operating_mode": "SHADOW",
                        **{k: v for k, v in data["energy"].items() if not isinstance(v, dict)},
                        "concordance_pct": data["comparison"]["concordance_pct"],
                        "availability_pct": data["data_quality"]["availability_pct"],
                    }
                ]
            headers_csv = list(flat_rows[0].keys())
            writer = csv.DictWriter(buf, fieldnames=headers_csv)
            writer.writeheader()
            for r in flat_rows:
                writer.writerow({k: ("" if v is None else v) for k, v in r.items()})
        return Response(
            content=buf.getvalue(),
            media_type="text/csv; charset=utf-8",
            headers={"Content-Disposition": f'attachment; filename="{filename}"'},
        )

    # PDF report export
    rep = data if scope == "report" else await query_analytics_report(db, cfg.device_id, s, e)
    en = rep["energy"]
    cmp_kpi = rep["comparison"]
    dq = rep["data_quality"]
    pdf_lines = [
        f"Periodo: {rep['period']['start']} -> {rep['period']['end']}",
        "Modalita: SHADOW (Decisioni simulate - Nessun comando applicato)",
        "",
        "--- RIEPILOGO ENERGETICO ---",
        f"Produzione FV: {en.get('pv_kwh')} kWh (misurato)",
        f"Consumo casa: {en.get('load_kwh')} kWh (derivato)",
        f"Importazione rete: {en.get('grid_import_kwh')} kWh | Esportazione: {en.get('grid_export_kwh')} kWh",
        f"Batteria carica / scarica: {en.get('battery_charge_kwh')} / {en.get('battery_discharge_kwh')} kWh",
        f"Autoconsumo: {en.get('self_consumption_pct')}% | Autosufficienza: {en.get('self_sufficiency_pct')}%",
        "",
        "--- CONFRONTO SEM (REALE) / ESP (SIMULATO) ---",
        f"Concordanza: {cmp_kpi.get('concordance_pct')}% ({cmp_kpi.get('concordant_points')}/{cmp_kpi.get('comparable_points')})",
        f"Differenza media: {cmp_kpi.get('mean_watt_diff')} W | P95: {cmp_kpi.get('p95_watt_diff')} W",
        f"Interventi Safety: {cmp_kpi.get('safety_interventions')}",
        "",
        "--- QUALITA DATI ---",
        f"Campioni telemetria: {dq.get('telemetry_samples')} | Disponibilita: {dq.get('availability_pct')}%",
        f"Gap secondi: {dq.get('gap_seconds')} s | Riavvii ESP: {dq.get('esp_reboots')}",
    ]
    pdf_bytes = build_simple_pdf("Afore EMS Companion - Report Energetico SHADOW", pdf_lines)
    return Response(
        content=pdf_bytes,
        media_type="application/pdf",
        headers={"Content-Disposition": f'attachment; filename="{filename}"'},
    )


@app.get("/api/v1/settings")
async def get_settings(
    request: Request,
    _user: SessionInfo = Depends(require_viewer),
    db: AsyncSession = Depends(get_db),
) -> dict[str, Any]:
    cfg: ApiSettings = request.app.state.settings
    runtime = await load_companion_settings(db)
    sys_status = await get_system_status(db, cfg.device_id)
    comps = sys_status["components"]

    return {
        "operating_mode": "SHADOW",
        "live_control_available": False,
        "companion": {
            "timezone": runtime.get("timezone", {}).get("value", cfg.companion_timezone),
            "locale": runtime.get("locale", {}).get("value", cfg.companion_locale),
            "theme": runtime.get("theme", {}).get("value", "dark"),
            "units_power": runtime.get("units_power", {}).get("value", "kW"),
            "sem_concordance_watt_threshold": runtime.get("sem_concordance_watt_threshold", {}).get(
                "value", cfg.sem_concordance_watt_threshold
            ),
        },
        "retention": {
            "raw_mqtt_days": runtime.get("retention_raw_mqtt_days", {}).get("value", cfg.retention_raw_mqtt_days),
            "telemetry_days": runtime.get("retention_telemetry_days", {}).get("value", cfg.retention_telemetry_days),
            "agg_1m_days": runtime.get("retention_agg_1m_days", {}).get("value", cfg.retention_agg_1m_days),
            "backup_retention_days": cfg.backup_retention_days,
        },
        "integrations": {
            "mqtt": {
                "enabled": cfg.mqtt_enabled,
                "host": cfg.mqtt_host,
                "port": cfg.mqtt_port,
                "base_topic": cfg.mqtt_base_topic,
                "username_configured": bool(cfg.mqtt_user),
                "password_configured": bool(cfg.mqtt_password),
                "status": comps["mqtt"]["status"],
                "last_connected_at": comps["mqtt"]["recorded_at"],
                "last_error": comps["mqtt"]["detail"].get("error"),
            },
            "esp32": {
                "enabled": cfg.esp32_enabled,
                "url": cfg.esp32_url,
                "username": cfg.esp32_username,
                "password_configured": bool(cfg.esp32_password),
                "status": comps["esp32"]["status"],
                "last_connected_at": comps["esp32"]["recorded_at"],
                "last_error": comps["esp32"]["detail"].get("error"),
            },
            "home_assistant": {
                "mqtt_inputs_active": comps["home_assistant"]["status"] == "online",
                "rest_fallback_enabled": cfg.ha_rest_enabled,
                "url": cfg.ha_url if cfg.ha_rest_enabled else None,
                "token_configured": bool(cfg.ha_token),
                "status": comps["home_assistant"]["status"],
                "last_seen_at": comps["home_assistant"]["recorded_at"],
            },
            "database": comps["database"],
        },
    }


@app.patch("/api/v1/settings")
async def patch_settings(
    body: SettingsPatchRequest,
    _admin: SessionInfo = Depends(require_admin_csrf),
    db: AsyncSession = Depends(get_db),
) -> dict[str, Any]:
    updates = body.model_dump(exclude_none=True)
    for k, v in updates.items():
        stmt = (
            insert(CompanionSetting)
            .values(key=k, value_json={"value": v})
            .on_conflict_do_update(
                index_elements=["key"],
                set_={"value_json": {"value": v}, "updated_at": datetime.now(timezone.utc)},
            )
        )
        await db.execute(stmt)
    await db.commit()
    return {"updated": list(updates.keys())}


@app.post("/api/v1/settings/test-connection")
async def test_connection(
    target: str = Query(pattern="^(esp32|database)$"),
    request: Request = None,
    _admin: SessionInfo = Depends(require_admin_csrf),
    db: AsyncSession = Depends(get_db),
) -> dict[str, Any]:
    cfg: ApiSettings = request.app.state.settings
    if target == "database":
        await db.execute(text("SELECT 1"))
        return {"target": "database", "ok": True, "detail": "TimescaleDB reachable"}

    if not cfg.esp32_password:
        return {"target": "esp32", "ok": False, "detail": "ESP32 password not configured"}

    try:
        async with httpx.AsyncClient(
            base_url=cfg.esp32_url,
            auth=httpx.BasicAuth(cfg.esp32_username, cfg.esp32_password),
            timeout=4.0,
        ) as client:
            r = await client.get("/api/v1/health")
            r.raise_for_status()
            info = r.json()
        return {
            "target": "esp32",
            "ok": True,
            "firmware": info.get("firmware"),
            "mode": info.get("mode"),
        }
    except Exception as exc:
        return {"target": "esp32", "ok": False, "detail": str(exc)[:160]}
