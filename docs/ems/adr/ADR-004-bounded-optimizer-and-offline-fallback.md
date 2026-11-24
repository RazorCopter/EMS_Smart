# ADR-004: Bounded Dynamic Optimizer, Time Representation, and Offline Fallback

## Context
Complex energy management systems sometimes rely on heavy linear programming solvers (e.g. GLPK, Coin-OR) or continuous web requests to calculate charging plans. In an embedded context with limited RAM (512 MB host, 512 KB ESP32), solvers with variable memory allocations or long non-deterministic execution times risk watchdog resets or out-of-memory panics. Furthermore, cloud connections, Home Assistant, and internet weather APIs frequently experience interruptions.

## Decision
1. **Bounded Discrete Time Representation**:
   - The planning horizon is divided into discrete slots (15-minute standard slots, or 60-minute coarse slots), covering up to 48 hours.
   - Total number of slots is bounded to a maximum of $N_{max} = 192$ slots.
   - All timestamps and slot intervals are represented in **UTC seconds since epoch** with an explicit slot duration (`duration_s`).
   - DST (Daylight Saving Time) transitions do not cause slot count assumptions: the system counts actual elapsed UTC duration, avoiding the classic bug of assuming exactly 96 slots per calendar day (which is false on clock-change days: 92 or 100 slots).
2. **Deterministic Bounded Optimizer**:
   - We utilize a lightweight dynamic programming / greedy forward-backward scheduler with discrete SOC stages ($0 \dots 100\%$, step 5% or 2%).
   - Memory footprint is statically allocated ($< 64\text{ KB}$ scratch buffer).
   - Worst-case execution time is strictly bounded to $< 50\text{ ms}$ on Linux and $< 100\text{ ms}$ on ESP32-S3.
3. **Comprehensive Cost Function**:
   $$C = \sum_{t} \Big[ P_{grid\_import}(t) \cdot \text{price}_{import}(t) - P_{grid\_export}(t) \cdot \text{price}_{export}(t) + |P_{battery}(t)| \cdot \text{cost}_{cycle\_wear} \Big] \cdot \Delta t$$
   - Correctly handles negative electricity prices (where importing electricity has negative cost or exporting incurs a penalty).
   - Considers battery round-trip efficiency ($\eta_{rt} \approx 90\%$).
4. **Autonomous Offline Fallback**:
   - Every smart input (solar forecast, spot prices, load forecast) carries an arrival timestamp and an explicit Time-To-Live (TTL).
   - If solar forecast is older than 3 hours or unavailable: the EMS immediately suspends solar deferral and reverts to standard greedy Self-Use.
   - If electricity tariff is older than 24 hours or unavailable: the EMS suspends tariff arbitrage and reverts to standard local Self-Use.
   - Loss of Home Assistant, MQTT broker, or Internet NEVER disables battery charging or self-consumption balancing.

## Consequences
- Guaranteed real-time performance and bounded memory consumption.
- Zero risk of system hang or watchdog trip during tariff calculations.
- Full local resilience against external network and server failures.
