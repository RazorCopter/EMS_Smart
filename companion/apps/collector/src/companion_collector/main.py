from __future__ import annotations

import asyncio
import contextlib
import logging
import ssl

from aiohttp import web
import aiomqtt
from prometheus_client import CONTENT_TYPE_LATEST, generate_latest
from sqlalchemy.ext.asyncio import async_sessionmaker, create_async_engine

from energy_domain import SecretRedactingFilter, SemComparisonThresholds
from .config import CollectorSettings
from .ingestor import IngestionEngine
from .esp_logs import esp_log_loop
from .rest_client import Esp32ReadOnlyClient

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s %(levelname)s [%(name)s] %(message)s",
)
for handler in logging.getLogger().handlers:
    handler.addFilter(SecretRedactingFilter())

logger = logging.getLogger("companion.collector")


async def run_health_server(settings: CollectorSettings, engine: IngestionEngine) -> web.AppRunner:
    async def health(_request: web.Request) -> web.Response:
        return web.json_response(
            {
                "status": "ok",
                "mode": "SHADOW",
                "device_id": settings.device_id,
                "last_seq": engine.state.last_seq,
                "boot_id": engine.state.boot_id,
            }
        )

    async def metrics(_request: web.Request) -> web.Response:
        return web.Response(body=generate_latest(), content_type=CONTENT_TYPE_LATEST)

    app = web.Application()
    app.router.add_get("/healthz", health)
    app.router.add_get("/metrics", metrics)
    runner = web.AppRunner(app)
    await runner.setup()
    site = web.TCPSite(runner, "0.0.0.0", settings.metrics_port)
    await site.start()
    return runner


async def mqtt_loop(settings: CollectorSettings, engine: IngestionEngine) -> None:
    if not settings.mqtt_enabled:
        logger.info("MQTT ingestion disabled via configuration")
        return

    base = settings.mqtt_base_topic.rstrip("/")
    sem = settings.mqtt_sem_topic_prefix.rstrip("/")
    topics = [
        (f"{base}/availability", 1),
        (f"{base}/telemetry", 0),
        (f"{base}/ems/state", 1),
        (f"{base}/ems/decision", 1),
        (f"{base}/ems/ack", 1),
        (f"{base}/config/state", 1),
        (f"{base}/diagnostics", 0),
        (f"{base}/system/manual", 1),
        (f"{base}/system/ha_inputs", 0),
        (f"{base}/input/#", 1),
        (f"{base}/forecast/solar", 1),
        (f"{sem}/#", 1),
        ("zigbee2mqtt/POW-Generale", 0),
        ("zigbee2mqtt/POW-Prese", 0),
    ]

    backoff = 2.0
    while True:
        try:
            tls_ctx = ssl.create_default_context() if settings.mqtt_tls else None
            async with aiomqtt.Client(
                hostname=settings.mqtt_host,
                port=settings.mqtt_port,
                username=settings.mqtt_user or None,
                password=settings.mqtt_password or None,
                identifier=settings.mqtt_client_id,
                tls_context=tls_ctx,
                keepalive=30,
            ) as client:
                logger.info("Connected to MQTT broker %s:%s", settings.mqtt_host, settings.mqtt_port)
                await engine.update_component_health(
                    "mqtt", "online", {"host": settings.mqtt_host, "port": settings.mqtt_port}
                )
                backoff = 2.0
                for topic, qos in topics:
                    await client.subscribe(topic, qos=qos)

                async for message in client.messages:
                    await engine.ingest_mqtt_message(
                        str(message.topic),
                        bytes(message.payload),
                        qos=message.qos,
                        retain=message.retain,
                    )
        except asyncio.CancelledError:
            raise
        except Exception as exc:
            logger.warning("MQTT connection interrupted (%s); reconnecting in %.1fs", exc, backoff)
            with contextlib.suppress(Exception):
                await engine.update_component_health(
                    "mqtt",
                    "offline",
                    {"host": settings.mqtt_host, "port": settings.mqtt_port, "error": str(exc)[:160]},
                )
            await asyncio.sleep(backoff)
            backoff = min(30.0, backoff * 1.8)


async def esp32_rest_loop(settings: CollectorSettings, engine: IngestionEngine) -> None:
    if not settings.esp32_enabled or not settings.esp32_password:
        logger.info("ESP32 REST bootstrap/backfill skipped (disabled or password not configured)")
        return

    client = Esp32ReadOnlyClient(settings.esp32_url, settings.esp32_username, settings.esp32_password)
    did_bootstrap = False
    last_backfill = 0.0
    health_failures = 0
    interval = max(5, settings.esp32_poll_health_s)

    while True:
        problems: dict[str, str] = {}
        try:
            health = await client.fetch_health()
        except Exception as exc:
            health_failures += 1
            problems["health"] = str(exc)[:160] or type(exc).__name__
            mqtt_seen = engine.state.last_mqtt_telemetry_at
            mqtt_fresh = mqtt_seen is not None and asyncio.get_running_loop().time() - mqtt_seen <= max(15, interval * 2)
            if health_failures >= 3 or mqtt_fresh:
                with contextlib.suppress(Exception):
                    await engine.update_component_health(
                        "esp32", "online" if mqtt_fresh else "offline",
                        {"url": settings.esp32_url, "source": "mqtt" if mqtt_fresh else "health",
                         "health_failures": health_failures, "error": problems["health"]},
                    )
        else:
            health_failures = 0
            try:
                await engine.ingest_rest_health_bundle(health=health)
            except Exception as exc:
                problems["health_storage"] = str(exc)[:160] or type(exc).__name__

            for name, fetch in (
                ("config", client.fetch_config), ("manual", client.fetch_manual),
                ("ha_inputs", client.fetch_ha_inputs), ("time_info", client.fetch_time),
                ("telemetry", client.fetch_telemetry),
            ):
                try:
                    payload = await fetch()
                    if payload is not None:
                        if name == "telemetry":
                            await engine.ingest_rest_telemetry(payload)
                        else:
                            await engine.ingest_rest_health_bundle(**{name: payload})
                except Exception as exc:
                    problems[name] = str(exc)[:160] or type(exc).__name__

            now_loop = asyncio.get_running_loop().time()
            if not did_bootstrap or engine.state.needs_backfill or (
                now_loop - last_backfill >= settings.esp32_backfill_interval_s
            ):
                try:
                    decisions = await client.fetch_decisions_ndjson()
                    if decisions:
                        inserted = await engine.ingest_rest_decisions(decisions)
                        logger.info("REST decision backfill synced %d/%d records", inserted, len(decisions))
                    engine.state.needs_backfill = False
                    did_bootstrap = True
                    last_backfill = now_loop
                except Exception as exc:
                    problems["decision_backfill"] = str(exc)[:160] or type(exc).__name__

        if problems:
            logger.warning("ESP32 REST synchronization degraded: %s", problems)
        with contextlib.suppress(Exception):
            await engine.update_component_health(
                "esp32_sync", "degraded" if problems else "online", {"errors": problems}
            )
        await asyncio.sleep(interval)


async def main() -> None:
    settings = CollectorSettings()
    db_engine = create_async_engine(settings.database_url, pool_pre_ping=True, pool_size=5, max_overflow=5)
    session_factory = async_sessionmaker(db_engine, expire_on_commit=False)

    engine = IngestionEngine(
        session_factory,
        device_id=settings.device_id,
        base_topic=settings.mqtt_base_topic,
        sem_prefix=settings.mqtt_sem_topic_prefix,
        max_energy_gap_s=settings.max_energy_gap_s,
        sem_thresholds=SemComparisonThresholds(
            watt_threshold=settings.sem_concordance_watt_threshold,
            deadband_w=settings.power_deadband_w,
            nominal_voltage_v=settings.battery_nominal_v,
        ),
    )

    # Wait briefly for DB readiness if starting alongside TimescaleDB
    for attempt in range(1, 15):
        try:
            await engine.bootstrap_from_db()
            break
        except Exception as exc:
            logger.info("Waiting for database (%d/15): %s", attempt, exc)
            await asyncio.sleep(2.0)

    tasks = [mqtt_loop(settings, engine), esp32_rest_loop(settings, engine)]
    if settings.esp32_enabled and settings.esp32_password:
        log_client = Esp32ReadOnlyClient(settings.esp32_url, settings.esp32_username, settings.esp32_password)
        tasks.append(esp_log_loop(log_client))

    runner = await run_health_server(settings, engine)
    try:
        await asyncio.gather(*tasks)
    finally:
        await runner.cleanup()
        await db_engine.dispose()


if __name__ == "__main__":
    asyncio.run(main())
