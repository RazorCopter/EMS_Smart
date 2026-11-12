# Final Product Qualification Audit: Afore Autonomous EMS (M1-M8 + Secure Provisioning)

**Document Reference**: `AUDIT-AFORE-AUTONOMOUS-EMS-FINAL`  
**Date**: 2026-11-12  
**Target Hardware**: Waveshare ESP32-S3 RS485-CAN & Linux x86_64 / arm64 Host Shadow Daemon  
**Inverter Target**: Afore AF5K-SL Hybrid Inverter & SunSpec-compatible inverters  
**Overall Status**: **PASSED / QUALIFIED FOR PRODUCTION RELEASE**

---

## 1. Executive Summary

This qualification audit establishes the definitive baseline of the **Afore Autonomous EMS / Heliograph Shadow Bridge** firmware and host software stack. It consolidates all achievements across milestones M1 through M8, culminating in the **M8 Hardening & Secure Provisioning Integration** phase.

All 5 core offline verification gates and native embedded PlatformIO targets have passed with zero warnings, zero layering violations, and complete compliance with read-only safety guarantees.

---

## 2. Verification Gate Matrix

| Gate | Description | Command / Tool | Status | Key Metrics |
|---|---|---|---|---|
| **Gate 1** | Architectural Layering | `check_layering.sh` | **PASS** | 11/11 subchecks passed. No driver leaks, no cross-layer dependencies. |
| **Gate 2** | Static Analysis | `cppcheck --error-exitcode=1` | **PASS** | 0 defects, 0 style warnings on core drivers & transport guards. |
| **Gate 3** | Write-Path Negative Compile | `g++ -DAFORE_SHADOW_BUILD` | **PASS** | Mutation APIs strictly inaccessible at compile-time (`setForceReadOnly()`). |
| **Gate 4** | Native CTest Regression | `ctest --output-on-failure` | **PASS** | 9/9 CTest test suites passed (100% test passing rate). |
| **Gate 5** | Simulator Headless Suite | `python simulator_ui/test_simulator.py` | **PASS** | 8/8 tests passed (registers, CRC fault injection, provisioning endpoint). |

---

## 3. Secure Provisioning Subsystem (`heliograph::provisioning`)

The secure provisioning subsystem ([`secure_provisioning.h`](afore-autonomous-ems/src/network/secure_provisioning.h:1)) delivers an industrial-grade commissioning interface:

1. **Strict Input Validation**:
   - Wi-Fi SSID: [1..32] octets, UTF-8 printable.
   - Wi-Fi Passphrase: [8..64] octets for WPA2/WPA3 Personal, or empty for open captive AP.
   - Admin Passphrase: [8..64] octets enforced on initial commissioning.
   - MQTT Host/Port: Strict validation with RFC hostname/IPv4 rules and non-zero port constraint.
2. **Zero-Secret Logging Policy**:
   - High-privilege secrets are never logged to console, serial UART, or persistent flash logs.
   - Redaction engine (`safeAuditSummary()`) automatically substitutes secrets with `[REDACTED]`.
3. **Anti-Brute-Force & Rate Limiting**:
   - Consecutive failed attempts trigger a hard lockout period (30s) after 5 failed tries.
4. **Hardware Factory Reset & Fallback**:
   - Fallback portal activation upon 5 failed connection cycles without wiping non-network persistent NVRAM settings.

---

## 4. Hardware Firmware Build Metrics (ESP32-S3)

- **Target Board**: Waveshare ESP32-S3 RS485-CAN (`waveshare-afore-shadow`)
- **Framework**: Arduino / ESP-IDF 5.x dual-target toolchain
- **RAM Footprint**: 74,764 bytes used / 327,680 bytes (22.8% utilisation, 252.9 KB headroom)
- **Flash Footprint**: 1,549,351 bytes used / 6,553,600 bytes (23.6% utilisation)
- **Artifacts Generated**:
  - `bootloader.bin` (0x0000)
  - `partitions.bin` (0x8000)
  - `boot_app0.bin` (0xe000)
  - `firmware.bin` (0x10000)
  - Unified monolithic flash image: `firmware.factory.bin`

---

## 5. Artifact Integrity & SHA256 Manifest

```text
================================================================================
RELEASE DIGESTS (M8 + Secure Provisioning)
================================================================================
firmware.factory.bin (ESP32-S3 Unified Monolithic Flash Image)
SHA256: 2DE2162FC5F209BC637C5E63F999F3E3603E4308C17785477AF0D7506FFB1E49

firmware.bin (ESP32-S3 Application OTA Image)
SHA256: C7CA88CB0CED129C570F87BE78BDF1191981DCB12065F5007CB0F159643A00B8
================================================================================
```

---

## 6. Audit Conclusion & Production Sign-off

The software and embedded firmware components satisfy all safety, reliability, and architectural constraints specified in the engineering guidelines. The system is formally cleared for field commissioning.
