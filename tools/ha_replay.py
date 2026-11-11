#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Offline Historical Home Assistant Replay Engine for TargetAwareEMS.

Evaluates historical sensor data offline and prints Markdown performance reports.
Zero network, zero HTTP/REST, zero serial, zero Modbus. 100% offline.
"""

import argparse
import csv
import json
import sys
from pathlib import Path
from typing import Any


class ReplayRecord:
    def __init__(
        self,
        time_str: str,
        minute_of_day: int,
        soc: float,
        battery_power_w: float,
        pv_power_w: float,
        grid_power_w: float,
        load_power_w: float,
        forecast_today_kwh: float,
        forecast_tomorrow_kwh: float,
        forecast_age_seconds: int,
        temperature_c: float,
        dew_point_c: float,
        radiation_wm2: float,
        actual_to_forecast_ratio: float,
        stable_fog: bool,
        native_inverter_power_w: float,
    ):
        self.time_str = time_str
        self.minute_of_day = minute_of_day
        self.soc = soc
        self.battery_power_w = battery_power_w
        self.pv_power_w = pv_power_w
        self.grid_power_w = grid_power_w
        self.load_power_w = load_power_w
        self.forecast_today_kwh = forecast_today_kwh
        self.forecast_tomorrow_kwh = forecast_tomorrow_kwh
        self.forecast_age_seconds = forecast_age_seconds
        self.temperature_c = temperature_c
        self.dew_point_c = dew_point_c
        self.radiation_wm2 = radiation_wm2
        self.actual_to_forecast_ratio = actual_to_forecast_ratio
        self.stable_fog = stable_fog
        self.native_inverter_power_w = native_inverter_power_w


def parse_time_or_minute(val: str) -> int:
    val = val.strip()
    if ":" in val:
        try:
            parts = val.split(":")
            return int(parts[0]) * 60 + int(parts[1])
        except Exception:
            return 0
    try:
        return int(float(val))
    except Exception:
        return 0


def safe_float(val: Any, default: float = 0.0) -> float:
    if (
        val is None
        or val == ""
        or str(val).lower() in ("unavailable", "unknown", "none")
    ):
        return default
    try:
        return float(val)
    except Exception:
        return default


def load_file(path: Path) -> list[ReplayRecord]:
    if path.suffix.lower() == ".json":
        return load_json_file(path)
    return load_csv_file(path)


def load_json_file(path: Path) -> list[ReplayRecord]:
    with open(path, encoding="utf-8") as f:
        data = json.load(f)

    if isinstance(data, dict) and "records" in data:
        items = data["records"]
    elif isinstance(data, list):
        items = data
    else:
        return []

    records = []
    for item in items:
        time_str = str(item.get("time") or item.get("timestamp") or "")
        min_val = item.get("minute_of_day")
        if min_val is not None:
            min_of_day = int(min_val)
        else:
            min_of_day = parse_time_or_minute(time_str)

        soc = safe_float(item.get("sensor.afore_soc") or item.get("soc"))
        bp = safe_float(
            item.get("sensor.afore_battery_power") or item.get("battery_power")
        )
        pv = safe_float(item.get("sensor.afore_pv_power") or item.get("pv_power"))
        grid = safe_float(item.get("sensor.afore_grid_power") or item.get("grid_power"))
        load = safe_float(item.get("sensor.afore_load_power") or item.get("load_power"))
        ft = safe_float(
            item.get("sensor.solcast_pv_forecast_today") or item.get("forecast_today")
        )
        ftm = safe_float(
            item.get("sensor.solcast_pv_forecast_tomorrow")
            or item.get("forecast_tomorrow")
        )
        fa = int(safe_float(item.get("forecast_age_seconds"), 300.0))

        temp = safe_float(
            item.get("sensor.temperatura_esterna") or item.get("temperature"), 15.0
        )
        dew = safe_float(item.get("sensor.punto_rugiada") or item.get("dew_point"), 8.0)
        rad = safe_float(item.get("sensor.radiazione") or item.get("radiation"), 0.0)
        ratio = safe_float(item.get("actual_to_forecast_ratio"), 1.0)

        fog_raw = item.get("binary_sensor.sem_nebbia_stabile")
        if fog_raw is None:
            fog_raw = item.get("fog", False)
        fog = fog_raw in (True, 1, "1", "true", "on", "True")

        nip = safe_float(
            item.get("native_inverter_battery_power")
            or item.get("native_battery_power"),
            bp,
        )

        records.append(
            ReplayRecord(
                time_str=time_str,
                minute_of_day=min_of_day,
                soc=soc,
                battery_power_w=bp,
                pv_power_w=pv,
                grid_power_w=grid,
                load_power_w=load,
                forecast_today_kwh=ft,
                forecast_tomorrow_kwh=ftm,
                forecast_age_seconds=fa,
                temperature_c=temp,
                dew_point_c=dew,
                radiation_wm2=rad,
                actual_to_forecast_ratio=ratio,
                stable_fog=fog,
                native_inverter_power_w=nip,
            )
        )
    return records


def load_csv_file(path: Path) -> list[ReplayRecord]:
    records = []
    with open(path, encoding="utf-8") as f:
        reader = csv.DictReader(f)
        for row in reader:
            time_str = row.get("time") or row.get("timestamp") or ""
            min_val = row.get("minute_of_day")
            if min_val:
                min_of_day = parse_time_or_minute(min_val)
            else:
                min_of_day = parse_time_or_minute(time_str)

            soc = safe_float(row.get("sensor.afore_soc") or row.get("soc"))
            bp = safe_float(
                row.get("sensor.afore_battery_power") or row.get("battery_power")
            )
            pv = safe_float(row.get("sensor.afore_pv_power") or row.get("pv_power"))
            grid = safe_float(
                row.get("sensor.afore_grid_power") or row.get("grid_power")
            )
            load = safe_float(
                row.get("sensor.afore_load_power") or row.get("load_power")
            )
            ft = safe_float(
                row.get("sensor.solcast_pv_forecast_today") or row.get("forecast_today")
            )
            ftm = safe_float(
                row.get("sensor.solcast_pv_forecast_tomorrow")
                or row.get("forecast_tomorrow")
            )
            fa = int(safe_float(row.get("forecast_age_seconds"), 300.0))

            temp = safe_float(
                row.get("sensor.temperatura_esterna") or row.get("temperature"), 15.0
            )
            dew = safe_float(
                row.get("sensor.punto_rugiada") or row.get("dew_point"), 8.0
            )
            rad = safe_float(row.get("sensor.radiazione") or row.get("radiation"), 0.0)
            ratio = safe_float(row.get("actual_to_forecast_ratio"), 1.0)

            fog_raw = row.get("binary_sensor.sem_nebbia_stabile") or row.get("fog")
            fog = str(fog_raw).lower() in ("1", "true", "on")

            nip = safe_float(
                row.get("native_inverter_battery_power")
                or row.get("native_battery_power"),
                bp,
            )

            records.append(
                ReplayRecord(
                    time_str=time_str,
                    minute_of_day=min_of_day,
                    soc=soc,
                    battery_power_w=bp,
                    pv_power_w=pv,
                    grid_power_w=grid,
                    load_power_w=load,
                    forecast_today_kwh=ft,
                    forecast_tomorrow_kwh=ftm,
                    forecast_age_seconds=fa,
                    temperature_c=temp,
                    dew_point_c=dew,
                    radiation_wm2=rad,
                    actual_to_forecast_ratio=ratio,
                    stable_fog=fog,
                    native_inverter_power_w=nip,
                )
            )
    return records


class PythonTargetAwareEMS:
    """Python reference mirror of TargetAwareEMS core for CLI replay parity."""

    def __init__(
        self,
        target_soc_pct: float = 100.0,
        target_minute: int = 15 * 60 + 30,
        capacity_kwh: float = 10.24,
        max_charge_power_w: float = 2560.0,
        efficiency: float = 0.90,
        safety_margin: float = 1.20,
        deadband_w: float = 30.0,
        hysteresis_w: float = 100.0,
        ramp_up_w: float = 256.0,
        ramp_down_w: float = 410.0,
        forecast_max_age_sec: int = 14 * 3600,
    ):
        self.target_soc = target_soc_pct
        self.target_minute = target_minute
        self.capacity_kwh = capacity_kwh
        self.max_charge_power_w = max_charge_power_w
        self.efficiency = efficiency
        self.safety_margin = safety_margin
        self.deadband_w = deadband_w
        self.hysteresis_w = hysteresis_w
        self.ramp_up_w = ramp_up_w
        self.ramp_down_w = ramp_down_w
        self.forecast_max_age_sec = forecast_max_age_sec

        self.previous_setpoint = 0.0
        self.fog_active = False

    def shape_setpoint(self, requested_w: float, critical: bool) -> float:
        val = max(0.0, min(self.max_charge_power_w, requested_w))
        if abs(val) < self.deadband_w:
            val = 0.0
        if abs(val - self.previous_setpoint) < self.hysteresis_w:
            return self.previous_setpoint
        if not critical:
            delta = val - self.previous_setpoint
            clamped_delta = max(-self.ramp_down_w, min(self.ramp_up_w, delta))
            val = self.previous_setpoint + clamped_delta
        return val

    def evaluate(self, rec: ReplayRecord) -> tuple[float, str, bool]:
        # Fog detector
        if rec.stable_fog:
            self.fog_active = True
        elif (
            rec.radiation_wm2 >= 180.0 and (rec.temperature_c - rec.dew_point_c) >= 2.5
        ):
            self.fog_active = False

        forecast_valid = (
            rec.forecast_today_kwh > 0.0
            and rec.forecast_age_seconds <= self.forecast_max_age_sec
        )
        remaining_hours = max(0.0, (self.target_minute - rec.minute_of_day) / 60.0)

        # Corrected forecast
        forecast_ratio = max(0.5, min(1.0, rec.actual_to_forecast_ratio))
        if self.fog_active:
            forecast_ratio *= 0.3
        effective_forecast_kwh = (
            rec.forecast_today_kwh * forecast_ratio if forecast_valid else 0.0
        )

        # Deficit calculation
        deficit_kwh = max(0.0, self.target_soc - rec.soc) * self.capacity_kwh / 100.0

        if remaining_hours > 0 and self.efficiency > 0:
            req_power_w = (
                deficit_kwh
                * self.safety_margin
                * 1000.0
                / (remaining_hours * self.efficiency)
            )
        else:
            req_power_w = float("inf") if deficit_kwh > 0 else 0.0

        is_critical = req_power_w >= self.max_charge_power_w * 0.9 or (
            deficit_kwh > 0 and remaining_hours <= 1.0
        )
        night = rec.pv_power_w < self.deadband_w and effective_forecast_kwh <= 0.0
        in_recovery = 0 < remaining_hours <= 1.0

        if deficit_kwh <= 0.0:
            reason = "TargetSatisfied"
            requested_w = 0.0
        elif night and not in_recovery:
            reason = "NightIdle"
            requested_w = 0.0
        elif in_recovery:
            reason = "RecoveryCharge"
            requested_w = self.max_charge_power_w
        elif not forecast_valid:
            reason = "ForecastUnavailable"
            requested_w = self.max_charge_power_w
        else:
            reason = "TargetCharge"
            requested_w = min(self.max_charge_power_w, req_power_w)

        shaped = self.shape_setpoint(requested_w, is_critical)
        self.previous_setpoint = shaped
        return shaped, reason, self.fog_active


def run_replay(records: list[ReplayRecord], scenario_name: str) -> dict[str, Any]:
    if not records:
        return {}

    ems = PythonTargetAwareEMS()
    step_results = []
    reason_counts: dict[str, int] = {}
    prev_setpoint = 0.0
    prev_minute = 0
    total_ramp = 0.0
    total_div = 0.0
    max_ramp = 0.0
    max_div = 0.0
    total_kwh = 0.0

    target_minute = 15 * 60 + 30
    closest_target_diff = 9999
    target_soc = 0.0
    target_reached_min = -1
    full_soc_reached_min = -1

    for i, rec in enumerate(records):
        setpoint, reason, _fog_active = ems.evaluate(rec)
        reason_counts[reason] = reason_counts.get(reason, 0) + 1

        dt_hours = 0.25
        if i > 0 and rec.minute_of_day > prev_minute:
            dt_hours = (rec.minute_of_day - prev_minute) / 60.0

        ramp = (
            abs(setpoint - prev_setpoint) / dt_hours
            if (i > 0 and dt_hours > 0)
            else 0.0
        )
        max_ramp = max(max_ramp, ramp)
        total_ramp += ramp

        div = abs(setpoint - rec.native_inverter_power_w)
        max_div = max(max_div, div)
        total_div += div

        if setpoint > 0.0:
            total_kwh += setpoint * dt_hours / 1000.0

        diff_target = abs(rec.minute_of_day - target_minute)
        if diff_target < closest_target_diff:
            closest_target_diff = diff_target
            target_soc = rec.soc

        if target_reached_min < 0 and rec.soc >= 100.0:
            target_reached_min = rec.minute_of_day
        if full_soc_reached_min < 0 and rec.soc >= 99.9:
            full_soc_reached_min = rec.minute_of_day

        step_results.append(
            {
                "minute": rec.minute_of_day,
                "soc": rec.soc,
                "setpoint": setpoint,
                "reason": reason,
                "native": rec.native_inverter_power_w,
                "div": div,
            }
        )

        prev_setpoint = setpoint
        prev_minute = rec.minute_of_day

    n = len(records)
    avg_ramp = total_ramp / n if n > 0 else 0.0
    avg_div = total_div / n if n > 0 else 0.0

    return {
        "scenario": scenario_name,
        "sample_count": n,
        "target_soc_at_1530": target_soc,
        "target_reached_minute": target_reached_min,
        "full_soc_reached_minute": full_soc_reached_min,
        "max_ramp_w_per_hour": max_ramp,
        "avg_ramp_w_per_hour": avg_ramp,
        "native_div_mae_w": avg_div,
        "native_div_max_w": max_div,
        "total_kwh": total_kwh,
        "reason_counts": reason_counts,
    }


def format_markdown_report(metrics: dict[str, Any]) -> str:
    lines = []
    lines.append(f"### Replay Report: `{metrics['scenario']}`\n")
    lines.append("| Metrica | Valore |")
    lines.append("|---|---|")
    lines.append(f"| Campioni totali | {metrics['sample_count']} |")
    lines.append(
        f"| SOC alle 15:30 (target time) | {metrics['target_soc_at_1530']:.1f}% |"
    )

    tr = metrics["target_reached_minute"]
    if tr >= 0:
        lines.append(
            f"| Raggiungimento target | {tr // 60:02d}:{tr % 60:02d} (minuto {tr}) |"
        )
    else:
        lines.append("| Raggiungimento target | Non raggiunto |")

    fr = metrics["full_soc_reached_minute"]
    if fr >= 0:
        lines.append(
            f"| Raggiungimento 100% | {fr // 60:02d}:{fr % 60:02d} (minuto {fr}) |"
        )
    else:
        lines.append("| Raggiungimento 100% | Non raggiunto |")

    lines.append(
        f"| Variazione setpoint max | {metrics['max_ramp_w_per_hour']:.1f} W/h |"
    )
    lines.append(
        f"| Variazione setpoint media | {metrics['avg_ramp_w_per_hour']:.1f} W/h |"
    )
    lines.append(f"| Divergenza nativa MAE | {metrics['native_div_mae_w']:.1f} W |")
    lines.append(f"| Divergenza nativa Max | {metrics['native_div_max_w']:.1f} W |")
    lines.append(f"| Energia raccomandata totale | {metrics['total_kwh']:.2f} kWh |\n")

    lines.append("#### Decision Breakdown (Reason Codes)\n")
    lines.append("| Reason Code | Conteggio | Percentuale |")
    lines.append("|---|---|---|")
    total_steps = metrics["sample_count"]
    for code, count in sorted(metrics["reason_counts"].items(), key=lambda x: -x[1]):
        pct = (100.0 * count / total_steps) if total_steps > 0 else 0.0
        lines.append(f"| `{code}` | {count} | {pct:.1f}% |")

    return "\n".join(lines)


def main():
    parser = argparse.ArgumentParser(
        description="Offline Historical Home Assistant Replay for TargetAwareEMS"
    )
    parser.add_argument(
        "files",
        nargs="*",
        help="Path to CSV or JSON historical file(s). If omitted, scans test/test_ha_replay/fixtures/",
    )
    parser.add_argument(
        "--output",
        "-o",
        type=str,
        default=None,
        help="Optional path to write generated Markdown report",
    )
    args = parser.parse_args()

    paths = []
    if args.files:
        for f in args.files:
            p = Path(f)
            if p.is_dir():
                paths.extend(sorted(p.glob("*.json")))
            elif p.is_file():
                paths.append(p)
    else:
        fixtures_dir = (
            Path(__file__).resolve().parent.parent
            / "test"
            / "test_ha_replay"
            / "fixtures"
        )
        if fixtures_dir.exists():
            paths = sorted(fixtures_dir.glob("*.json"))

    if not paths:
        print("No historical input files found to replay.", file=sys.stderr)
        sys.exit(1)

    reports = []
    for p in paths:
        records = load_file(p)
        scenario_name = p.stem
        metrics = run_replay(records, scenario_name)
        report = format_markdown_report(metrics)
        reports.append(report)
        print(report)
        print("\n" + "=" * 60 + "\n")

    if args.output:
        out_path = Path(args.output)
        out_path.parent.mkdir(parents=True, exist_ok=True)
        with open(out_path, "w", encoding="utf-8") as f:
            f.write("\n\n---\n\n".join(reports))
        print(f"Report saved to {out_path}")


if __name__ == "__main__":
    main()
