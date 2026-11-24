# AUX abstraction

AUX1 is the single generic output. Reserve a stable channel ID and avoid an energy role, DRM mapping or command simulation through the relay.

| Contract | Fields and semantics |
|---|---|
| AuxChannel | Stable id `AUX1`, physical capability, off-only driver binding, verified electrical state flag. GPIO is board-private. |
| AuxMode | `DISABLED` in this release. Future capability extensions require a separate build qualification. |
| AuxState | `UNAVAILABLE`, `DISABLED`, `OFF`, `REQUESTED_ON`, `REQUESTED_OFF`, `BLOCKED`, `FAULT`. Use separate `mode`, `effective_state`, `request_disposition` and `physical_feedback` so a rejected request cannot imply coil motion. AUX1 mode and normal effective state are DISABLED. Fault diagnostics may report FAULT/UNAVAILABLE independently. |
| AuxCommand | Channel, desired state, origin, correlation ID, requested UTC/monotonic time and optional TTL. Origins `LOCAL_EMS`, `MQTT`, `REST_API`, `WEB_UI`, `SCHEDULE`, `SAFETY_ENGINE`. No remote command endpoint is installed. Legacy or malformed requests at authenticated ingress are rejected/audited as attempts. |
| AuxInterlock | Compile-time off-only driver, highest-priority Safety veto, no user override, single hardware owner, no remembered ON state. |
| AuxAuditEvent | Boot ID, sequence, source, desired state, disabled denial reason, effective state, feedback provenance, and `executed=false`. Deduplicate repeated IDs and meter overflow. |

GPIO47 active high and LOW de-energized are the schematic/demo baseline. Before creating tasks or initializing services, set the output latch LOW before enabling output drive using the verified HAL sequence. During ROM boot, reset and power ramp the pad may be high impedance. The coil circuit and measured behavior determine whether the complete board is safe during that interval. Scope GPIO and coil, and verify the NO contact stays open with a low-energy continuity setup. Bench tests leave relay terminals unloaded other than the measurement circuit, with RS485/inverter disconnected. Exercise normal boot, cold replug, manual reset, watchdog, panic and controlled brownout as specified in [bring-up plan](09_HARDWARE_BRINGUP_PLAN.md). Reject any pulse that leaves the characterized OFF electrical envelope or any contact closure. Quarantine the board/image pending root cause if either occurs.

REST and MQTT emit mode DISABLED, the OFF design target, disabled reason and verification status. For ordinary operation without contact sensing, emit `physical_feedback=null`; the model records the commanded safe output and bench qualification rather than claiming a live contact reading. State reports and simulated EMS setpoints do not touch AUX hardware. Firmware startup always reasserts LOW and ignores legacy relay state/configuration.
