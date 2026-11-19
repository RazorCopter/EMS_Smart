# Afore Autonomous EMS — Traceability Requirements Matrix (Milestone M1)

Date: 2026-11-13  
Status: ACTIVE TRACEABILITY  
Covers: Historic specifications (V4 Roadmap, Security Audit, Register Provenance) and OpenEMS Adaptation Plan

---

## 1. Requirement Inventory & Traceability Table

| Req ID | Category | Requirement Statement | Implementing Component | Verification / Test |
|---|---|---|---|---|
| **REQ-EMS-01** | Safety | Zero dynamic write paths into EEPROM registers `0x0BC1` and `0x0BC2`. | `src/drivers/modbus_profile/` & `src/transport/readonly_guard.cpp` | `heliograph-tests`, `test-afore-telemetry` |
| **REQ-EMS-02** | Physical Sign | Standard sign convention: Grid Import > 0, Export < 0; Battery Charge > 0, Discharge < 0; Solar PV >= 0. | `src/ems/power_balance_evaluator.cpp` | `test-power-balance` (24/24 assertions) |
| **REQ-EMS-03** | Core Balancing | Calculate net house load and set battery charge/discharge to balance grid exchange to 0 W. | `src/ems/` controllers | `test-shadow-ems`, `test-power-balance` |
| **REQ-EMS-04** | Deadband & Ramp | Prevent oscillation with ±20 W deadband; limit battery setpoint changes to 250 W/s slew rate. | `src/ems/` constraint pipeline | `test-shadow-ems`, `test-endurance-7d` |
| **REQ-EMS-05** | SOC Floor Reserve | Battery discharge forbidden below configurable reserve threshold (default 15%). | `src/ems/` constraint pipeline | `test-target-aware-ems`, `test-endurance-7d` |
| **REQ-EMS-06** | Maintenance Cycle | Scheduled full-charge (100% SOC) and cell balancing cycle with 6h max timeout. | `src/ems/` maintenance manager | `test-target-aware-ems` |
| **REQ-EMS-07** | Local Autonomy | Device must continue autonomous balancing and battery protection when MQTT, HA, or Internet fails. | Common C++ core supervisor | `test-ha-replay`, `test-shadow-ems` |
| **REQ-EMS-08** | Lease-based Override| Remote control commands must carry an explicit lease duration (TTL); auto-reverts on timeout. | Operation lease manager | `test-target-aware-ems` |
| **REQ-EMS-09** | Serial Exclusivity | Exactly one process/thread may open and communicate over `/dev/ttyUSB0`. | `src/transport/posix_serial_transport.cpp` | `test-posix-serial` |
| **REQ-EMS-10** | Embedded Parity | Zero heap allocations on the 1 Hz fast control path; compiles under both Linux aarch64 and ESP32-S3. | C++17 Common Core library | `test-waveshare-hal`, desktop CMake & PlatformIO build |
| **REQ-EMS-11** | Bounded Optimizer | Solar deferral and TOU tariff dynamic scheduler runs in < 50ms with statically bounded memory (< 64KB). | `src/ems/` planner | `test-endurance-7d` |
| **REQ-EMS-12** | Secret Redaction | Secrets (passwords, tokens) are redacted in configuration dumps and serial/REST logs. | `src/config/` & web REST API | `test-secure-provisioning`, `test-host-config` |
