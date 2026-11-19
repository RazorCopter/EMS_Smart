from __future__ import annotations

from dataclasses import dataclass
from datetime import datetime
import math
from typing import Iterable


@dataclass(frozen=True)
class SemComparisonThresholds:
    watt_threshold: float = 100.0
    deadband_w: float = 20.0
    nominal_voltage_v: float = 51.2


@dataclass(frozen=True)
class ComparisonPoint:
    compared_at: datetime
    sem_decision: str | None
    sem_mapped_strategy: str | None
    sem_amps: float | None
    sem_target_w: float | None
    esp_strategy: str | None
    esp_simulated_w: float | None
    actual_battery_w: float | None
    actual_grid_w: float | None
    actual_soc: float | None
    comparable: bool
    concordant: bool | None
    direction_match: bool | None
    strategy_match: bool | None
    watt_diff: float | None
    reason: str


@dataclass(frozen=True)
class ComparisonAggregate:
    total_points: int
    comparable_points: int
    concordant_points: int
    divergent_points: int
    concordance_pct: float | None
    mean_watt_diff: float | None
    p95_watt_diff: float | None
    false_positives: int
    missed_opportunities: int
    safety_interventions: int
    setpoint_oscillations_per_hour: float | None
    strategy_share: dict[str, int]


def map_sem_decision_to_strategy(sem_decision: str | None, solarman_mode: str | None = None) -> str | None:
    """Maps Home Assistant SEM / Solarman strings onto ESP32 canonical strategy names."""
    text = (sem_decision or "").strip()
    mode = (solarman_mode or "").strip()

    if text.startswith("SelfUse") or mode == "SelfUse":
        return "SELF_USE"
    if text.startswith("ChgFst") or mode == "ChgFst":
        return "CHARGE_FIRST"
    if text in {"SOC non disponibile", "Previsione non valida", "Solo modulazione"}:
        return None
    return None


def _direction(watts: float | None, deadband_w: float) -> str | None:
    if watts is None:
        return None
    if watts > deadband_w:
        return "charge"
    if watts < -deadband_w:
        return "discharge"
    return "neutral"


def compare_sem_and_esp(
    *,
    compared_at: datetime,
    sem_decision: str | None,
    sem_amps: float | None,
    solarman_mode: str | None,
    esp_strategy: str | None,
    esp_simulated_w: float | None,
    actual_battery_w: float | None,
    actual_grid_w: float | None,
    actual_soc: float | None,
    battery_voltage_v: float | None = None,
    thresholds: SemComparisonThresholds | None = None,
) -> ComparisonPoint:
    """Compare real HA/SEM state against simulated ESP SHADOW decision.

    Note:
    - SEM is the actual real controller (`solarman_mode`, `sem_decision`, `sem_amps`).
    - ESP32 SHADOW produces simulated setpoints (`esp_simulated_w`) for validation only.
    - When `sem_amps` represents a charge current limit (e.g., 50 A in ChgFst or modulation),
      we convert to a reference watt ceiling (`sem_amps * voltage`) for magnitude awareness,
      while strategy and direction concordance use observed/expected behavior.
    """
    cfg = thresholds or SemComparisonThresholds()
    sem_strategy = map_sem_decision_to_strategy(sem_decision, solarman_mode)
    voltage = battery_voltage_v if (battery_voltage_v and battery_voltage_v > 10.0) else cfg.nominal_voltage_v

    sem_target_w: float | None = None
    if sem_amps is not None and sem_strategy == "CHARGE_FIRST":
        sem_target_w = round(sem_amps * voltage, 1)
    elif sem_strategy == "SELF_USE" and actual_battery_w is not None:
        sem_target_w = round(actual_battery_w, 1)

    comparable = (
        sem_strategy is not None
        and esp_strategy in {"SELF_USE", "CHARGE_FIRST", "TARGET_SOC", "MAINTENANCE"}
        and esp_simulated_w is not None
    )

    if not comparable:
        return ComparisonPoint(
            compared_at=compared_at,
            sem_decision=sem_decision,
            sem_mapped_strategy=sem_strategy,
            sem_amps=sem_amps,
            sem_target_w=sem_target_w,
            esp_strategy=esp_strategy,
            esp_simulated_w=esp_simulated_w,
            actual_battery_w=actual_battery_w,
            actual_grid_w=actual_grid_w,
            actual_soc=actual_soc,
            comparable=False,
            concordant=None,
            direction_match=None,
            strategy_match=None,
            watt_diff=None,
            reason="NOT_COMPARABLE",
        )

    assert sem_strategy is not None
    assert esp_simulated_w is not None

    # Compare ESP simulated setpoint against real inverter battery power when in SELF_USE,
    # or against SEM target charge power when SEM commands CHARGE_FIRST.
    ref_w = actual_battery_w if sem_strategy == "SELF_USE" else sem_target_w
    dir_sem = _direction(ref_w, cfg.deadband_w)
    dir_esp = _direction(esp_simulated_w, cfg.deadband_w)
    direction_match = (dir_sem == dir_esp) if (dir_sem is not None and dir_esp is not None) else None

    comparable_strategy_esp = "CHARGE_FIRST" if esp_strategy in {"CHARGE_FIRST", "TARGET_SOC"} else esp_strategy
    strategy_match = sem_strategy == comparable_strategy_esp

    watt_diff: float | None = None
    if ref_w is not None:
        watt_diff = round(abs(esp_simulated_w - ref_w), 1)

    within_watts = (watt_diff <= cfg.watt_threshold) if watt_diff is not None else False
    concordant = bool(strategy_match and (direction_match is not False) and within_watts)

    if concordant:
        reason = "CONCORDANT"
    elif not strategy_match:
        reason = f"STRATEGY_DIVERGENCE ({sem_strategy} vs {esp_strategy})"
    elif direction_match is False:
        reason = f"DIRECTION_MISMATCH ({dir_sem} vs {dir_esp})"
    else:
        reason = f"DELTA_ABOVE_{int(cfg.watt_threshold)}W"

    return ComparisonPoint(
        compared_at=compared_at,
        sem_decision=sem_decision,
        sem_mapped_strategy=sem_strategy,
        sem_amps=sem_amps,
        sem_target_w=sem_target_w,
        esp_strategy=esp_strategy,
        esp_simulated_w=esp_simulated_w,
        actual_battery_w=actual_battery_w,
        actual_grid_w=actual_grid_w,
        actual_soc=actual_soc,
        comparable=True,
        concordant=concordant,
        direction_match=direction_match,
        strategy_match=strategy_match,
        watt_diff=watt_diff,
        reason=reason,
    )


def compute_setpoint_churn(
    series: Iterable[tuple[datetime, float | None]],
    *,
    delta_threshold_w: float = 100.0,
) -> float | None:
    """Counts setpoint jumps > delta_threshold_w per hour."""
    pts = [(ts, w) for ts, w in series if w is not None]
    if len(pts) < 2:
        return None
    span_h = (pts[-1][0] - pts[0][0]).total_seconds() / 3600.0
    if span_h <= 0:
        return None
    jumps = 0
    for idx in range(1, len(pts)):
        if abs(pts[idx][1] - pts[idx - 1][1]) > delta_threshold_w:
            jumps += 1
    return round(jumps / span_h, 2)


def summarize_comparisons(
    points: list[ComparisonPoint],
    *,
    safety_interventions: int = 0,
    setpoint_oscillations_per_hour: float | None = None,
) -> ComparisonAggregate:
    comp = [p for p in points if p.comparable]
    concordant = [p for p in comp if p.concordant is True]
    divergent = [p for p in comp if p.concordant is False]

    diffs = sorted(p.watt_diff for p in comp if p.watt_diff is not None)
    mean_diff = round(sum(diffs) / len(diffs), 1) if diffs else None
    p95_diff: float | None = None
    if diffs:
        idx = min(len(diffs) - 1, max(0, math.ceil(0.95 * len(diffs)) - 1))
        p95_diff = round(diffs[idx], 1)

    false_positives = sum(
        1
        for p in divergent
        if p.sem_mapped_strategy == "SELF_USE" and p.esp_strategy in {"CHARGE_FIRST", "TARGET_SOC"}
    )
    missed_opps = sum(
        1
        for p in divergent
        if p.sem_mapped_strategy == "CHARGE_FIRST" and p.esp_strategy == "SELF_USE"
    )

    strategy_share: dict[str, int] = {}
    for p in points:
        key = p.esp_strategy or "UNKNOWN"
        strategy_share[key] = strategy_share.get(key, 0) + 1

    concordance_pct = round((len(concordant) / len(comp)) * 100.0, 1) if comp else None

    return ComparisonAggregate(
        total_points=len(points),
        comparable_points=len(comp),
        concordant_points=len(concordant),
        divergent_points=len(divergent),
        concordance_pct=concordance_pct,
        mean_watt_diff=mean_diff,
        p95_watt_diff=p95_diff,
        false_positives=false_positives,
        missed_opportunities=missed_opps,
        safety_interventions=safety_interventions,
        setpoint_oscillations_per_hour=setpoint_oscillations_per_hour,
        strategy_share=strategy_share,
    )
