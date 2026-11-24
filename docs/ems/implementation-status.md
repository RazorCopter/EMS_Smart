# Afore Autonomous EMS — Implementation & Milestone Status Register

Project: Afore Autonomous Energy Management System  
Reference Architecture: OpenEMS Controller Paradigm  
Target Hardware: Raspberry Pi Zero 2 W (Testbed) / ESP32-S3 (Future Embedded Target)  
Current Target IP: `10.0.5.174:8080` (SSH alias: `afore-m3`)  
Last Updated: 2026-11-13  

---

## 1. Milestone Tracking Matrix

| Milestone | Description | Status | Evidence / Artifact |
|---|---|---|---|
| **M0** | Reconnaissance & Reproducible Baseline | **COMPLETED** | `docs/ems/baseline.md`, target audit, CTest inventory, RAM constraints identified (make -j1 required). |
| **M1** | Functional Specification, OpenEMS Matrix & ADRs | **IN PROGRESS** | `docs/ems/specification.md`, `docs/ems/openems-comparison.md`, `docs/ems/requirements.md`, ADR-001..005. |
| **M2** | C++ Telemetry Consolidation & Serial Decoder | PENDING | Unified driver/profile, exclusive POSIX serial, quality filter, raw frames capture. |
| **M3** | Common C++17 EMS Core & Supervisor | PENDING | Shared core library, dependency-free contracts, pure decision evaluation pipeline. |
| **M4** | Self-Use, SOC Reserve, Charge-First & Ramping | PENDING | Closed-loop balancing controller, ramping (W/s), deadband, shadow decisions. |
| **M5** | Target SOC Planning & Maintenance Workflow | PENDING | Deadline planner, cell equalization cycle, safe exit & longevity constraints. |
| **M6** | Solar PV Forecasts & Deferred Optimized Charging | PENDING | Smart input adapter, forecast aging, solar deferral to avoid peak sell limit. |
| **M7** | Time-of-Use (TOU) Tariff Optimization | PENDING | Bounded dynamic scheduler, negative price handling, cost minimization. |
| **M8** | Configuration REST API & Atomic Persistence | PENDING | Versioned schema, schema validation, optimistic locking, secret redaction. |
| **M9** | MQTT & Home Assistant Integration | PENDING | Auto-discovery, LWT, advisory override with leases/TTL, degraded offline mode. |
| **M10** | Comprehensive Web GUI (OpenEMS-styled) | PENDING | Reactive cards, strategy selector, parameters, decision justification, responsive UI. |
| **M11** | Definite Host Runtime & Migration from Bridge | PENDING | Single serial owner, native acquisition daemon, systemd unit, zero downtime rollback. |
| **M12** | ESP32-S3 Shadow Firmware Build | PENDING | Common C++ core ported to ESP32 platform, memory footprint, zero write invariants. |
| **M13** | Controller Actuator in Simulation | PENDING | FC10 S32 atomic payload generator, state machine (`DISARMED` -> `ACTIVE`), safety tests. |
| **M14** | Hardware Validation & Attended Control | PENDING | Explicit write authorization gate, physical step-by-step verification, rollback. |
| **M15** | Multi-Day Endurance & Production Release | PENDING | Continuous shadow run (>= 24-72h), memory stability, telemetry continuity. |

---

## 2. Active Environment Constraints & Invariants

1. **Physical Safety Rule**: External EMS write registers (`0x0A08`, `0x0A09-0x0A0A`) remain DISARMED until Milestone M14 with explicit user protocol approval.
2. **Wear Protection Rule**: EEPROM registers `0x0BC1` (Max Charge Current) and `0x0BC2` (Max Discharge Current) are PERMANENTLY FORBIDDEN from all dynamic write paths.
3. **Serial Exclusivity**: Exactly ONE process may own `/dev/ttyUSB0` at any time.
4. **Memory Constraint**: Raspberry Pi Zero 2 W has ~415 MB free RAM. Compilation must never exceed `make -j1` or `make -j2`.
5. **Architectural Parity**: Core control algorithms are implemented once in pure C++17 and compiled for both Linux host daemon and ESP32 firmware. No divergent Python logic for EMS decisions.
