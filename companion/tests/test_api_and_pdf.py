from datetime import datetime, timedelta, timezone
from types import SimpleNamespace

from fastapi import HTTPException
import pytest

from companion_api.auth import LoginRateLimiter, sign_session, verify_session
from companion_api.pdf_export import build_simple_pdf
from companion_api.queries import group_dashboard_events, is_legacy_stride_gap, resolve_window


def test_session_signing_and_tamper_rejection():
    secret = "super-secret-test-key"
    token, csrf = sign_session("viewer", "viewer", secret, ttl_hours=1)
    assert len(csrf) > 10

    info = verify_session(token, secret)
    assert info is not None
    assert info.username == "viewer"
    assert info.role == "viewer"
    assert info.csrf_token == csrf

    assert verify_session(token + "x", secret) is None
    assert verify_session(token, "wrong-secret") is None


def test_login_rate_limiter_blocks_excessive_attempts():
    limiter = LoginRateLimiter(max_attempts=3, window_seconds=60)
    limiter.check("127.0.0.1")
    limiter.check("127.0.0.1")
    limiter.check("127.0.0.1")
    with pytest.raises(HTTPException) as exc:
        limiter.check("127.0.0.1")
    assert exc.value.status_code == 429


def test_pdf_generator_outputs_valid_pdf_header_and_eof():
    pdf = build_simple_pdf("Afore EMS Companion Test", ["Line 1", "SHADOW mode"])
    assert pdf.startswith(b"%PDF-1.4")
    assert b"%%EOF" in pdf


def test_resolve_window_quick_intervals():
    now = datetime(2026, 11, 19, 12, 0, tzinfo=timezone.utc)
    for label, hours, exp_bucket in [("1h", 1, "1m"), ("3h", 3, "1m"), ("6h", 6, "5m"), ("12h", 12, "5m")]:
        s, e, b = resolve_window(label, now_utc=now)
        assert e == now
        assert e - s == timedelta(hours=hours)
        assert b == exp_bucket


def test_group_dashboard_events_filters_legacy_gaps_and_groups_repetitive():
    now = datetime(2026, 11, 19, 11, 0, tzinfo=timezone.utc)
    events = [
        SimpleNamespace(id=1, occurred_at=now, event_type="DECISION_GAP", severity="warning", title="Gap sequenza decisioni (4 record)", detail="10424 -> 10429"),
        SimpleNamespace(id=2, occurred_at=now, event_type="DECISION_GAP", severity="warning", title="Gap sequenza decisioni (300 record)", detail="10000 -> 10301"),
        SimpleNamespace(id=3, occurred_at=now, event_type="SOC_LIMIT", severity="warning", title="Intervento Safety: SOC_RESERVE", detail="SOC 20%"),
        SimpleNamespace(id=4, occurred_at=now - timedelta(seconds=10), event_type="SOC_LIMIT", severity="warning", title="Intervento Safety: SOC_RESERVE", detail="SOC 20%"),
        SimpleNamespace(id=5, occurred_at=now - timedelta(seconds=20), event_type="MQTT_ONLINE", severity="info", title="ESP32 availability: online", detail="online"),
        SimpleNamespace(id=6, occurred_at=now - timedelta(seconds=30), event_type="STRATEGY_CHANGE", severity="info", title="Cambio strategia", detail="Self Use"),
    ]
    assert is_legacy_stride_gap("DECISION_GAP", "Gap sequenza decisioni (4 record)", None) is True
    assert is_legacy_stride_gap("DECISION_GAP", "Gap sequenza decisioni (300 record)", None) is False

    grouped, legacy_cnt = group_dashboard_events(events, limit=3)
    assert legacy_cnt == 1
    assert len(grouped) == 3
    soc_grp = next(g for g in grouped if g["event_type"] == "SOC_LIMIT")
    assert soc_grp["count"] == 2
    assert "2 volte" in soc_grp["title"]
