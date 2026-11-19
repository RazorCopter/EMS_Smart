from __future__ import annotations

import json
import logging
from typing import Any

import httpx

logger = logging.getLogger(__name__)


class Esp32ReadOnlyClient:
    """Strictly read-only HTTP client for ESP32 bootstrap, gap backfill, and diagnostics.

    By design, this client implements ONLY GET requests. No write or command
    endpoints exist anywhere in the Companion.
    """

    def __init__(self, base_url: str, username: str, password: str, timeout_s: float = 5.0) -> None:
        self.base_url = base_url.rstrip("/")
        self.auth = httpx.BasicAuth(username, password) if password else None
        self.timeout_s = timeout_s

    async def _get_json(self, path: str) -> dict[str, Any] | None:
        async with httpx.AsyncClient(base_url=self.base_url, auth=self.auth, timeout=self.timeout_s) as client:
            resp = await client.get(path)
            resp.raise_for_status()
            return resp.json()

    async def fetch_health(self) -> dict[str, Any] | None:
        return await self._get_json("/api/v1/health")

    async def fetch_telemetry(self) -> dict[str, Any] | None:
        return await self._get_json("/api/v1/telemetry")

    async def fetch_config(self) -> dict[str, Any] | None:
        return await self._get_json("/api/v1/ems/config")

    async def fetch_manual(self) -> dict[str, Any] | None:
        return await self._get_json("/api/v1/manual")

    async def fetch_ha_inputs(self) -> dict[str, Any] | None:
        return await self._get_json("/api/v1/ha/inputs")

    async def fetch_time(self) -> dict[str, Any] | None:
        return await self._get_json("/api/v1/time")

    async def fetch_decisions_ndjson(self) -> list[dict[str, Any]]:
        async with httpx.AsyncClient(base_url=self.base_url, auth=self.auth, timeout=self.timeout_s) as client:
            resp = await client.get("/api/v1/ems/decisions")
            resp.raise_for_status()
            items: list[dict[str, Any]] = []
            for line in resp.text.splitlines():
                line = line.strip()
                if not line:
                    continue
                try:
                    items.append(json.loads(line))
                except json.JSONDecodeError:
                    logger.warning("Skipping malformed NDJSON decision line from ESP32")
            return items
