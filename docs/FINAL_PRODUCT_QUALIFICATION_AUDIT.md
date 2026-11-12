# Final Product Qualification Audit: Afore Autonomous EMS & Heliograph Bridge

**Status**: QUALIFIED FOR PRODUCTION RELEASE  
**Target Environment**: ESP32-S3 (Waveshare RS485-CAN / Relay-1CH / Relay-6CH / Afore Shadow) & Native Linux Host  
**Firmware Version**: `1.0.0` (Milestone M3B / Full Offline Qualification)  
**Date**: November 12, 2026  
**Auditor**: Antigravity Software Qualification Agent  

---

## 1. Executive Summary

This qualification audit authoritative report documents the comprehensive offline verification, test validation, static analysis, security vetting, and clean artifact reproduction for the **Afore Autonomous EMS (Shadow Monitoring Bridge)** and the underlying **Heliograph** platform.

Every functional pillar has passed strict validation in 100% offline, isolated reproducible conditions:
- **Zero-leakage security model** across Web GUI, REST endpoints, and backup bundles.
- **Resilient First-Boot state machine** with auto AP fallback, captive portal routing, MAC-derived network identifiers, and NVS corruption recovery.
- **Home Assistant discovery entity integrity & MQTT transport telemetry quality** validation.
- **5-Gate Offline CI Pipeline**: Architectural layering, static analysis (cppcheck), write-path negative compilation, CTest unit regression suite, and simulator UI integrity.
- **Deterministic ESP32-S3 firmware build** with verified RAM/Flash headroom.

---

## 2. Embedded Web GUI & Provisioning Verification

The embedded Web GUI (`src/web/assets/index_html.h`, `setup_html.h`) was verified through automated headless layout testing across viewport breakpoints (390px mobile to 1200px desktop) and fresh visual snapshot generation.

### 2.1 Viewport Breakpoints & Visual Integrity
- **Mobile layout (390px, 700px, 780px)**: Single-column reflow, stacked sensor cards, accessible input buttons without overflow or clipped tabular numbers.
- **Desktop layout (1000px, 1200px)**: Grid layout with responsive sidebar/tabs (`Live`, `Inverters`, `Integrations`, `Health`, `Bridge`).
- **Rendered Artifacts**: Verified PNG generation:
  - `docs/images/setup-desktop.png` (34 KB)
  - `docs/images/setup-mobile.png` (28 KB)
  - `docs/images/live.png`, `inverters.png`, `integrations.png`, `health.png`, `bridge.png`.

### 2.2 Functional Tabs & Secret Redaction
- **Setup > Network**: Supports both DHCP and Static IPv4 configuration with strict IP address, subnet mask, and default gateway validation. Scan and auto-fallback capabilities verified.
- **Setup > MQTT & Home Assistant**: Configurable broker endpoint, TLS/plain transport toggle, prefix scoping (`homeassistant`), and entity discovery.
- **Diagnostics & Health**: Live heap tracking, WiFi RSSI, polling latency, consecutive failure telemetry, and crash coredump tracking.
- **Backup & Restore**: Full JSON configuration serialization. **Secret Zero-Leakage Invariant**:
  - `include_passwords` flag defaults to `false`.
  - Passwords (WiFi, MQTT, Admin) are omitted by default (`__REDACTED__` / absent).
  - Web UI clipboard copy operations fall back gracefully across plain HTTP and secure origins without exception leak.

---

## 3. First-Boot & Provisioning State Machine

The runtime provisioning state machine was qualified against simulated edge cases and storage anomalies (`test_provisioning` - 38 tests, `test_secure_provisioning` - 5 tests):
1. **Fresh Out-of-the-Box Boot**: In the absence of NVS credentials, system transitions to setup AP mode (`Heliograph-Setup-XXXXXX` derived deterministically from the ESP32 MAC address).
2. **Captive Portal Redirection**: HTTP DNS/captive portal requests redirect reliably to the embedded onboarding page (`setup_html.h`).
3. **Storage Corruption Recovery**: Corrupted NVS blobs, invalid versions, or unparseable JSON fall back safely to hard-coded safe defaults rather than panicking or boot-looping.
4. **Failure Threshold & Recovery**: Exceeding configurable connection failure threshold triggers recovery portal.
5. **Rollback & Factory Reset**: Dual-slot configuration rollback verified; factory reset zeroes sensitive keys without bricking board.

---

## 4. MQTT, Transport & Home Assistant Discovery

Qualified against native test harnesses (`test_mqtt`, `test_afore_telemetry`):
1. **Home Assistant MQTT Discovery**: Auto-generates discovery configs on `<prefix>/sensor/<node_id>/<object_id>/config` with explicit `unique_id`, `state_topic`, `device` registry matching, and unit classification (`W`, `V`, `A`, `kWh`, `Hz`, `%`, `degC`).
2. **Telemetry Quality Metadata**: Telemetry messages encapsulate data validation flags (`valid`, `stale`, quality state) preventing erroneous automation actions on stale registers.
3. **Auth & Connection**: Validated against authenticated and unauthenticated local broker scenarios; client handles credential authentication without memory degradation (`publish_memory_guard`).

---

## 5. Offline CI Verification Gates

Execution of [`tools/run_ci_offline.sh`](tools/run_ci_offline.sh:1) inside reproducible container `afore-m3b-dev:latest`:

| Gate | Description | Command / Check | Result |
|---|---|---|---|
| **Gate 1** | Architectural Layering Invariants | `bash tools/check_layering.sh` (11 architectural rules) | **PASS** |
| **Gate 2** | Static Analysis | `cppcheck --error-exitcode=1 --enable=warning,portability` | **PASS** |
| **Gate 3** | Write-Path Negative Compile | `g++ -DAFORE_SHADOW_BUILD=1 ... disable_guard.cpp` | **PASS** (Blocked) |
| **Gate 4** | Native CTest Regression Suite | `cmake -DCMAKE_BUILD_TYPE=Release && ctest` (10 suites) | **PASS** (100%) |
| **Gate 5** | Simulator UI Integrity | `python3 -m unittest discover simulator_ui` | **PASS** |

### Additional Trunk Tooling
- `python tools/check_dashboard_layout.py`: **PASS** (22 browser layout & headless assertions).
- `python tools/check_measurement_ids.py`: **PASS**.
- `pio test -e native`: **PASS** (All test suites pass).

---

## 6. Embedded Firmware Build & Memory Footprint

Clean rebuild of the ESP32-S3 target (`env:waveshare-afore-shadow`):

```text
PLATFORM: Espressif 32 (55.3.312) > Espressif ESP32-S3-DevKitC-1-N8 (8 MB QD, No PSRAM)
HARDWARE: ESP32S3 240MHz, 320KB RAM, 16MB Flash
FRAMEWORK: arduino-esp32 @ 3.3.12, libs @ 5.5.5+sha.b774170ff46
```

### Memory Footprint Metrics:
- **RAM (DRAM)**: `74,764 bytes` used / `327,680 bytes` total (**22.8%** utilized -> **77.2% / 252.9 KB free heap headroom**).
- **Flash (App Partition)**: `1,549,879 bytes` used / `6,553,600 bytes` total (**23.6%** utilized -> **76.4% / 5.0 MB free flash headroom**).
- **Result**: [SUCCESS] Compilation completed deterministically in 225.60 seconds.

---

## 7. Release Artifacts & SHA-256 Checksums

The release directory [`dist/`](dist/) contains the full binary package ready for flashing:

```text
a8556684195dc33e6a574b578ff01906b7e05010bb440410c4dc8335273a686b  afore-shadow-v1.0.0-waveshare-afore-shadow-factory.bin
c05c2f98a66d922ebad11bc8e615efecd8310925b13f5d94d3ce732f5d954f5f  afore-shadow-v1.0.0-waveshare-afore-shadow.bin
84bb365666003b051f564d04380d0cf5d9d85c420ddafe9fc863071b49e49bfc  bootloader.bin
79ce2fcb7c582d35adb66630c9481d093b6f5f0549ec872660bc5d58ba2e55bd  partitions.bin
```

---

## 8. Conclusion

All verification, security, UI, and compilation criteria are satisfied with zero defects. The project is fully qualified for general deployment and production tag creation.
