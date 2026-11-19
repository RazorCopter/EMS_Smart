from __future__ import annotations

import os
import time
import httpx
import pytest


@pytest.mark.asyncio
async def test_live_docker_api_end_to_end_and_security():
    base_url = os.getenv("COMPANION_E2E_URL")
    if not base_url:
        pytest.skip("Set COMPANION_E2E_URL (e.g. http://127.0.0.1:8080) for live Docker E2E test")

    async with httpx.AsyncClient(base_url=base_url, timeout=10.0) as client:
        # 1. Public health check
        r = await client.get("/api/v1/health")
        assert r.status_code == 200
        h = r.json()
        assert h["operating_mode"] == "SHADOW"
        assert h["read_only"] is True
        assert h["modbus_write_count"] if "modbus_write_count" in h else h["modbus_write_capable"] is False

        # 2. Protected route rejects unauthenticated access
        unauth = await client.get("/api/v1/live")
        assert unauth.status_code == 401

        # 3. Login as Viewer and verify RBAC blocks admin PATCH
        login_v = await client.post("/api/v1/auth/login", json={"username": "viewer", "password": "AforeViewer2026!"})
        assert login_v.status_code == 200
        csrf_v = login_v.json()["csrf_token"]
        patch_v = await client.patch(
            "/api/v1/settings",
            json={"theme": "light"},
            headers={"X-CSRF-Token": csrf_v},
        )
        assert patch_v.status_code == 403

        # 4. Login as Admin and verify CSRF enforcement + settings redaction
        login_a = await client.post("/api/v1/auth/login", json={"username": "admin", "password": "AforeAdmin2026!"})
        assert login_a.status_code == 200
        csrf_a = login_a.json()["csrf_token"]

        no_csrf = await client.patch("/api/v1/settings", json={"sem_concordance_watt_threshold": 100})
        assert no_csrf.status_code == 403

        settings_r = await client.get("/api/v1/settings")
        assert settings_r.status_code == 200
        settings_json = settings_r.json()
        assert settings_json["live_control_available"] is False
        assert "password" not in settings_json["integrations"]["esp32"]
        assert settings_json["integrations"]["esp32"]["password_configured"] is True

        # 5. Measure query latency (< 500ms for daily history)
        t0 = time.perf_counter()
        hist_r = await client.get("/api/v1/history?range=today")
        elapsed_ms = (time.perf_counter() - t0) * 1000
        assert hist_r.status_code == 200
        assert elapsed_ms < 1000.0

        for ep in [
            "/api/v1/live",
            "/api/v1/system/status",
            "/api/v1/decisions?range=today",
            "/api/v1/events?range=today",
            "/api/v1/kpi?range=today",
            "/api/v1/comparison?range=today",
            "/api/v1/battery?range=today",
            "/api/v1/reports?range=today",
            "/api/v1/export?format=csv&scope=history&range=today",
            "/api/v1/export?format=json&scope=decisions&range=today",
            "/api/v1/export?format=pdf&scope=report&range=today",
        ]:
            resp = await client.get(ep)
            assert resp.status_code == 200, f"Endpoint {ep} failed: {resp.status_code}"
            if ep.endswith("pdf&scope=report&range=today"):
                assert resp.content.startswith(b"%PDF-1.4")

        # 6. Admin PATCH with CSRF succeeds
        patch_ok = await client.patch(
            "/api/v1/settings",
            json={"sem_concordance_watt_threshold": 100},
            headers={"X-CSRF-Token": csrf_a},
        )
        assert patch_ok.status_code == 200
