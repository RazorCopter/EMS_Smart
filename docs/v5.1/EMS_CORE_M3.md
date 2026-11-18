# Mission 3 EMS core

## Starting point at 73b0ad5

| Strategy | Inputs and algorithm | Entry, exit and fallback | Output, constraints and evidence | Remaining at baseline |
|---|---|---|---|---|
| SELF_USE | SOC, grid and battery with freshness and power skew. Battery W minus grid W. | Fixed deployed strategy. Invalid inputs or halted reader select SAFE_MODE; good data restores it. | Signed simulated W with 20–95% SOC limits, 1 kW limits, 20 W deadband, 250 W/s ramp and final clamp. Host tests and live audit exist. | Configurable policy, explanatory limits and extended observation. |
| CHARGE_FIRST | Same telemetry; takes positive balancing surplus. | Host-only invocation. Invalid inputs select SAFE_MODE. | Charge-only candidate, no grid charge; immediate caps and recheck after filtering. Host test covers cap. | Target, stage near target, expiry, policy and controlled return. |
| NATIVE_INVERTER | Requires usable telemetry through the shared initial gate. | Host-only request; invalid telemetry selects SAFE_MODE. | No setpoint. Host test asserts absent output. | Persistent selection and board replay. |
| SAFE_MODE | Missing, stale, halted or skewed essential measurements, or explicit request. | Immediate entry; valid inputs allow recovery under configured strategy. | No candidate or setpoint. Host tests cover stale and halt plus boot logs. | Configurable freshness and structured transition context. |

## Controls

Authenticated GET /api/v1/ems/config shows persisted parameters. POST accepts a bounded form such as `strategy=0&soc_min=20&soc_target=80`. Strategy IDs 0–5 are SELF_USE, CHARGE_FIRST, NATIVE_INVERTER, SAFE_MODE, TARGET_SOC and MAINTENANCE. Return strategy permits 0, 2 or 3. Starting a target or maintenance session inherits the previous stable strategy unless the request explicitly supplies a fallback. A successful configuration starts a new local strategy session. Deadlines are relative seconds from session start. A reboot starts a new session based on the persisted policy. Battery capacity and efficiency are provisional.

Authenticated GET /api/v1/ems/replay runs the production decision logic with deterministic synthetic inputs and fresh local manager instances. It has no effect on the live policy, transport or GPIO. Measured decisions in /api/v1/logs carry source MEASURED; the load is AC-derived. The decision sequence plus uptime identify the cycle within each boot.

| Strategy | Mission 3 behavior |
|---|---|
| SELF_USE | Grid cancellation model, safety range, deadband/ramp and final clamp. |
| CHARGE_FIRST | Configurable target, max power, surplus or explicit grid allowance, near-target taper and deadline. Returns to SELF_USE on target or timeout. |
| TARGET_SOC | Estimated input energy deficit and required mean power to deadline. Marks infeasible under the currently available power; returns to configured fallback at target or timeout. |
| MAINTENANCE | Same planner with a hold reservation, hysteretic hold, completion, fault abort and fallback. No automatic calibration. |
| NATIVE_INVERTER | Observes valid inputs and emits null setpoints. |
| SAFE_MODE | Emits null setpoints on bad essentials or an explicit safe choice. |

## Acceptance evidence

SELF_USE was observed for 1,804.64 s on v5.1-ems-core-1, with 341 field samples, maximum gap 5.58 s and no unexpected reset. No sample was outside Safety. The firmware reported 4,042 valid FC04 requests and zero errors at observation end. RSSI ranged from -69 to -55 dBm and free heap from 186,632 to 186,852 B.

| Signal | Measured or simulated interval |
|---|---|
| SOC | 20 to 22% |
| Grid | -21 to 1,556 W |
| Battery | -698 to 6 W |
| Derived load | 420 to 1,484 W |
| Candidate | -1,555 to -497 W |
| Final simulated | -700 to 0 W |

The increase in grid import coincided with the 20% SOC reserve and a drop in measured discharge. All 131 sampled Safety limits in this interval kept the simulated plan within the configured envelope. These are counterfactual setpoints alongside the inverter's native behavior. [Samples](evidence/M3_SELF_USE.csv)

The final v5.1-ems-core-2 release keeps the measured SELF_USE calculation unchanged and adds asymmetric Charge First taper hysteresis, maintenance fault abort on invalid/read-error inputs, and previous-strategy inheritance. Signed OTA confirmed app1 valid at 10.0.0.110, preserving the previous validated app0. A non-default deadband survived OTA and was restored after verification. The final board served all six replay strategies and demonstrated the maintenance completion transition. [ESP32 replay](evidence/M3_ESP32_REPLAY.json)

Host assertions passed on the final source. Final sampled state: RSSI -62 dBm, 186,852 B free heap, 8 MiB PSRAM, 59 valid FC04 requests and no errors. ELF did not link legacy write encoders or relay control. Live health and heartbeat reported executed=false, zero writes, AUX1 disabled and GPIO47 LOW. Final app SHA-256: 37e32e7775b5f9446aa0e9313fefbe4119c0cd2b2b8aec254be38a5463107d39.
