# Home Assistant Integration: Afore AF5K-SL Shadow Mode

This package integrates the Heliograph Afore AF5K-SL Shadow EMS daemon and firmware into Home Assistant.

## Security & Shadow Mode Invariants

1. **Strictly Read-Only**: The firmware and daemon operate under read-only guards (FC03/FC04 only).
2. **Zero Actuation**: No switches, relays, or command topics (`set`, `write`, `command`) are exposed to Home Assistant.
3. **No Safety Hazards**: Home Assistant is purely an observer. Inverter state cannot be altered remotely.

---

## Prerequisites

1. An operational MQTT Broker (e.g. Mosquitto add-on for Home Assistant).
2. MQTT integration configured in Home Assistant with MQTT Discovery enabled (`discovery_prefix: homeassistant`).
3. Heliograph configured to publish to:
   - Base Topic: `heliograph/afore_shadow`
   - Discovery Prefix: `homeassistant`

---

## MQTT Topics & Telemetry Schema

| Topic | Description | Payload Format |
|---|---|---|
| `heliograph/afore_shadow/status` | Bridge LWT availability | `online` / `offline` |
| `heliograph/afore_shadow/telemetry` | Periodic inverter telemetry | JSON payload with measurements & quality |
| `heliograph/afore_shadow/shadow_state`| Shadow engine health & quality summary | JSON payload with aggregated metrics |

### Sample Telemetry Payload (`heliograph/afore_shadow/telemetry`):
```json
{
  "battery_soc": 82.5,
  "grid_power_w": 1250.0,
  "battery_power_w": -800.0,
  "pv1_power_w": 1400.0,
  "pv2_power_w": 1100.0,
  "inverter_temperature_c": 38.2,
  "total_yield_kwh": 1452.8,
  "quality_battery_soc": "good",
  "age_battery_soc_ms": 250
}
```

---

## Installation Steps

1. Copy `afore_package.yaml` to your Home Assistant config directory under `packages/afore_package.yaml` (or merge into `configuration.yaml` under `homeassistant.packages`).
2. Reload YAML configuration or restart Home Assistant (`Developer Tools -> YAML -> Reload All`).
3. Check discovered entities under **Settings -> Devices & Services -> MQTT -> Afore AF5K-SL Inverter**:
   - `sensor.afore_v4_battery_soc`
   - `sensor.afore_v4_grid_power`
   - `sensor.afore_v4_battery_power`
   - `sensor.afore_v4_pv1_power`
   - `sensor.afore_v4_pv2_power`
   - `sensor.afore_v4_inverter_temperature`
   - `sensor.afore_v4_energy_total_yield`
   - `sensor.afore_v4_shadow_quality_summary`

---

## Home Assistant Energy Dashboard Configuration

Navigate to **Settings -> Dashboards -> Energy** and map the following entities:

1. **Electricity Grid**:
   - **Grid consumption**: `sensor.afore_grid_import_energy`
   - **Return to grid**: `sensor.afore_grid_export_energy`
2. **Solar Panels**:
   - **Solar production**: `sensor.afore_v4_energy_total_yield`
3. **Battery Systems**:
   - **Energy going in to battery**: `sensor.afore_battery_charge_energy`
   - **Energy coming out of battery**: `sensor.afore_battery_discharge_energy`

---

## Schema & Consistency Validation

- `state_class`:
  - Instantaneous values (Power, Voltage, Current, SoC, Temperature): `measurement`
  - Cumulative counters (Yield, Imported Energy, Exported Energy): `total_increasing`
- `device_class`:
  - `power` for Watts (W)
  - `energy` for Kilowatt-hours (kWh)
  - `battery` for State of Charge (%)
  - `temperature` for Degrees Celsius (°C)
