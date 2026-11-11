#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Generate realistic Home Assistant historical replay fixtures for M5."""

import csv
import json
import math
from pathlib import Path

FIXTURES_DIR = (
    Path(__file__).resolve().parent.parent / "test" / "test_ha_replay" / "fixtures"
)


def generate_fixtures():
    FIXTURES_DIR.mkdir(parents=True, exist_ok=True)

    scenarios = [
        "sunny_day",
        "foggy_day",
        "evening_load_spike",
        "missing_forecast_day",
    ]

    for scenario in scenarios:
        records = []
        # 24 hours in 15-minute intervals = 96 steps
        for step in range(96):
            minute = step * 15
            hour_float = minute / 60.0
            time_str = f"{minute // 60:02d}:{minute % 60:02d}"

            # Base solar profile (bell curve between 06:00 and 19:30, peak at 13:00)
            solar_rad = 0.0
            if 6.0 <= hour_float <= 19.5:
                solar_rad = 850.0 * math.sin((hour_float - 6.0) / 13.5 * math.pi)
                solar_rad = max(0.0, solar_rad)

            temp = 12.0 + 8.0 * math.sin((hour_float - 8.0) / 14.0 * math.pi)
            dew_point = 8.0

            # Default load (base 400W, lunch bump at 12:30, evening bump at 20:00)
            load = (
                400.0
                + 200.0 * math.exp(-((hour_float - 12.5) ** 2) / 2.0)
                + 350.0 * math.exp(-((hour_float - 20.0) ** 2) / 3.0)
            )

            fog_flag = 0
            forecast_today = 18.5
            forecast_tomorrow = 17.0
            forecast_age_sec = (
                int(hour_float * 3600) % 21600 + 300
            )  # refreshed every 6h

            # Scenario adjustments
            if scenario == "sunny_day":
                # Clear sky
                pv_power = solar_rad * 4.8  # up to ~4000W
                fog_flag = 0
                actual_ratio = 1.0

            elif scenario == "foggy_day":
                # Fog until 11:30
                if hour_float <= 11.5:
                    fog_flag = 1
                    temp = 4.0
                    dew_point = 3.5
                    solar_rad = min(solar_rad * 0.25, 80.0)
                    pv_power = solar_rad * 3.0
                    actual_ratio = 0.4
                    forecast_today = 8.0
                else:
                    fog_flag = 0
                    temp = 9.0
                    dew_point = 4.0
                    solar_rad *= 0.85
                    pv_power = solar_rad * 4.5
                    actual_ratio = 0.85
                    forecast_today = 8.0

            elif scenario == "evening_load_spike":
                # High evening cooking / HVAC load (up to 3800W)
                pv_power = solar_rad * 4.5
                if 17.5 <= hour_float <= 21.5:
                    load += 2800.0 * math.exp(-((hour_float - 19.5) ** 2) / 1.5)
                actual_ratio = 1.0
                forecast_today = 16.0

            elif scenario == "missing_forecast_day":
                # Forecast sensor unavailable / stale (> 16 hours)
                pv_power = solar_rad * 4.0
                forecast_today = 0.0
                forecast_tomorrow = 0.0
                forecast_age_sec = 16 * 3600 + 100  # stale!
                actual_ratio = 1.0

            # Battery SOC & Native Inverter comparison behavior
            # Inverter native behavior: aggressively charges from PV surplus as fast as possible until 100%
            if hour_float < 7.0:
                soc = 35.0
                native_battery_power = 0.0
            elif hour_float < 13.0:
                progress = (hour_float - 7.0) / 6.0
                soc = 35.0 + progress * (55.0 if scenario == "foggy_day" else 65.0)
                native_battery_power = min(2500.0, max(0.0, pv_power - load))
            elif hour_float < 18.0:
                soc = 90.0 if scenario == "foggy_day" else 100.0
                native_battery_power = 200.0 if soc < 100.0 else 0.0
            else:
                discharge_factor = (hour_float - 18.0) * 4.0
                soc = max(40.0, 100.0 - discharge_factor)
                native_battery_power = -min(load, 2000.0)

            # Grid power balance
            grid_power = load - pv_power + native_battery_power

            records.append(
                {
                    "time": time_str,
                    "minute_of_day": minute,
                    "sensor.afore_soc": round(soc, 1),
                    "sensor.afore_battery_power": round(native_battery_power, 1),
                    "sensor.afore_pv_power": round(pv_power, 1),
                    "sensor.afore_grid_power": round(grid_power, 1),
                    "sensor.afore_load_power": round(load, 1),
                    "sensor.solcast_pv_forecast_today": round(forecast_today, 2),
                    "sensor.solcast_pv_forecast_tomorrow": round(forecast_tomorrow, 2),
                    "forecast_age_seconds": forecast_age_sec,
                    "sensor.temperatura_esterna": round(temp, 1),
                    "sensor.punto_rugiada": round(dew_point, 1),
                    "sensor.radiazione": round(solar_rad, 1),
                    "actual_to_forecast_ratio": round(actual_ratio, 2),
                    "binary_sensor.sem_nebbia_stabile": fog_flag,
                    "native_inverter_battery_power": round(native_battery_power, 1),
                }
            )

        # Save JSON
        json_path = FIXTURES_DIR / f"{scenario}.json"
        with open(json_path, "w", encoding="utf-8") as f:
            json.dump(
                {
                    "scenario": scenario,
                    "sample_count": len(records),
                    "interval_minutes": 15,
                    "records": records,
                },
                f,
                indent=2,
            )

        # Save CSV
        csv_path = FIXTURES_DIR / f"{scenario}.csv"
        fieldnames = list(records[0].keys())
        with open(csv_path, "w", newline="", encoding="utf-8") as f:
            writer = csv.DictWriter(f, fieldnames=fieldnames)
            writer.writeheader()
            writer.writerows(records)

    print(f"Generated {len(scenarios)} replay fixtures in {FIXTURES_DIR}")


if __name__ == "__main__":
    generate_fixtures()
