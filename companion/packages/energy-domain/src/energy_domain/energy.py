from __future__ import annotations

from dataclasses import dataclass
from datetime import datetime


@dataclass(frozen=True)
class EnergyTotals:
    pv_wh: float | None
    load_wh: float | None
    grid_import_wh: float | None
    grid_export_wh: float | None
    battery_charge_wh: float | None
    battery_discharge_wh: float | None
    self_consumed_pv_wh: float | None
    estimated_losses_wh: float | None
    valid_seconds: float
    gap_seconds: float


@dataclass(frozen=True)
class KpiSummary:
    pv_kwh: float | None
    load_kwh: float | None
    grid_import_kwh: float | None
    grid_export_kwh: float | None
    battery_charge_kwh: float | None
    battery_discharge_kwh: float | None
    self_consumed_pv_kwh: float | None
    estimated_losses_kwh: float | None
    self_consumption_pct: float | None
    self_sufficiency_pct: float | None
    equivalent_cycles: float | None
    battery_throughput_kwh: float | None
    data_availability_pct: float | None
    valid_seconds: float
    gap_seconds: float


@dataclass(frozen=True)
class BatteryStats:
    soc_min: float | None
    soc_max: float | None
    soc_avg: float | None
    charge_kwh: float | None
    discharge_kwh: float | None
    throughput_kwh: float | None
    equivalent_cycles: float | None
    max_charge_w: float | None
    max_discharge_w: float | None
    time_below_reserve_s: float
    time_high_soc_s: float


class EnergyAccumulator:
    """Integrates instantaneous power samples into Wh over contiguous intervals.

    Gaps exceeding `max_gap_s` are counted in `gap_seconds` and never interpolated
    with zeros or stale values.
    """

    def __init__(self, max_gap_s: float = 60.0) -> None:
        self.max_gap_s = max_gap_s
        self.pv_wh: float = 0.0
        self.has_pv = False
        self.load_wh: float = 0.0
        self.has_load = False
        self.grid_import_wh: float = 0.0
        self.grid_export_wh: float = 0.0
        self.has_grid = False
        self.battery_charge_wh: float = 0.0
        self.battery_discharge_wh: float = 0.0
        self.has_battery = False
        self.self_consumed_pv_wh: float = 0.0
        self.has_self_consumed = False
        self.valid_seconds: float = 0.0
        self.gap_seconds: float = 0.0

    def add_interval(
        self,
        dt_seconds: float,
        *,
        pv_w: float | None,
        load_w: float | None,
        grid_w: float | None,
        battery_w: float | None,
    ) -> None:
        if dt_seconds <= 0:
            return
        if dt_seconds > self.max_gap_s:
            self.gap_seconds += dt_seconds
            return

        hours = dt_seconds / 3600.0
        self.valid_seconds += dt_seconds

        if pv_w is not None and pv_w >= 0:
            self.pv_wh += pv_w * hours
            self.has_pv = True
        if load_w is not None and load_w >= 0:
            self.load_wh += load_w * hours
            self.has_load = True
        if grid_w is not None:
            self.has_grid = True
            if grid_w >= 0:
                self.grid_import_wh += grid_w * hours
            else:
                self.grid_export_wh += (-grid_w) * hours
        if battery_w is not None:
            self.has_battery = True
            if battery_w >= 0:
                self.battery_charge_wh += battery_w * hours
            else:
                self.battery_discharge_wh += (-battery_w) * hours

        if pv_w is not None and grid_w is not None and pv_w >= 0:
            export_w = max(0.0, -grid_w)
            self.self_consumed_pv_wh += max(0.0, pv_w - export_w) * hours
            self.has_self_consumed = True

    def totals(self) -> EnergyTotals:
        losses_wh: float | None = None
        if self.has_pv and self.has_load and self.has_grid and self.has_battery:
            # Energy balance residual: (PV + Import + Discharge) - (Load + Export + Charge)
            inflow = self.pv_wh + self.grid_import_wh + self.battery_discharge_wh
            outflow = self.load_wh + self.grid_export_wh + self.battery_charge_wh
            losses_wh = round(max(0.0, inflow - outflow), 3)

        return EnergyTotals(
            pv_wh=round(self.pv_wh, 3) if self.has_pv else None,
            load_wh=round(self.load_wh, 3) if self.has_load else None,
            grid_import_wh=round(self.grid_import_wh, 3) if self.has_grid else None,
            grid_export_wh=round(self.grid_export_wh, 3) if self.has_grid else None,
            battery_charge_wh=round(self.battery_charge_wh, 3) if self.has_battery else None,
            battery_discharge_wh=round(self.battery_discharge_wh, 3) if self.has_battery else None,
            self_consumed_pv_wh=round(self.self_consumed_pv_wh, 3) if self.has_self_consumed else None,
            estimated_losses_wh=losses_wh,
            valid_seconds=round(self.valid_seconds, 2),
            gap_seconds=round(self.gap_seconds, 2),
        )


def integrate_energy_interval(
    prev_ts: datetime,
    curr_ts: datetime,
    *,
    pv_w: float | None,
    load_w: float | None,
    grid_w: float | None,
    battery_w: float | None,
    max_gap_s: float = 60.0,
) -> EnergyTotals:
    acc = EnergyAccumulator(max_gap_s=max_gap_s)
    acc.add_interval(
        (curr_ts - prev_ts).total_seconds(),
        pv_w=pv_w,
        load_w=load_w,
        grid_w=grid_w,
        battery_w=battery_w,
    )
    return acc.totals()


def compute_kpis(
    totals: EnergyTotals,
    *,
    battery_capacity_wh: float = 10240.0,
    expected_window_seconds: float | None = None,
) -> KpiSummary:
    """Compute energy KPIs preserving None whenever denominator or inputs are missing.

    Formulas:
    - Self-consumption (%) = (PV consumed locally incl. PV->battery / PV production) * 100
      where PV consumed locally = sum(max(0, pv_w - grid_export_w) * dt).
    - Self-sufficiency (%) = (Load covered by PV & battery / Total load) * 100
      where covered load = max(0, load_wh - grid_import_wh).
    - Equivalent cycles = (charge_wh + discharge_wh) / (2 * battery_capacity_wh).
    """

    def to_kwh(wh: float | None) -> float | None:
        return round(wh / 1000.0, 3) if wh is not None else None

    self_consumption_pct: float | None = None
    if (
        totals.pv_wh is not None
        and totals.pv_wh > 1.0
        and totals.self_consumed_pv_wh is not None
    ):
        self_consumption_pct = round(
            min(100.0, max(0.0, (totals.self_consumed_pv_wh / totals.pv_wh) * 100.0)),
            1,
        )

    self_sufficiency_pct: float | None = None
    if (
        totals.load_wh is not None
        and totals.load_wh > 1.0
        and totals.grid_import_wh is not None
    ):
        covered = max(0.0, totals.load_wh - totals.grid_import_wh)
        self_sufficiency_pct = round(
            min(100.0, max(0.0, (covered / totals.load_wh) * 100.0)),
            1,
        )

    throughput_wh: float | None = None
    cycles: float | None = None
    if totals.battery_charge_wh is not None or totals.battery_discharge_wh is not None:
        c = totals.battery_charge_wh or 0.0
        d = totals.battery_discharge_wh or 0.0
        throughput_wh = c + d
        if battery_capacity_wh > 0:
            cycles = round(throughput_wh / (2.0 * battery_capacity_wh), 3)

    total_span = expected_window_seconds or (totals.valid_seconds + totals.gap_seconds)
    availability: float | None = None
    if total_span > 0:
        availability = round(min(100.0, max(0.0, (totals.valid_seconds / total_span) * 100.0)), 2)

    return KpiSummary(
        pv_kwh=to_kwh(totals.pv_wh),
        load_kwh=to_kwh(totals.load_wh),
        grid_import_kwh=to_kwh(totals.grid_import_wh),
        grid_export_kwh=to_kwh(totals.grid_export_wh),
        battery_charge_kwh=to_kwh(totals.battery_charge_wh),
        battery_discharge_kwh=to_kwh(totals.battery_discharge_wh),
        self_consumed_pv_kwh=to_kwh(totals.self_consumed_pv_wh),
        estimated_losses_kwh=to_kwh(totals.estimated_losses_wh),
        self_consumption_pct=self_consumption_pct,
        self_sufficiency_pct=self_sufficiency_pct,
        equivalent_cycles=cycles,
        battery_throughput_kwh=to_kwh(throughput_wh),
        data_availability_pct=availability,
        valid_seconds=totals.valid_seconds,
        gap_seconds=totals.gap_seconds,
    )
