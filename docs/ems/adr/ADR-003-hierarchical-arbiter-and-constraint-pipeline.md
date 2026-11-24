# ADR-003: Hierarchical Arbiter and Sequential Constraint Pipeline

## Context
An energy manager must reconcile multiple competing controllers and goals:
1. Low battery protection (preventing permanent cell damage).
2. BMS hardware limits (charge/discharge current and thermal limits).
3. Scheduled maintenance operations (cell balancing at 100% SOC).
4. Time-of-Use price optimization (arbitrage or grid pre-charge).
5. Solar forecast deferral (preventing feed-in limitation clipping).
6. Basic real-time Self-Use balance (matching household load to minimize grid exchange).

Without a strictly defined priority hierarchy, controllers would conflict, producing setpoint chatter, oscillating power commands, or safety violations.

## Decision
We adopt a sequential, deterministic two-phase decision pipeline:
1. **Phase 1: Intent Proposal (Controller Selection)**:
   Controllers propose an intended active battery power setpoint $P_{proposed}$ and a justification code, evaluated in priority order:
   - **Level 1 (Critical Emergency)**: Critical Low SOC rescue ($SOC < 10\% \to P_{proposed} = +500\text{ W}$ grid trickle charge).
   - **Level 2 (Maintenance / Calibration)**: Cell balancing cycle ($SOC \to 100\%$ hold).
   - **Level 3 (Time-bounded Manual Override)**: Active Home Assistant or GUI operator lease.
   - **Level 4 (TOU Price Optimization)**: Economic charge/delay schedule if active.
   - **Level 5 (Solar Deferral / Optimized Charge)**: Solar peak shaving headroom hold.
   - **Level 6 (Default Baseline)**: Self-Use household power balance.

2. **Phase 2: Hard Physical Constraint Pipeline**:
   The proposed intent $P_{proposed}$ is sequentially clipped by hard physical boundaries:
   - **Constraint A (SOC Reserve Floor)**: If $SOC \le SOC_{reserve}$, discharge is clamped to $0\text{ W}$ ($P_{intent} = \max(0, P_{proposed})$).
   - **Constraint B (BMS Capabilities)**: Clamped to $[ -P_{discharge\_max}(t), +P_{charge\_max}(t) ]$.
   - **Constraint C (Grid Import/Export Rules)**: If grid charging is disabled by user configuration, grid-sourced charge power is curtailed.
   - **Constraint D (Inverter AC Rating)**: Total battery + PV throughput cannot exceed inverter apparent power limit ($5000\text{ VA}$).
   - **Constraint E (Ramp Rate Limiting)**: Change in setpoint between consecutive 1-second cycles cannot exceed slew rate $\Delta P_{max} = \text{ramp\_w\_per\_s} \times \Delta t$ (e.g. $250\text{ W/s}$).
   - **Constraint F (Deadband Filter)**: Small residual changes ($|\Delta P| < 20\text{ W}$) are suppressed to eliminate jitter.

3. **Decision Artifact (`EmsDecision`)**:
   Every cycle outputs an immutable snapshot detailing:
   - Timestamp and unique decision sequence ID.
   - Chosen strategy and primary justification reason.
   - Initial proposed power $P_{proposed}$.
   - Intervening active constraints (bitmask / list of clipped constraints).
   - Final authorized setpoint $P_{final}$.
   - Authority source (e.g. `LOCAL_AUTONOMOUS`, `SCHEDULED_MAINTENANCE`, `MQTT_OVERRIDE`).

## Consequences
- Deterministic behavior: identical input telemetry and configuration produce bit-for-bit identical decisions.
- Complete explainability: every Watt sent (or rejected) carries a machine-readable reason code.
- Hard safety constraints can never be overridden by higher-level software or remote commands.
