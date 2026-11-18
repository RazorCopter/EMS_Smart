# Mission 3 EMS core

## Starting point at 73b0ad5

| Strategy | Inputs and algorithm | Entry, exit and fallback | Output, constraints and evidence | Remaining at baseline |
|---|---|---|---|---|
| SELF_USE | SOC, grid and battery with freshness and power skew. Battery W minus grid W. | Fixed deployed strategy. Invalid inputs or halted reader select SAFE_MODE; good data restores it. | Signed simulated W with 20–95% SOC limits, 1 kW limits, 20 W deadband, 250 W/s ramp and final clamp. Host tests and live audit exist. | Configurable policy, explanatory limits and extended observation. |
| CHARGE_FIRST | Same telemetry; takes positive balancing surplus. | Host-only invocation. Invalid inputs select SAFE_MODE. | Charge-only candidate, no grid charge; immediate caps and recheck after filtering. Host test covers cap. | Target, stage near target, expiry, policy and controlled return. |
| NATIVE_INVERTER | Requires usable telemetry through the shared initial gate. | Host-only request; invalid telemetry selects SAFE_MODE. | No setpoint. Host test asserts absent output. | Persistent selection and board replay. |
| SAFE_MODE | Missing, stale, halted or skewed essential measurements, or explicit request. | Immediate entry; valid inputs allow recovery under configured strategy. | No candidate or setpoint. Host tests cover stale and halt plus boot logs. | Configurable freshness and structured transition context. |

## Controls

Authenticated GET /api/v1/ems/config shows persisted parameters. POST accepts a bounded form such as `strategy=0&soc_min=20&soc_target=80`. Strategy IDs 0–5 are SELF_USE, CHARGE_FIRST, NATIVE_INVERTER, SAFE_MODE, TARGET_SOC and MAINTENANCE. Return strategy permits 0, 2 or 3. A successful configuration starts a new local strategy session. Deadlines are relative seconds from session start. A reboot starts a new session based on the persisted policy. Battery capacity and efficiency are provisional.

Authenticated GET /api/v1/ems/replay runs the production decision logic with deterministic synthetic inputs and fresh local manager instances. It has no effect on the live policy, transport or GPIO. Measured decisions in /api/v1/logs carry source MEASURED; the load is AC-derived. The decision sequence plus uptime identify the cycle within each boot.

| Strategy | Mission 3 behavior |
|---|---|
| SELF_USE | Grid cancellation model, safety range, deadband/ramp and final clamp. |
| CHARGE_FIRST | Configurable target, max power, surplus or explicit grid allowance, near-target taper and deadline. Returns to SELF_USE on target or timeout. |
| TARGET_SOC | Estimated input energy deficit and required mean power to deadline. Marks infeasible under the currently available power; returns to configured fallback at target or timeout. |
| MAINTENANCE | Same planner with a hold reservation, hysteretic hold, completion, fault abort and fallback. No automatic calibration. |
| NATIVE_INVERTER | Observes valid inputs and emits null setpoints. |
| SAFE_MODE | Emits null setpoints on bad essentials or an explicit safe choice. |
