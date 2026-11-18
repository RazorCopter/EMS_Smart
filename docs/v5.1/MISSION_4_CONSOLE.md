# Mission 4 · ESP32 console and MQTT

The authenticated console is served at http://10.0.0.110/. Its compressed HTML, CSS and JavaScript share the ESP32 application binary. The decision buffer is volatile, holds 256 structured records in PSRAM, and is separate from the system log.

## Console

- Overview supplies measured SOC, battery W/V/A, inverter AC W and heatsink temperature, PV total and both MPPT W/V/A, grid W/V/Hz, and derived AC load. Each qualified field has quality, origin and last-sample age.
- EMS strategy settings include local selection, return strategy, SOC target, separate Charge First, Target SOC and Maintenance limits/timeouts, telemetry age, grid charging and surplus-only policy.
- Battery settings include SOC minimum, maximum and reactivation, current and power limits, voltage limits, provisional battery capacity and efficiency, deadband, ramp, grid exchange target and export cap. An ampere constraint is converted at the latest valid battery voltage and intersects the watt constraints. Unavailable voltage blocks the affected simulated direction when its ampere limit is enabled.
- MQTT settings are enabled, broker host, port, TLS, username, masked password, device ID, topic root, discovery prefix, QoS 0/1, retain, telemetry and diagnostic periods, keepalive and network timeout.
- System includes Wi-Fi setup, diagnostics, reboot, export, validated import, per-section rollback and signed OTA. Browser OTA accepts the firmware, SHA-256 and hex DER ECDSA signature. Both the CLI and UI use the same signed upload endpoint.

The EMS policy shares its parser, range validation, persistence and lease handling between REST, UI and MQTT. Each section is committed as a validated NVS blob and retains the last valid previous blob. EMS and broker revisions are reported for the current boot. The combined browser restore rolls back EMS if the broker section is rejected.

## REST

| Endpoint | Method and payload |
|---|---|
| /api/v1/health, /api/v1/system, /api/v1/telemetry | GET |
| /api/v1/ems/config | GET or POST integer URL-encoded settings; rollback=1 restores the previous policy |
| /api/v1/ems/decision | GET, latest decision or null |
| /api/v1/ems/decisions | GET, newline-delimited JSON in chronological order |
| /api/v1/mqtt | GET, POST JSON settings or {"rollback":true}; password accepted only on write |
| /api/v1/mqtt/test | POST, reconnects with the saved settings |
| /api/v1/reboot | POST |
| /api/v1/ota | POST signed firmware image |
| /api/v1/system/wifi-test | POST, authenticated bounded eight-second network dropout |

## MQTT and Home Assistant

Default topic root: `afore/afore-ems/v1`. Suffixes: `availability`, `telemetry`, `ems/state`, `ems/decision`, `ems/command`, `ems/ack`, `config/state`, `diagnostics`.

Commands require schema 1, a unique message_id, Unix timestamp seconds, TTL 1–600 s and a payload of allowed integer policy fields. Actions: policy, override, target_soc, maintenance, cancel_override, cancel_session. Overrides are RAM-only and expire against monotonic elapsed time. A stale or retained command can never reapply a previous policy at reconnection. Commands are rejected if the device has no valid clock.

Example command with an illustrative timestamp (generate the actual current timestamp when sending):

```json
{"schema":1,"message_id":"ui-20261118-001","timestamp":1795024800,"ttl":30,"action":"override","payload":{"strategy":2}}
```

Expected success ACK:

```json
{"schema":1,"message_id":"ui-20261118-001","accepted":true,"reason":"ACCEPTED","executed":false}
```

Discovery uses one device identifier, with 36 unique entity IDs in the default configuration. The [captured Discovery payloads](evidence/M4_DISCOVERY.json) include every entity and its metadata. Control entities are the strategy select; min/max/target SOC, charge/discharge power, charge/discharge current and min/max voltage numbers; simulated grid charge switch; and cancellation buttons. Diagnostics include valid telemetry, MQTT, SHADOW and bus fault. Sensor entities cover all qualified telemetry channels, effective strategy and simulated power. A dedicated Home Assistant instance was not provided for the broker test. Discovery follows the [Home Assistant MQTT contract](https://www.home-assistant.io/integrations/mqtt/).

## Screenshots

[Desktop](evidence/M4_desktop.png) · [Mobile](evidence/M4_mobile.png)

## Bench acceptance

Firmware v5.1-console-2, signed OTA SHA-256 5660bc6859c915a09036346724dd65fce730ecc420a03f092941e7751ed3f1c7, confirmed valid. The previous validated image remained available in the alternate slot. Configuration and browser tests exercised all tabs, save, backup without secrets, import, EMS and MQTT rollback, strategy selection, ampere conversion and broker connection test. The firmware served a complete 256-record ordered decision ring.

The temporary authenticated LAN broker saw all 36 unique Discovery messages with one shared device identifier. Tests passed for accepted, expired, duplicate and retained commands, and an expiring override returned to the local strategy. Stopping the broker and an eight-second Wi-Fi radio interruption both preserved FC04 acquisition. MQTT reconnected on recovery. A requested reboot preserved the policy and reconnected to MQTT. Final ACK replay was repeated on the final firmware. The factory-default disabled broker settings were restored after the bench run and temporary credentials were purged from both active and previous broker configuration.

The final firmware completed 3,608.80 s of sampled observation. The 345 samples show FC04 readings 5 to 7,974 (7,969 in window), zero errors and reset-free uptime. Decision sequence advanced from 19 to 3,561. Heap 159,564 B at start, 159,092 B at end and 155,732 B minimum across samples. Firmware low-water heap was 149,612 B. RSSI -62 to -58 dBm. Every sample showed broker connected, GPIO47 LOW, AUX1 DISABLED and zero Modbus writes. [Stability CSV](evidence/M4_STABILITY.csv) · [MQTT ACK evidence](evidence/M4_MQTT_ACKS.json).
