from datetime import datetime, timedelta, timezone
import logging

import pytest

from energy_domain import (
    DecisionAuditPayload,
    EnergyAccumulator,
    SecretRedactingFilter,
    SemComparisonThresholds,
    TelemetryPayload,
    compare_sem_and_esp,
    compute_kpis,
    compute_setpoint_churn,
    hash_password,
    redact_dict,
    summarize_comparisons,
    verify_password,
)


def test_telemetry_preserves_null_never_substitutes_zero():
    payload = {
        "soc": None,
        "battery_w": None,
        "pv_w": 0.0,
        "grid_w": -250.0,
        "load_w": None,
        "telemetry_valid": False,
        "modbus_write_count": 0,
        "timestamp": "2026-11-19T12:00:00Z",
        "uptime_ms": 12000,
        "time_valid": True,
    }
    tel = TelemetryPayload.model_validate(payload)
    assert tel.soc is None
    assert tel.battery_w is None
    assert tel.load_w is None
    assert tel.pv_w == 0.0
    assert tel.grid_w == -250.0


def test_decision_enforces_shadow_invariants():
    base = {
        "schema": 1,
        "source": "MEASURED",
        "seq": 42,
        "timestamp": "2026-11-19T12:00:00Z",
        "uptime_ms": 42000,
        "time_source": "NTP",
        "time_valid": True,
        "strategy": "SELF_USE",
        "configured": "SELF_USE",
        "previous": "SELF_USE",
        "state": "RUN",
        "transition": "UNCHANGED",
        "soc": 45.0,
        "grid_w": -300.0,
        "battery_w": 280.0,
        "load_w": 900.0,
        "pv_w": 1480.0,
        "candidate_w": 580,
        "limited_w": 580,
        "simulated_w": 580,
        "safety_min_w": -1000,
        "safety_max_w": 1000,
        "executed": False,
        "applied": None,
        "modbus_write_count": 0,
    }
    dec = DecisionAuditPayload.model_validate(base)
    assert dec.executed is False
    assert dec.modbus_write_count == 0
    assert dec.applied is None

    with pytest.raises(ValueError):
        DecisionAuditPayload.model_validate({**base, "executed": True})

    with pytest.raises(ValueError):
        DecisionAuditPayload.model_validate({**base, "modbus_write_count": 1})


def test_energy_accumulator_skips_gaps_and_computes_kpis():
    acc = EnergyAccumulator(max_gap_s=60.0)
    # 1 hour of 2000W PV, 1000W Load, 500W battery charge, -500W grid export -> 3600s in 60s chunks
    for _ in range(60):
        acc.add_interval(60.0, pv_w=2000.0, load_w=1000.0, grid_w=-500.0, battery_w=500.0)
    # 15-minute telemetry gap -> must be counted as gap_seconds, NOT interpolated
    acc.add_interval(900.0, pv_w=2000.0, load_w=1000.0, grid_w=-500.0, battery_w=500.0)

    totals = acc.totals()
    assert totals.valid_seconds == 3600.0
    assert totals.gap_seconds == 900.0
    assert totals.pv_wh == 2000.0
    assert totals.load_wh == 1000.0
    assert totals.grid_export_wh == 500.0
    assert totals.grid_import_wh == 0.0
    assert totals.battery_charge_wh == 500.0
    assert totals.self_consumed_pv_wh == 1500.0  # 2000 - 500 exported

    kpi = compute_kpis(totals, battery_capacity_wh=10240.0, expected_window_seconds=4500.0)
    assert kpi.pv_kwh == 2.0
    assert kpi.self_consumption_pct == 75.0
    assert kpi.self_sufficiency_pct == 100.0
    assert kpi.data_availability_pct == 80.0


def test_kpis_return_none_when_no_samples():
    acc = EnergyAccumulator()
    totals = acc.totals()
    kpi = compute_kpis(totals)
    assert kpi.pv_kwh is None
    assert kpi.load_kwh is None
    assert kpi.self_consumption_pct is None
    assert kpi.self_sufficiency_pct is None


def test_sem_comparison_and_churn():
    t0 = datetime(2026, 11, 19, 10, 0, tzinfo=timezone.utc)
    p1 = compare_sem_and_esp(
        compared_at=t0,
        sem_decision="SelfUse (oggi basta)",
        sem_amps=20,
        solarman_mode="SelfUse",
        esp_strategy="SELF_USE",
        esp_simulated_w=420.0,
        actual_battery_w=400.0,
        actual_grid_w=-20.0,
        actual_soc=55.0,
        thresholds=SemComparisonThresholds(watt_threshold=100.0),
    )
    assert p1.comparable is True
    assert p1.concordant is True
    assert p1.watt_diff == 20.0

    p2 = compare_sem_and_esp(
        compared_at=t0 + timedelta(minutes=1),
        sem_decision="ChgFst attivo",
        sem_amps=50,
        solarman_mode="ChgFst",
        esp_strategy="SELF_USE",
        esp_simulated_w=-300.0,
        actual_battery_w=1000.0,
        actual_grid_w=200.0,
        actual_soc=30.0,
    )
    assert p2.comparable is True
    assert p2.concordant is False
    assert "STRATEGY_DIVERGENCE" in p2.reason

    agg = summarize_comparisons([p1, p2], safety_interventions=1, setpoint_oscillations_per_hour=2.0)
    assert agg.concordance_pct == 50.0
    assert agg.divergent_points == 1
    assert agg.missed_opportunities == 1

    churn = compute_setpoint_churn(
        [(t0, 0.0), (t0 + timedelta(minutes=30), 300.0), (t0 + timedelta(minutes=60), -200.0)],
        delta_threshold_w=100.0,
    )
    assert churn == 2.0


def test_password_hashing_and_redaction():
    hashed = hash_password("SuperSecretPassword123")
    assert verify_password("SuperSecretPassword123", hashed) is True
    assert verify_password("WrongPassword", hashed) is False

    redacted = redact_dict({"host": "10.0.10.10", "password": "secret", "nested": {"api_key": "tok"}})
    assert redacted["host"] == "10.0.10.10"
    assert redacted["password"] == "***REDACTED***"
    assert redacted["nested"]["api_key"] == "***REDACTED***"

    rec = logging.LogRecord("test", logging.INFO, "", 0, 'password="mysecret"', None, None)
    SecretRedactingFilter().filter(rec)
    assert "mysecret" not in rec.getMessage()
