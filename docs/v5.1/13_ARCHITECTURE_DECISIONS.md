# Architecture decisions

These records are accepted design choices for v5.1. Implementation and physical qualification follow the gate plan.

| ADR | Decision and rationale | Consequence and verification |
|---|---|---|
| ADR-V51-01 | Use the Waveshare ESP32-S3-Relay-1CH integrated-antenna unit as sole production Modbus owner and runtime. Existing deployment material is archival. | Desktop builds and replay develop shared pure code; G1/G4 prove board and ownership. HA/container outages leave local decisions available. |
| ADR-V51-02 | Keep PlatformIO with pinned pioarduino/Arduino over ESP-IDF and C++17. Begin with standalone USB diagnostics. This reuses supported HAL and build patterns. | Dedicated source allowlist and audit ELF gate precede any flash. Keep SDK and standalone ESP-IDF migration independent. |
| ADR-V51-03 | Seal SHADOW at build time with typed reads and no-I/O executor. Legacy firmware targets retain their existing role outside the v5.1 composition. | Proof combines compile graph, negative tests, ELF/call edges and observed bus. Future LIVE belongs to G11 and a separate build. |
| ADR-V51-04 | Use one versioned FC04 profile decoder, canonical signs and source-aware Energy State. Clock age uses monotonic time; wall time is metadata and for schedules. | Older offsets or FC03 draft can supply fixtures only. Missing or stale stays explicit. G5 verifies signs and quality. |
| ADR-V51-05 | Assign one scheduler, one Arbiter, hard Safety limits and one Shadow executor. Treat Wi-Fi/MQTT/HA inputs as bounded advice. | Leases expire, local safe mode survives loss, and every accepted or vetoed intent is audited. Final bound check follows smoothing. |
| ADR-V51-06 | Bind AUX1 to an off-only board function with generic logical contract. Reuse pin knowledge while excluding DRM and motion paths. | G3 must measure reset behavior. Wire feedback remains null without a sensor; requests have explicit denial disposition. |
| ADR-V51-07 | Use versioned audit with boot-scoped ordering, nullable UTC and bounded RAM buffer. Transport and container share one record. | The container can reconstruct duplicates and gaps. Long outages can lose detail and this is measurable. |
| ADR-V51-08 | Independently implement OpenEMS behaviors in MIT C++ and keep algorithm boundaries explicit. | OpenEMS Edge/Backend EPL-2.0 and UI AGPL-3.0 have their own provenance and license obligations. Scope and order follow the [mapping](06_OPENEMS_MAPPING.md). |
| ADR-V51-09 | Use bounded solver horizons, bounded load and deterministic offline fallback. | G6/G10 verify run time, cap relevant queues and fall back when stale/deadline exceeded. |
| ADR-V51-10 | Atomically commit validated local settings with a previous generation. Expose read-only diagnostics and narrowly scoped authenticated settings. Keep advisory leases in RAM. | Legacy presets cannot enable control, and config recovery preserves safe defaults. G8 tests interruption and concurrency. |
| ADR-V51-11 | Gate qualification by proof and coverage, including independent electrical evidence. | Elapsed time, a build label or counters alone cannot certify this release. Keep associated evidence and exact binary hash. |

These decisions incorporate the earlier [OpenEMS assessment](../ems/openems-comparison.md) and its [original ADRs](../ems/adr/ADR-001-openems-adaptation-and-licensing.md). See [clean restart](08_CLEAN_RESTART_PLAN.md) for the historical milestone crosswalk.
