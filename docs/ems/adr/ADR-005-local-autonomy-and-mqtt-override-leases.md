# ADR-005: Local Autonomy, Multi-Master Protection, and MQTT Override Leases

## Context
When an EMS integrates with smart home platforms like Home Assistant, network partitions, broker crashes, or automated automations running amok can result in stale or erratic setpoint commands being fed to the inverter. If the controller blindly surrenders control to remote commands, a disconnected broker leaves the home battery unmanaged or locked in an unintended state.

## Decision
1. **Local Autonomous Primacy**:
   - The device controller is self-contained. It contains all logic necessary to balance power, protect the battery, and schedule charging without Home Assistant.
   - Home Assistant is treated as an advisory supervisor and telemetry consumer, NOT the essential brain of the system.
2. **Lease-Based Override Model**:
   - Any remote command from Home Assistant (via MQTT `afore/set/...` or REST `/api/v1/ems/operations`) MUST specify an explicit duration / lease (TTL, e.g. 1800 seconds).
   - If the lease expires without an active renewal heartbeat from Home Assistant, the controller automatically and cleanly revokes the override and returns to local autonomous Self-Use.
   - The controller enforces maximum lease caps (e.g. max 4 hours for manual forced charge) to prevent permanent accidental pinning.
3. **Multi-Master Protection on Modbus RS485**:
   - Modbus RTU is strictly single-master.
   - Exactly one OS thread/process owns the serial port (`/dev/ttyUSB0` on Linux, UART on ESP32).
   - Remote interfaces (REST, MQTT, Web GUI) communicate with the EMS core in-memory; they NEVER issue raw Modbus frames or open the serial device directly.
4. **Distinction of States**:
   - Telemetry and MQTT status entities explicitly distinguish between:
     - `requested_power_w`: What the remote operator or strategy requested.
     - `authorized_power_w`: What the safety arbiter permitted after applying constraints.
     - `actuated_power_w`: What was successfully transmitted to the inverter.
     - `observed_power_w`: What the physical battery telemetry actually reads back.

## Consequences
- No catastrophic single point of failure: Home Assistant can reboot or update at any time without impacting domestic energy balancing.
- No zombie states: overrides always expire safely.
- Clear auditability of why a commanded setpoint differed from physical reality.
