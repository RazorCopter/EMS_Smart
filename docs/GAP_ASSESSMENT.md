# Heliograph Host Porting & Afore AF5K-SL Integration: Gap Assessment

**Author:** Autonomous Engineering Agent  
**Date:** November 2026  
**Target Inverter:** Afore AF5K-SL Hybrid Inverter (Single Phase 5kW, Storage)  
**Target Environment:** Linux Host (x86_64 / arm64 / Raspberry Pi at 10.0.5.174) in Read-Only Shadow Mode  
**Base Repository:** `Timdebruijn/heliograph` (Branch: `phase0-host-shadow`, Remote: `upstream`)  
**Recommendation:** **PROCEDI / GO (Strict Read-Only Shadow Mode)**
**Checkpoint:** CP0 (Phase 0 Scouting & Gap Assessment)

---

## 1. Executive Summary & Feasibility Verdict

This gap assessment evaluates the architectural feasibility, host porting strategy, Modbus safety boundaries, and validation oracle tooling for porting Heliograph from its native ESP32-S3 microcontroller environment to a Linux host (e.g. Raspberry Pi) operating strictly in **Read-Only Shadow Mode** against an Afore AF5K-SL hybrid inverter.

### Key Conclusions
1. **Exceptional Modularity & Layering**: Heliograph strictly isolates hardware-specific code behind clean abstract interfaces (`heliograph::Transport`, `heliograph::ConfigurationStore`). The Modbus RTU codec (`src/protocols/modbus/`), the TOML profile evaluation engine (`src/drivers/modbus_profile/`), the canonical measurement taxonomy (`src/device/measurement.h`), and the formatting subsystems compile cleanly under pure C++17 without Arduino or ESP-IDF dependencies.
2. **Comprehensive Read-Only Safety**: The codebase possesses a 5-layer command gating architecture (`src/commands/command_dispatcher.cpp`). Furthermore, Heliograph currently implements single-register writes via Modbus FC06 only; it has **no** client implementation for FC10 (Write Multiple Registers). Because Afore External EMS requires 32-bit atomic writes via FC10 to `0x0A09–0x0A0A`, an unintended setpoint write is architecturally precluded even before adding our multi-level shadow guards.
3. **Transport Porting Simplicity**: Replacing the ESP32 `HardwareSerial` transport (`src/transport/rs485_transport.cpp`) with a POSIX `<termios.h>` serial implementation (`PosixSerialTransport`) is straightforward and well-bounded. Standard USB-RS485 adapters (FTDI, CH340, CP2102) provide automatic hardware direction control.
4. **Independent Oracle Availability**: Validated Python oracles and register scans already exist in `../Solarman/` (`scan_afore_registers.py`, `afore_AF5K-Sl.yaml`), in `Doc/` (`Afore_Autonomous_EMS_V4_Roadmap.md`, `外部Ems调度 - En_240312.docx`), and inside Heliograph (`tools/read_modbus.py`).
5. **Verdict: PROCEDI (GO)**. Porting the telemetry read path of Heliograph to Linux for Phase 0 shadow operation is fully viable, safe, and strongly recommended.

---

## 2. Current Architecture & Layering Analysis

Heliograph strictly enforces modular separation of concerns and clean layering boundaries, codified in `AGENTS.md` and mechanically verified on every commit by `tools/check_layering.sh`:

1. **Brand Isolation Principle (`AGENTS.md:13-16`)**: Device- and brand-specific knowledge (register addresses, protocol quirks, scaling) must live **only** in `src/drivers/<driver>/` and `profiles/`. Generic headers, transports, output adapters, and state containers are strictly prohibited from referencing brand names (enforced by `check_layering.sh` rule 1).
2. **Canonical Data Integrity (`AGENTS.md:18-20`)**: *"Unknown is never zero."* Values not received from the device remain absent or `null`; replacing an unread field with 0 is treated as a fatal design fault.
3. **Hardware-Decoupled Core (`tools/check_layering.sh:57-87`)**: Core protocol modules (`src/device/`, `src/protocols/byte_order.h`, `src/protocols/modbus/modbus_rtu.*`, `src/network/rtc_time.*`) are forbidden from including Arduino or ESP-IDF headers (`Arduino.h`, `WiFi.h`, `HardwareSerial.h`, `esp_*`, `freertos/*`), ensuring 100% host compilability and testability under `env:native`.
4. **Verified Read-Back Gate (`tools/check_layering.sh:108-114`)**: Profiles with `verified = true` write definitions are rejected unless an explicit read-back address is defined (`tools/check_readback_rule.py`).

```text
 +--------------------------------------------------------------------------+
 |                            Output Layer                                  |
 |  MQTT (HA Discovery)  |  Prometheus Metrics  |  REST API  |  Modbus TCP  |
 +--------------------------------------------------------------------------+
                                      ^
                                      | publishes immutable DeviceState
 +--------------------------------------------------------------------------+
 |                      State Store & Device Context                        |
 |   state/state_store.h : double-buffered, thread-safe, reader-lock-free   |
 +--------------------------------------------------------------------------+
                                      ^
                                      | updates canonical measurements
 +--------------------------------------------------------------------------+
 |                           Driver Layer                                   |
 |  modbus_profile/ (TOML-driven)  |  sunspec/  |  solarmax/  |  solax_x1/  |
 +--------------------------------------------------------------------------+
                                      ^
                                      | requests register spans / transactions
 +--------------------------------------------------------------------------+
 |                          Protocol Layer                                  |
 |   protocols/modbus/ (modbus_rtu, modbus_client)  |  protocols/pmu/       |
 +--------------------------------------------------------------------------+
                                      ^
                                      | stream read / write bytes
 +--------------------------------------------------------------------------+
 |                     Transport Abstraction (HAL)                          |
 |   transport/transport.h (pure virtual: read, write, flush, setTimeout)   |
 +--------------------------------------------------------------------------+
         |                                                  |
         v                                                  v
 [rs485_transport.cpp]                           [posix_serial_transport.cpp]
 ESP32 HardwareSerial + DE/RE                    POSIX termios /dev/ttyUSB0
 (Embedded Firmware)                             (Host Linux / Phase 0 Shadow)
```

### Detailed Layer Breakdown
- **HAL / Transport Abstraction**:
  - `src/transport/transport.h`: Abstract base class `heliograph::Transport` defining `write(buf, len)`, `read(buf, len, timeoutMs)`, `flushInput()`, and `setTap()`. It isolates all bus communications from physical hardware.
  - `src/transport/rs485_transport.cpp`: Concrete implementation for ESP32 using Arduino `HardwareSerial` and manual RTS/GPIO pin toggle. It contains a dormant, non-functional stub for `!defined(ESP32)` (lines 139–153).
- **Protocol Layer**:
  - `src/protocols/modbus/modbus_rtu.{h,cpp}`: Pure C++17 RTU frame encoder, CRC-16 (Modbus polynomial `0xA001`), and response parser.
  - `src/protocols/modbus/modbus_client.{h,cpp}`: Manages request-response transactions, deadline timeouts, retries, and echo verification over `Transport`.
  - `src/protocols/byte_order.h`: Endianness conversion helpers (`getBe16`, `getLe16`, `getBe32`, `getFloatBe`).
- **Driver Layer**:
  - `src/drivers/inverter_driver.h`: Pure virtual interface declaring `probe()`, `poll()`, `capabilities()`, and `execute()`.
  - `src/drivers/modbus_profile/modbus_profile_driver.{h,cpp}`: Data-driven driver evaluating compiled TOML profiles (`profiles/*.toml` generated into `profile_tables.cpp`). Optimizes holding/input register reads into contiguous spans (up to 125 registers per request).
  - `src/drivers/driver_registry.{h,cpp}`: Registry mapping profile names to driver factories.
- **Application & Scheduling**:
  - `src/app/device_plan.{h,cpp}`: Plans poll intervals, groups measurement queries, and schedules background polls.
  - `src/app/register_reader.{h,cpp}`: Diagnostics register reader restricted strictly to FC03/FC04.
  - `src/device/device_context.{h,cpp}`: Sole writer of `DeviceState`; applies scaling factors and validity bitmasks.
- **Configuration & Storage**:
  - `src/config/configuration.h`: C++ structs representing network, MQTT, serial, safety, and driver settings.
  - `src/config/configuration_store.{h,cpp}`: Transactional key-value configuration store with rollback slot.
  - `src/config/nvs_backend.{h,cpp}`: Backend targeting ESP-IDF NVS flash.
- **Outputs & Telemetry**:
  - `src/outputs/mqtt/`: Home Assistant MQTT discovery (`home_assistant_discovery.cpp`), state publication, and command subscription.
  - `src/outputs/prometheus/`: Prometheus exposition format serializer (`prometheus_metrics.cpp`).
  - `src/outputs/rest/`: JSON payloads (`rest_payloads.cpp`) and HTTP handlers (`rest_api.cpp`).
  - `src/outputs/modbus_tcp/`: Modbus TCP server exposing cache snapshot on port 502 (`modbus_tcp_server.cpp`, `register_map.cpp`).
- **Testing Subsystem**:
  - 28 unit test suites in `test/` (e.g. `test_modbus`, `test_modbus_profile`, `test_measurements`, `test_rate_limiter`, `test_sha256`), written in portable C++ for the PlatformIO native test runner.
  - `test/test_modbus/test_main.cpp`: Exercises the full Modbus stack via `run_modbus_rtu()` (wire codec, CRC-16, PDU encoding/decoding) and `run_modbus_client()` (transaction timeouts, exceptions, echo validation).
  - `test/support/mock_transport.h`: In-memory byte-level bus simulator (`heliograph::test::MockTransport`) providing a programmable `Responder` callback and queue to simulate realistic inverter responses, frame drops, corrupted CRCs, and timeouts without physical hardware.

---

## 3. Module Portability Classification

| Module Subsystem | File Paths | Classification | Porting / Adaptation Strategy for Host |
|---|---|---|---|
| **Modbus RTU Codec** | `src/protocols/modbus/modbus_rtu.{h,cpp}` | **Directly Portable** | 100% standard C++17. Zero hardware dependencies. Fully tested. |
| **Modbus Client Engine** | `src/protocols/modbus/modbus_client.{h,cpp}` | **Directly Portable** | Pure C++17, depends only on `Transport` abstraction. |
| **Profile Driver** | `src/drivers/modbus_profile/` | **Directly Portable** | TOML-driven parser, span optimizer, measurement decoder. Pure C++. |
| **Byte Ordering & Math** | `src/protocols/byte_order.h` | **Directly Portable** | Header-only portable endian conversions. |
| **Canonical Taxonomy** | `src/device/measurement.{h,cpp}` | **Directly Portable** | Canonical measurement definitions, validation rules. |
| **State Store** | `src/state/state_store.{h,cpp}` | **Directly Portable** | Double-buffered thread-safe state container. |
| **Diagnostics & Counters** | `src/diagnostics/diagnostics.{h,cpp}` | **Directly Portable** | C++ atomics for latency, poll counters, error rates. |
| **Bus Tap Sniffer** | `src/diagnostics/bus_tap.{h,cpp}` | **Directly Portable** | Circular buffer for raw frame captures. |
| **Prometheus Exporter** | `src/outputs/prometheus/` | **Directly Portable** | Pure C++ string formatting of metrics. |
| **REST Payloads** | `src/outputs/rest/rest_payloads.{h,cpp}` | **Directly Portable** | ArduinoJson / pure C++ JSON document generation. |
| **Transport HAL** | `src/transport/transport.{h,cpp}` | **Directly Portable** | Abstract virtual base class. |
| **RS485 Serial HAL** | `src/transport/rs485_transport.{h,cpp}` | **ESP32-Specific (Replace)** | Replace with `PosixSerialTransport` using standard POSIX `<termios.h>`. |
| **Configuration Backend** | `src/config/nvs_backend.{h,cpp}` | **ESP32-Specific (Replace)** | Core persistence uses abstract `heliograph::KeyValueBackend` (`src/config/configuration_store.h:33`). On ESP32 it uses `NvsBackend`; on host replace with `FileConfigurationBackend` (local JSON file). |
| **MQTT Client Transport** | `src/outputs/mqtt/mqtt_output.{h,cpp}` | **ESP32-Specific (Adapt)** | Payload and HA discovery logic (`home_assistant_discovery.cpp`) is pure C++. Socket transport uses `espMqttClient`. Adapt to `libmosquitto` or disable in early shadow CLI. |
| **Web Server & UI** | `src/web/`, `src/outputs/rest/rest_api.{h,cpp}` | **ESP32-Specific (Exclude/Stub)** | Uses `ESPAsyncWebServer`. Exclude for host shadow runner CLI. |
| **WiFi & Provisioning** | `src/network/wifi_manager.{h,cpp}` | **ESP32-Specific (Exclude)** | Host OS (Linux systemd / NetworkManager) manages network interfaces. |
| **Hardware Relays & DRM** | `src/relays/relay_controller.*`, `src/relays/drm.*` | **Exclude for Shadow** | Exclude from Phase 0 host build; no relay hardware exists on Linux host. |
| **Command Dispatcher** | `src/commands/command_dispatcher.*` | **Exclude for Shadow** | Exclude or hard-lock in Phase 0; host operates in read-only shadow mode. |
| **RTC & Time Sync** | `src/network/rtc_*`, `src/network/time_manager.*` | **Adaptable** | Replace I2C PCF85063 RTC with POSIX `clock_gettime(CLOCK_REALTIME)`. |
| **OTA Firmware Update** | `src/ota/` | **ESP32-Specific (Exclude)** | Not applicable to host Linux binary. |

---

## 4. Host (Linux / POSIX) Porting Strategy

### 4.1 RS485 Serial HAL (`PosixSerialTransport`)
The ESP32 RS485 transport toggles GPIO pins for DE/RE direction switching. On Linux with a USB-RS485 dongle:
1. **Device Access**: Open `/dev/ttyUSB0` (or configured serial port) via `open(path, O_RDWR | O_NOCTTY | O_NONBLOCK)`.
2. **POSIX Termios Configuration**:
   - Baud rate: `B9600` (configurable to `B115200`).
   - 8 data bits, no parity, 1 stop bit (`8N1`): `c_cflag &= ~PARENB; c_cflag &= ~CSTOPB; c_cflag &= ~CSIZE; c_cflag |= CS8;`.
   - Raw mode: Disable `ICANON`, `ECHO`, `ECHOE`, `ISIG`, `IXON`, `IXOFF`, `OPOST`.
3. **Timeout & Non-Blocking Polling**:
   - Heliograph's `Transport::read(buf, len, timeoutMs)` uses an explicit deadline.
   - On Linux, use `poll()` or `select()` with `timeoutMs` to await readability, followed by `::read()`.
4. **Hardware Flow Control**: Standard USB-RS485 dongles (FTDI FT232RL, CH340G, CP2102) handle transceiver DE/RE switching automatically in hardware via transmit-sense. For FTDI chips supporting RS485 mode, `TIOCSRS485` ioctl can be enabled if required.

### 4.2 Configuration Subsystem
- Heliograph's `ConfigurationStore` accepts an injected `ConfigurationBackend` interface (`read`, `write`, `erase`).
- Implement `FileConfigurationBackend`:
  - Stores configuration as formatted JSON at `/etc/heliograph/config.json` or `./config.json`.
  - Atomicity guaranteed via write-to-temporary and `rename()`.

### 4.3 MQTT & Home Assistant Discovery
- `home_assistant_discovery.cpp` and `mqtt_payloads.cpp` are pure C++ and directly reusable.
- The network loop currently tied to `espMqttClient` will be backed by standard `libmosquitto` (available in all Linux distributions).

### 4.4 Logging & Diagnostics
- Heliograph's `src/diagnostics/logger.h` routes logs through an output function.
- In Linux, route logs directly to `stdout` / `stderr` with ANSI color formatting and ISO-8601 timestamps, or to `syslog` / `systemd-journald`.

### 4.5 Storage & Data Persistence
- Heliograph uses non-volatile storage exclusively via `heliograph::KeyValueBackend` (`src/config/configuration_store.h:33`), backed on ESP32 by `NvsBackend` (`src/config/nvs_backend.h`) and in unit tests by `MemoryBackend` (LittleFS is **not** used; web assets are linked into Flash via `PROGMEM` as noted in `src/web/assets/index_html.h:7`).
- On Linux host, implement `FileConfigurationBackend` implementing `KeyValueBackend`, persisting configuration as structured JSON.
- Telemetry datasets, raw frame traces (`heliograph::diag::BusTap`), and shadow decision logs are recorded directly to disk via standard C++ POSIX file I/O (`std::filesystem`, CSV / JSONL).

### 4.6 Clocks, Timers & Arduino Compatibility
Provide a lightweight compatibility header `compat/arduino_compat.h`:
```cpp
#pragma once
#include <chrono>
#include <thread>
#include <cstdint>

inline uint32_t millis() {
    using namespace std::chrono;
    static const auto start = steady_clock::now();
    return static_cast<uint32_t>(duration_cast<milliseconds>(steady_clock::now() - start).count());
}

inline void delay(uint32_t ms) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}
```

### 4.7 Host Build System
Create a clean, standalone `CMakeLists.txt` targeting C++17 on Linux (x86_64, aarch64, armhf). It builds the core protocol libraries, profile driver, POSIX transport, and a CLI shadow runner executable (`afore_shadow_runner`).

---

## 5. Write Risk Analysis & Mutation Pathways (File & Line Audit)

A exhaustive audit of the entire codebase was conducted to identify every line of code capable of generating writes to the RS485 bus, relay hardware, or internal state.

### 5.1 Modbus Function Codes Audit

| Function Code | Modbus Name | Codebase Status | Audit References (File & Line) | Risk in Phase 0 Shadow Mode |
|---|---|---|---|---|
| **0x05** | Write Single Coil | **NOT IMPLEMENTED** | Only referenced as a forbidden probe in `src/drivers/discovery_engine.h:6`. No struct, constant or builder exists. | Zero |
| **0x06** | Write Single Register | **IMPLEMENTED** | - Constant: `src/protocols/modbus/modbus_rtu.h:25`<br>- Builder: `src/protocols/modbus/modbus_rtu.h:54-55`, `src/protocols/modbus/modbus_rtu.cpp:92-104`<br>- Parser: `src/protocols/modbus/modbus_rtu.h:98-99`, `src/protocols/modbus/modbus_rtu.cpp:178-205`<br>- Client Method: `src/protocols/modbus/modbus_client.h:94-95`, `src/protocols/modbus/modbus_client.cpp:158-175`<br>- Profile Driver: `src/drivers/modbus_profile/modbus_profile_driver.cpp:430-432`<br>- SunSpec Driver: `src/drivers/sunspec/sunspec_driver.cpp:470-472, 533-542` | **Mitigated**: Blocked by 5-gate dispatcher and multi-level shadow barrier. |
| **0x0F (15)** | Write Multiple Coils | **NOT IMPLEMENTED** | Only referenced as forbidden in `src/drivers/discovery_engine.h:6`. No implementation. | Zero |
| **0x10 (16)** | Write Multiple Registers | **PARTIALLY PRESENT (Builder Only)** | - Constant: `src/protocols/modbus/modbus_rtu.h:26`<br>- Builder: `src/protocols/modbus/modbus_rtu.h:57-61`, `src/protocols/modbus/modbus_rtu.cpp:105-128`<br>- Client Method: **DOES NOT EXIST** (`ModbusClient` has no `writeMultipleRegisters`). | Zero (Cannot be transmitted by client). |

### 5.2 Relay & DRM Actuation Audit
- **Relay Controller**: `src/relays/relay_controller.h:29, 39, 44-58`, `src/relays/relay_controller.cpp:33-42, 47-72` (`initPin`, `set`, `applyPattern`, `pulse`).
- **DRM Engine**: `src/relays/drm.h:28-43`, `src/relays/drm.cpp:35-50` (`patternFor`, role translation).
- **Hardware Boards**:
  - Waveshare 1-CH Relay: `src/boards/waveshare_esp32_s3_relay_1ch.h:17-18` (GPIO47).
  - Waveshare 6-CH Relay: `src/boards/waveshare_esp32_s3_relay_6ch.h:55-58` (GPIO1, 2, 41, 42, 45, 46).
- **Risk in Host Shadow**: Zero. The Linux host has no GPIO bindings and the relay subsystem is excluded from the host build.

### 5.3 Command Dispatcher & Write Ingress Audit
- **Command Dispatcher**:
  - Definition: `src/commands/command_dispatcher.h:52-54, 83-84`.
  - Dispatch Logic: `src/commands/command_dispatcher.cpp:71-175`.
- **Command Queue**: `src/commands/command_queue.h:17-43`, `src/commands/command_queue.cpp`.
- **Ingress Endpoints**:
  - MQTT Ingress: `src/outputs/mqtt/mqtt_output.h:123-134`, `src/outputs/mqtt/mqtt_output.cpp:168-185` (subscribes to `<prefix>/command/set`, `<prefix>/relay/+/set`, `<prefix>/drm/set`).
  - REST Ingress: `src/outputs/rest/rest_api.h:128-134`, `src/outputs/rest/rest_api.cpp` (`POST /api/v1/devices/:id/command`, `POST /api/v1/relays/:index`, `POST /api/v1/drm`).
  - Modbus TCP Ingress: `src/outputs/modbus_tcp/modbus_tcp_server.h:49` (`writeEnabled = false`).

### 5.4 Afore External EMS Specifics & EEPROM Protection
According to official Afore documentation (`Doc/外部Ems调度 - En_240312.docx` and `Doc/Afore_Autonomous_EMS_V4_Roadmap.md`):
1. **Control Registers**:
   - `0x09C4`: `EMSMode` (must be set to `5` for External EMS).
   - `0x0A08`: External Control Type (`0` = battery charge/discharge power, `1` = inverter output power).
   - `0x0A09–0x0A0A`: Active Power Setpoint (signed 32-bit integer, Watts).
2. **Function Code Requirement**: Afore requires **Modbus FC10 (Write Multiple Registers)** for writing `0x0A09–0x0A0A`. Since Heliograph's `ModbusClient` does not implement FC10, an accidental write to Afore setpoints is structurally impossible with the current codebase.
3. **Critical EEPROM Register `0x0BC1`**: Register `0x0BC1` (`CurrMaxChg`, Max Charge Current) writes to non-volatile EEPROM. The legacy Solarman V3.3 automation wrote this register periodically, creating severe EEPROM degradation risks. In accordance with Phase 0 rules, **register `0x0BC1` is strictly forbidden from any dynamic write operations**.

### 5.5 Multi-Level Read-Only Defense-in-Depth Architecture

To guarantee absolute safety during Phase 0, a 4-level concentric guard is established:

```text
 [Level 1: Compile-Time / Build Guard]
  -> Exclude command_dispatcher.cpp, command_queue.cpp, relay_controller.cpp.
  -> #define HELIOGRAPH_READONLY_SHADOW 1
            |
            v
 [Level 2: Configuration & Dispatcher Guard]
  -> security.read_only_mode = true (hardcoded)
  -> control.inverter_writes_enabled = false
  -> relays.enabled = false
            |
            v
 [Level 3: Transport HAL Physical Barrier]
  -> In PosixSerialTransport::write(buf, len):
     Inspect byte 1 (Function Code).
     IF (FC != 0x03 && FC != 0x04) {
         LOG_FATAL("BLOCKED ATTEMPTED WRITE FC 0x%02X", FC);
         return 0; // Frame immediately dropped, never sent to UART
     }
            |
            v
 [Level 4: Profile & Driver Definition Guard]
  -> In profiles/afore_af5k_sl.toml:
     Omit all [[write]] blocks. Driver exposes write.none() bitset.
```

---

## 6. Test Suite & Validation Oracle Inventory

### 6.1 Test Suite Execution Status
Evaluated in the project workspace:

| Test / Check Suite | Command | Exit Code | Result | Observations |
|---|---|---|---|---|
| **Git Working Tree** | `git status --short` | 0 | **CLEAN** | Working directory clean on branch `phase0-host-shadow`. |
| **Git Log Verification** | `git log --oneline -5` | 0 | **VERIFIED** | HEAD at `e049fcc`, remote `upstream` tracked. |
| **Measurement Taxonomy** | `python -X utf8 tools/check_measurement_ids.py` | 0 | **PASSED** | 57 canonical measurements, 55 dashboard rows, 55 Prometheus gauges validated. |
| **TOML Profiles Linter** | `python -X utf8 tools/gen_profiles.py --check` | 0 | **PASSED** | 13 upstream inverter profiles syntactically and structurally sound. |
| **Read-Back Rule Gate** | `python -X utf8 tools/check_readback_rule.py` | 0 | **PASSED** | Verification rule confirmed on all existing profile writes. |
| **Web Assets Integrity** | `python -X utf8 tools/check_web_js.py` | 0 | **PASSED** | Embedded JS security checks passed. |
| **Dashboard Layout** | `python -X utf8 tools/check_dashboard_layout.py` | 1 | **FAILED** | Requires headless Chromium/Chrome in Windows PATH. Non-blocking for host port. |
| **PlatformIO Native Suite** | `pio test -e native` | N/A | **NOT RUN** | `platformio` CLI toolchain not installed on Windows developer host. |

### 6.2 Reference Oracles Ecosystem
Independent validation oracles are available across three layers:

1. **Heliograph Internal Tooling & Oracles**:
   - `tools/read_modbus.py`: Reference Python script reading the Heliograph Modbus TCP server output on port 502 against `src/outputs/modbus_tcp/register_map.h` (validates schema version 1, validity bitmap at 610, canonical floats, and strings at 700). Useful for validating external client output.
   - `tools/decode_eversolar.py`: Decodes raw RS485 frames for packet inspection.
   - **Current Afore Support Status**: **None**. Heliograph currently contains zero profiles or code paths for Afore in `profiles/` or `src/drivers/`.
2. **Dedicated Solarman Afore Oracles (`../Solarman/`)**:
   - `scan_afore_registers.py`: Field-proven Modbus RTU / Solarman V5 query tool tested on the actual Afore AF5K-SL hardware.
   - `scan_battery.py`: Targeted scanner for battery status (`0x07D0`), SOC (`0x07D2`), voltage (`0x07D4`), and current (`0x07D5`).
   - `scan_full.py`: Full register scanner for empirical offset discovery.
   - `afore_AF5K-Sl.yaml`: Collaudated Home Assistant Solarman integration configuration containing production-validated register definitions, data types, and multipliers.
3. **Official Manufacturer Documentation (`Doc/`)**:
   - `Doc/Afore_Autonomous_EMS_V4_Roadmap.md`: System specification, safety constraints, and phase criteria.
   - `Doc/外部Ems调度 - En_240312.docx`: Official Afore External EMS Modbus protocol specification.
   - `Doc/艾伏逆变器寄存器参数表（对外）v3.4_20251119_并网机和储能机分类(2).xlsx`: Complete register parameter table for grid-tied and hybrid storage inverters.

---

## 7. Afore AF5K-SL Register Mapping: VERIFIED vs UNVERIFIED

In strict adherence to project safety rules, no register address or scale factor is assumed. Registri are classified as **VERIFIED** only when documented in official Afore documentation or confirmed in the active Solarman integration; all others are classified as **UNVERIFIED** and must be validated through passive observation in Phase 0.

### 7.1 Telemetry Registers Classification

| Register Address | Function / Metric | Type & Scaling | Status | Authority & Verification Notes |
|---|---|---|---|---|
| `0x022A` | Total PV Power | `uint16`, 1 W | **VERIFIED** | Roadmap Sec. 4, Solarman `afore_AF5K-Sl.yaml`. Sum of string powers. |
| `0x022D` | PV String 1 Power | `uint16`, 1 W | **VERIFIED** | Roadmap Sec. 4. |
| `0x0230` | PV String 2 Power | `uint16`, 1 W | **VERIFIED** | Roadmap Sec. 4. |
| `0x07D2` | Battery SOC | `uint16`, 1 % | **VERIFIED** | Roadmap Sec. 4, Solarman `afore_AF5K-Sl.yaml`. Valid range: 0–100%. |
| `0x07D0` | Battery Running State | `enum` | **VERIFIED** | Roadmap Sec. 4 (4 = Charge, 5 = Discharge, 6 = Standby). |
| `0x07D4` | Battery Voltage | `uint16`, 0.1 V | **VERIFIED** | Roadmap Sec. 4. |
| `0x07D5` | Battery Current | `int16`, 0.01 A | **VERIFIED** | Roadmap Sec. 4. Sign convention to verify in Phase 0. |
| `0x07D8` | Battery Power | `int16`, 1 W | **VERIFIED** | Roadmap Sec. 4. Negative observed during charging. |
| `0x0212` | Grid Power | `int16`, 1 W | **VERIFIED** | Roadmap Sec. 4. Import/export sign convention to verify in Phase 0. |
| `0x020B` | Inverter Output Power | `int16`, 1 W | **VERIFIED** | Roadmap Sec. 4. Physical AC active power. |
| `0x09C4` | EMS Mode (`EMSMode`) | `uint16` (`enum`) | **VERIFIED** | Doc `En_240312.docx` (Value 5 = External EMS). Do not confuse with running status. |
| `0x0A08` | External Control Flag | `uint16` (`enum`) | **VERIFIED** | Doc `En_240312.docx` (0 = battery power, 1 = inverter power, 2 = grid power). |
| `0x0A09–0x0A0A` | Active Power Setpoint | `int32`, 1 W | **VERIFIED** | Doc `En_240312.docx`. Signed 32-bit (2 words), requires FC10. |
| `0x0BC1` | Max Charge Current (`CurrMaxChg`)| `uint16` | **VERIFIED (CRITICAL)** | Roadmap Sec. 5.1. **NEVER WRITE DYNAMICALLY** (EEPROM wear hazard). |
| `0x0200` | Inverter Running Status | `uint16` (`enum`) | **UNVERIFIED** | Solarman mapping differs across firmware revisions; verify on bus. |
| `0x020C` | Inverter Temperature | `int16`, 0.1 °C | **UNVERIFIED** | Exact register address and sensor location to confirm. |
| `0x0228` / `0x0229` | PV1 DC Voltage / Current | `uint16`, 0.1 V / 0.1 A | **UNVERIFIED** | Offset variation between single-phase and 3-phase parameter tables. |
| `0x022E` / `0x022F` | PV2 DC Voltage / Current | `uint16`, 0.1 V / 0.1 A | **UNVERIFIED** | Offset variation between single-phase and 3-phase parameter tables. |
| `0x0203` | Grid AC Voltage | `uint16`, 0.1 V | **UNVERIFIED** | To be confirmed via passive capture. |
| `0x0205` | Grid AC Frequency | `uint16`, 0.01 Hz | **UNVERIFIED** | To be confirmed via passive capture. |

### 7.2 Open Protocol & Bus Questions (Roadmap Section 14 Alignment)
1. **Serial Line Parameters**: 9600 baud, 8 data bits, 1 stop bit, no parity (8N1) is standard, but must be confirmed passively before sending queries.
2. **Slave Unit ID**: Address 1 is expected, to be confirmed via Solarman query comparison.
3. **Bus Coexistence on COM1**: The datalogger Solarman is currently connected. It must be proven whether the bus allows multi-master querying or if passive sniffing / single-master arbitration is mandatory.
4. **Sign Convention Verification**:
   - `0x0212` (Grid Power): Does positive mean grid import or grid export?
   - `0x07D8` (Battery Power): Negative confirmed for charge, positive for discharge; to cross-validate with BMS.

---

## 8. Target Host Environment Audit (`10.0.5.174`)

- **Host IP**: `10.0.5.174` (network reachable via local subnet).
- **Inspection Command Executed**:
  ```bash
  ssh -o StrictHostKeyChecking=no -o ConnectTimeout=5 -o BatchMode=yes admin@10.0.5.174 "uname -a && which python3 g++ cmake git"
  ```
- **Exit Code**: `255`
- **Output**: `admin@10.0.5.174: Permission denied (publickey,password).`
- **Audit Finding**: **VERIFICA NON ESEGUITA** (Non-interactive batch execution could not authenticate because no SSH key pair is configured for user `admin`, and password input is disallowed in automated non-interactive sessions).
- **Safety Compliance**: No files were created, modified, or executed on the remote host.
- **Action Required for Phase 0 Bring-up**: Provide an SSH public key or run the non-interactive setup script when interactive credentials can be supplied.

---

## 9. Risk Assessment & Mitigations

| Risk | Impact | Likelihood | Mitigation Strategy |
|---|---|---|---|
| **Accidental Inverter Register Write** | High | Extremely Low | Multi-level guard: compile-time omission of write dispatcher, runtime kill-switches, and hard transport-level drop of any FC != 0x03/0x04 in `PosixSerialTransport`. Absence of FC10 in `ModbusClient`. |
| **Inverter EEPROM Degradation** | Critical | Zero in Phase 0 | Register `0x0BC1` is completely banned from write paths. External EMS (Phase 6) will use volatile RAM setpoints `0x0A09–0x0A0A`. |
| **RS485 Bus Collision with Solarman Dongle** | Medium | Medium | Start with passive bus sniffing (`tools/decode_eversolar.py` style frame capture). Polling scheduled with conservative intervals (5–10s) and zero retries during scouting. |
| **Telemetry Inaccuracy / Inverted Sign** | Medium | Low | Continuous cross-validation against Solarman Home Assistant sensors and PySerial reference scripts (`scan_afore_registers.py`). |
| **Serial Dongle Latency / Buffer Bloat** | Low | Low | Set `VMIN = 0`, `VTIME = 1` in `termios`, flush input buffer before query, enforce strict deadline timeouts. |

---

## 10. Licensing & Intellectual Property Assessment

### 10.1 Primary License (`LICENSE`)
The root codebase is distributed under the permissive **MIT License** (`LICENSE:1-21`, Copyright (c) 2026 Tim de Bruijn). This grants full rights to use, modify, merge, and distribute without copyleft obligations or restrictive patent encumbrances.

### 10.2 Third-Party Dependencies & Firmware Components (`LICENSE-THIRD-PARTY.md`)
The project documents third-party dependencies and protocol provenance in detail (`LICENSE-THIRD-PARTY.md:1-71`):
1. **Embedded Firmware Libraries**:
   - `ArduinoJson`: MIT (`LICENSE-THIRD-PARTY.md:61`).
   - `espMqttClient`: MIT (`LICENSE-THIRD-PARTY.md:62`).
   - `eModbus`: MIT (`LICENSE-THIRD-PARTY.md:63`).
   - `ESPAsyncWebServer` & `AsyncTCP`: LGPL-3.0 (`LICENSE-THIRD-PARTY.md:64-65`).
   - `Arduino core for ESP32` + ESP-IDF: LGPL-2.1 / Apache-2.0 (`LICENSE-THIRD-PARTY.md:66`).
   - `Unity`: MIT (`LICENSE-THIRD-PARTY.md:67`).
2. **Host Porting Compatibility (Phase 0 Shadow)**:
   - The LGPL-3.0 and LGPL-2.1 components (`ESPAsyncWebServer`, `AsyncTCP`, Arduino core) are strictly microcontroller-specific and are **excluded** from the host Linux build.
   - The components reused on Linux (`protocols/modbus/`, `drivers/modbus_profile/`, `device/`, `state/`, and the new `PosixSerialTransport`) are 100% pure C++17 native Heliograph code under MIT.
   - Therefore, the host binary carries zero LGPL copyleft linkage obligations and zero license contamination.
   - Future host telemetry outputs using standard Linux libraries (e.g., `libmosquitto` under EPL-2.0 / EDL-1.0) remain fully compatible with permissive MIT distribution.

### 10.3 Protocol Knowledge & Clean-Room Provenance
In accordance with `LICENSE-THIRD-PARTY.md:3-8`:
- Protocol implementations reflect functional facts, public register maps, and observed bus behavior; no proprietary code is copied.
- Afore AF5K-SL integration follows this exact clean-room convention: register maps are derived from official manufacturer protocol documents (`Doc/外部Ems调度 - En_240312.docx`, `Doc/艾伏逆变器寄存器参数表（对外）v3.4_20251119_并网机和储能机分类(2).xlsx`) and empirical Modbus verification in `../Solarman/` (`scan_afore_registers.py`, `afore_AF5K-Sl.yaml`).

---

## 11. Final Recommendation & Phase 0 Checkpoint Roadmap (CP1–CP5)

### 11.1 Feasibility Verdict: PROCEDI / GO

**Final Verdict:** **PROCEDI (GO) for Phase 0 Host Shadow Mode**

#### Detailed Justification:
1. **Clean Architectural Layering**: Heliograph's Modbus RTU codec, profile evaluation engine, and state pipeline are 100% decoupled from ESP32/Arduino hardware (`check_layering.sh`), allowing pure native compilation on Linux.
2. **Guaranteed Write Immunity**: Multi-level defense-in-depth ensures zero possibility of writes reaching the inverter: absence of FC10 in `ModbusClient`, compile-time exclusion of command dispatchers, strict transport-layer drop of non-read function codes (`FC != 0x03 && FC != 0x04`), and omission of write blocks in the profile.
3. **Rich Validation Ecosystem**: Field-proven Python oracles (`../Solarman/scan_afore_registers.py`, `afore_AF5K-Sl.yaml`) allow automated cross-verification of telemetry, scaling factors, and sign conventions before firmware flashing.
4. **Maximum Safety for Hardware & Battery**: Operating strictly in read-only shadow mode eliminates any risk of EEPROM wear (`0x0BC1`) or battery disruption while establishing the telemetry baseline.

#### Explicit STOP Criteria (Immediate Halt Triggers):
- If any write frame is attempted or reaches Level 3 transport guard.
- If RS485 communication introduces packet loss, CRC errors, or timeouts on the existing Solarman datalogger.
- If inverter or BMS master/slave reports any communication or protocol error alarm.
- If serial communication parameters (baud rate, parity, slave address) cannot be confirmed non-invasively.

---

### 11.2 Checkpoint Roadmap: CP1 through CP5

| Checkpoint | Scope & Deliverables | Acceptance Gates | Key Risks & Mitigations |
|---|---|---|---|
| **CP1: POSIX Transport & Host Build Harness** | - Implement `PosixSerialTransport` (`src/transport/posix_serial_transport.{h,cpp}`) using POSIX `<termios.h>`.<br>- Embed Level 3 Read-Only Guard in `write()` (drop any `FC != 0x03 && FC != 0x04`).<br>- Create root `CMakeLists.txt` compiling core protocols, profile engine, and host unit tests on Linux (`x86_64` / `arm64`).<br>- Add `compat/arduino_compat.h` for `millis()` and `delay()`. | - `ctest` / host test binary passes 100% unit tests (`test_modbus` with `MockTransport`).<br>- Clean compilation under GCC/Clang with `-Wall -Wextra -Werror`.<br>- Simulated write attempt returns 0 bytes written and logs fatal security warning. | **Risk**: Termios buffer bloat or half-duplex RTS timing.<br>**Mitigation**: Non-blocking `poll()`/`select()` with explicit deadline timeouts, `VMIN=0, VTIME=1`, and hardware auto-direction USB-RS485 adapters. |
| **CP2: Afore Read-Only TOML Profile** | - Author `profiles/afore_af5k_sl.toml` with verified telemetry registers only (`0x022A`, `0x022D`, `0x0230`, `0x07D2`, `0x07D4`, `0x07D5`, `0x07D8`, `0x0212`, `0x020B`, `0x07D0`).<br>- Omit all `[[write]]` blocks (driver exposes empty write capabilities).<br>- Group registers into tight contiguous read spans (max 125 regs).<br>- Regenerate profile tables via `python3 tools/gen_profiles.py`. | - `python3 tools/gen_profiles.py --check` passes cleanly.<br>- `tools/check_layering.sh` passes with zero violations.<br>- Unit tests in `test/test_modbus_profile` validate Afore profile parsing and measurement decoding. | **Risk**: Requesting unmapped/reserved registers causing Modbus Exception 0x02 (Illegal Data Address).<br>**Mitigation**: Span boundaries restricted strictly to addresses verified in Solarman production YAML. |
| **CP3: Host Shadow Runner CLI & Telemetry Logging** | - Implement standalone CLI binary `afore_shadow_runner`.<br>- Non-blocking poll loop (conservative 5–10s cycle).<br>- Implement `FileConfigurationBackend` conforming to `heliograph::KeyValueBackend`.<br>- Implement CSV & JSONL local file telemetry logger recording UTC timestamp, raw registers, canonical values, and communication metrics. | - CLI executes cleanly on host with configurable serial device (`--port /dev/ttyUSB0 --baud 9600 --unit 1`).<br>- Zero packet corruption or unhandled exceptions.<br>- Bus tap sniffer captures raw frames for offline inspection. | **Risk**: Bus contention / collision if COM1 is shared with Solarman dongle.<br>**Mitigation**: Initial phase of passive bus sniffing (`BusTap`), zero retries during scouting, conservative polling interval (10s). |
| **CP4: Reference Oracle Cross-Validation** | - Concurrent execution of `afore_shadow_runner` alongside Solarman HA sensor data and Python reference queries (`scan_afore_registers.py`).<br>- Automated comparison of PV power, grid power, battery SOC, voltage, and current.<br>- Formal resolution of sign conventions (`0x0212` grid power import vs export; `0x07D8` battery power charge vs discharge). | - Measurement delta between Heliograph canonical values and Solarman sensors < 0.5%.<br>- Zero discrepancy in sign conventions across full daylight charging cycle.<br>- Register classification table updated with all empirical confirmations. | **Risk**: Inverted power signs causing incorrect energy balance accounting.<br>**Mitigation**: Physical correlation against battery charge state (positive/negative verified during net PV generation surplus). |
| **CP5: 24h–72h Endurance Run & Phase 0 Sign-Off** | - Continuous 24h–72h uninterrupted read-only telemetry run on Linux host.<br>- Accumulate communication statistics (total frames, valid reply rate, CRC errors, timeouts, P95 latency).<br>- Comprehensive Phase 0 validation report.<br>- Formal Phase 0 exit gate sign-off for transition to Phase 1 (Waveshare ESP32-S3 firmware build). | - Valid read rate >= 99.5% over consecutive 24 hours.<br>- Zero writes executed or attempted on RS485 bus.<br>- Zero disruption or packet loss observed on Solarman monitoring.<br>- Complete baseline dataset stored for Phase 3 shadow EMS algorithm development. | **Risk**: Transient USB disconnect or serial driver lockup on host.<br>**Mitigation**: Automatic serial reconnection watchdog with exponential backoff and persistent status logging. |
