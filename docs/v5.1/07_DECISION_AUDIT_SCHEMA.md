# Decision audit schema v1

One canonical typed model `DecisionAuditV1` is serialized identically by REST and MQTT. The container stores the original payload, validates the version, deduplicates by `(device_id, boot_id, seq)`, and projects fields for analytics/export. Use UTC RFC 3339 when trusted. UTC is null for invalid/unset time, with an explicit time quality and monotonic uptime. A random boot ID scopes a strictly increasing uint64 sequence assigned by the sole audit writer (decimal string on JSON wire). Reboot starts a new stream without per-event persistent counters.

| Field | Contract |
|---|---|
| Header | `schema_version`, `event_type`, `device_id`, `boot_id`, `seq`, `uptime_ms`, `timestamp_utc`, `time_quality`, `firmware_version`, `config_revision`, `operating_mode` |
| Strategy | Selected configured strategy, active strategy, source, request/correlation ID if any, decision reason codes and bounded detail |
| Telemetry snapshot | Snapshot ID and age/skew, SOC %, PV DC W, load AC W, grid W, battery W. Each uses nullable numeric value, unit, source, provenance, measured/derived quality, monotonic sample age, validity and scope. Optional BMS/status and limits include the same metadata. |
| Advice and forecasts | Provider, source/advice ID, revision, timestamps, age, quality, expiry, accepted/rejected reason and fields actually used, with no credentials. Include per-series age and planning horizon. |
| Intent and constraints | Requested override or schedule, candidate, ordered constraints with IDs and before/after, safety result and reason, final simulated output, and optional model version. |
| Execution | Executor result (SIMULATED, NO_ACTION or BLOCKED), executed=false, modbus_write_count=0, and actually_applied=null. Inverter behavior is observed telemetry. |
| AUX / errors | Mode, disable reason, physical feedback provenance and last attempt or linked AUX event. Errors are stable codes plus bounded details, including transport/config/time and data gap events. |

## Example

This is a synthetic schema example.

```json
{
  "schema_version": 1,
  "event_type": "ems_decision",
  "device_id": "ems-01",
  "boot_id": "68e72191-d2bf-4ca8-8845-8d38b6707f62",
  "seq": "42",
  "uptime_ms": 42000,
  "timestamp_utc": "2026-11-18T12:00:00Z",
  "time_quality": "RTC_VALID",
  "firmware_version": "5.1.0-shadow.1",
  "config_revision": 3,
  "operating_mode": "SHADOW",
  "strategy": {"configured":"SELF_USE","active":"SELF_USE","source":"LOCAL_CONFIG"},
  "telemetry": {
    "snapshot_id":"poll-14","max_skew_ms":420,
    "soc":{"value":36,"unit":"%","kind":"measured","quality":"VALID","age_ms":750,"source":"MODBUS_FC04"},
    "pv":{"value":3000,"unit":"W","kind":"measured","quality":"VALID","age_ms":1010,"source":"MODBUS_FC04"},
    "load":{"value":2150,"unit":"W","kind":"derived","quality":"VALID","age_ms":1100,"source":"AC_BALANCE"},
    "grid":{"value":-700,"unit":"W","kind":"measured","quality":"VALID","age_ms":1100,"source":"PCC_METER"},
    "battery":{"value":0,"unit":"W","kind":"measured","quality":"VALID","age_ms":750,"source":"MODBUS_FC04"}
  },
  "advice":{"received":[],"used":[],"forecast_age_ms":null},
  "requested":null,
  "candidate":{"kind":"simulated","battery_w":700},
  "constraints":[{"code":"CHARGE_POWER_CAP","before_w":700,"after_w":500}],
  "safety":{"result":"LIMITED","reason_codes":["CHARGE_POWER_CAP"]},
  "final_simulated":{"kind":"safety-limited","battery_w":500},
  "reason_codes":["SURPLUS_PV","CHARGE_POWER_CAP"],
  "executor":{"result":"SIMULATED","executed":false,"modbus_write_count":0,"actually_applied":null},
  "aux":{"id":"AUX1","mode":"DISABLED","effective_state":"DISABLED","physical_feedback":null},
  "errors":[]
}
```

Provenance kinds are `measured`, `derived`, `forecast`, `simulated`, `requested`, `safety-limited` and `actually applied` (wire key `actually_applied`). Quality and source are independent of kind. Serialize missing values as null, including freshness for never-sampled inputs. Forecast advice links by ID to the validated forecast and records the used horizon/slots. An actually applied field can only be populated by a qualified future executor. This schema's SHADOW validation enforces null.

## Bounded buffering and transport

Use preallocated RAM slots for every evaluated decision, plus an independent bounded fault/denial queue so telemetry bursts cannot hide all safety evidence. Initial design budget 256 compact decision records at ≤256 B each (64 KiB), plus 32 fault/denial records at ≤256 B (8 KiB), and bounded serialization buffers outside the control path. Keep complete semantic fields using bounded enums and references with lifetime at least the retained record horizon. Profile compile-time sizing and place noncritical buffers in PSRAM when qualified. Use fixed per-message input caps and reject payload overflow with a diagnostic. At 1 Hz, 256 slots give approximately four minutes; longer outages can lose old records. Emit gap ranges, record counts and reboot markers; overwrite oldest deterministically without blocking decisions. Expose the lost range separately from MQTT acknowledgement. Store configuration only on validated change; no audit flash writes by default. Any future persistent fault journal needs a write budget and power-loss qualification.

REST `GET /api/v1/decisions?boot_id=...&after_seq=...&limit=...` returns bounded pages and oldest/newest/next/gap metadata. MQTT publishes the same decision record on `ems/<id>/v1/decision` QoS 1, non-retained, with bounded replay after reconnect and deduplication. Container fetches missing ranges through read-only REST when possible and emits gaps for unrecoverable history. Test overflow, replay, teardown, missing UTC, 64-bit sequence serialization, duplicate conflicts, version mismatches, payload size and representation parity. Never mark an unrecoverable interval as complete.
