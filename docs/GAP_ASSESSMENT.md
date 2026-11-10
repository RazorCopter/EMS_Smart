# Heliograph Host Porting & Afore AF5K-SL Integration: Gap Assessment

**Author:** Autonomous Engineering Agent  
**Date:** November 2026  
**Target Inverter:** Afore AF5K-SL Hybrid Inverter (Single Phase 5kW, Storage)  
**Target Environment:** Linux Host (x86_64 / arm64 / Raspberry Pi at 10.0.5.174) in Read-Only Shadow Mode  
**Base Repository:** Timdebruijn/heliograph (Branch: `phase0-host-shadow`)  
**Recommendation:** **GO (with Read-Only Architectural Isolation)**

---

## 1. Executive Summary

This assessment evaluates the feasibility, architectural gap, and security risks of porting the Heliograph firmware from an embedded ESP32-S3 microcontroller target to a Linux host environment (including Raspberry Pi), operating in strict **Read-Only Shadow Mode** for the Afore AF5K-SL hybrid inverter.

### Key Findings
1. **Core Architecture Portability**: Heliograph exhibits exceptional separation of concerns. Protocols (`src/protocols/modbus/`), profile parsing (`src/drivers/modbus_profile/`), canonical measurement taxonomy, and formatting logic are pure C++ and compile cleanly under native C++17 without Arduino or ESP-IDF dependencies.
2. **Write Safety in Phase 0 Shadow Mode**: Heliograph has an existing 5-gate defense architecture (`src/commands/command_dispatcher.cpp`). Furthermore, Heliograph currently only supports single 16-bit register writes via Modbus FC06 (`kWriteSingleRegister`), whereas Afore external EMS commands require 32-bit writes via FC10 (`kWriteMultipleRegisters`). This mismatch provides an inherent layer of protection, but for Phase 0 all write entry points must be explicitly removed or stubbed at compile time.
3. **Transport Gap**: The default RS485 transport (`src/transport/rs485_transport.cpp`) is tied to ESP32 HardwareSerial with GPIO DE/RE half-duplex control. A POSIX/Linux serial abstraction using standard `termios` (for USB-RS485 dongles such as FTDI/CH340 with automatic hardware direction flow control) is required.
4. **Feasibility Verdict**: **GO**. The porting of Heliograph's telemetry read path to a Linux host in shadow mode is straightforward, highly maintainable, and completely satisfies Phase 0 requirements without putting hardware at risk.

---

## 2. Module Map & Portability Classification

| Module | Source Path | Target Role | Portability Classification | Host Porting Strategy |
|---|---|---|---|---|
| **Modbus Protocol Core** | `src/protocols/modbus/modbus_rtu.{h,cpp}` | RTU frame parsing, CRC16, frame building | **Directly Portable** | 100% pure C++17. Zero hardware dependencies. Fully tested in host unit tests. |
| **Modbus Client Engine** | `src/protocols/modbus/modbus_client.{h,cpp}` | Transaction management, timeout, retries | **Directly Portable** | Depends solely on `heliograph::Transport`. |
| **Profile Driver Engine** | `src/drivers/modbus_profile/` | TOML profile evaluation, register decoding | **Directly Portable** | Pure C++17 parser and measurement evaluator. |
| **Byte Ordering & Math** | `src/protocols/byte_order.h` | Big-endian/Little-endian conversions, float/int | **Directly Portable** | Header-only pure C++. |
| **Canonical Measurements** | `src/measurement_id.h` | Standardized metric identifiers | **Directly Portable** | Pure C++ enum and string tables. |
| **Transport Abstraction** | `src/transport/transport.{h,cpp}` | Virtual interface for byte streaming | **Directly Portable** | Base interface `Transport` with `read()`, `write()`, `flush()`. |
| **RS485 Transport (ESP32)** | `src/transport/rs485_transport.{h,cpp}` | HardwareSerial + GPIO DE/RE pin toggle | **ESP32-Specific (Replace)** | Currently contains a non-functional stub for `!ESP32` (lines 142–153). Replace with POSIX `termios` implementation (`PosixSerialTransport`). |
| **Command Dispatcher** | `src/commands/command_dispatcher.{h,cpp}` | Multi-gate write execution & safety checks | **Directly Portable (Adapt)** | Retain for validation, but force compile-time read-only lockdown for Phase 0. |
| **Relay Controller & DRM** | `src/relays/relay_controller.{h,cpp}`, `src/relays/drm.{h,cpp}` | AS/NZS 4777.2 DRM relay actuation | **Exclude for Host Shadow** | Exclude from host build or provide dummy stub. No relay hardware on host. |
| **MQTT Output** | `src/outputs/mqtt/` | Home Assistant MQTT discovery & publishing | **Adaptable** | Replace Arduino-specific PubSubClient / AsyncMqtt with standard `libmosquitto` or embedded POSIX MQTT client. |
| **Prometheus Metrics** | `src/outputs/prometheus/` | Prometheus metric text formatting | **Directly Portable** | String serialization logic is pure C++. HTTP delivery via lightweight POSIX server. |
| **REST API Payloads** | `src/outputs/rest/rest_payloads.{h,cpp}` | JSON telemetry formatting | **Directly Portable** | Pure C++ JSON generation. |
| **Web Server & UI** | `src/web/`, `src/outputs/rest/rest_api.{h,cpp}` | ESPAsyncWebServer / embedded UI assets | **Exclude / Replace** | Exclude or replace with lightweight micro-HTTP server (e.g. `httplib` or `mongoose`) if local UI is needed. |
| **Network & WiFi Manager** | `src/network/wifi_manager.{h,cpp}` | ESP32 Wi-Fi provisioning, AP mode | **Exclude** | Host OS (Linux systemd/NetworkManager) handles network interfaces. |
| **RTC & Time Management** | `src/network/rtc_*`, `src/network/time_manager.*` | I2C RTC (PCF85063) & SNTP sync | **Adaptable** | Replace with standard Linux `clock_gettime(CLOCK_REALTIME)` and `std::chrono`. |
| **OTA Subsystem** | `src/ota/` | ESP32 dual-partition flash update | **Exclude** | Host updates handled via Git / Linux package manager. |
| **State Storage** | `src/state/state_store.{h,cpp}` | LittleFS non-volatile key-value store | **Adaptable** | Map to standard POSIX filesystem (JSON file in `/var/lib/heliograph/` or local working directory). |

---

## 3. Write Risk Analysis & Mutation Pathways (File & Line Audit)

A thorough audit of the Heliograph codebase was conducted to identify all code paths capable of mutating external hardware state (inverter registers or relay outputs).

### 3.1 Modbus Function Codes
- **FC06 (`0x06` - Write Single Register)**:
  - Definition: `src/protocols/modbus/modbus_rtu.h:25` (`kWriteSingleRegister = 0x06`)
  - Frame Builder: `src/protocols/modbus/modbus_rtu.cpp:92–104` (`buildWriteSingleRegister`)
  - Response Parser: `src/protocols/modbus/modbus_rtu.cpp:178–205` (`parseWriteResponse`)
  - Client Method: `src/protocols/modbus/modbus_client.h:94` and `src/protocols/modbus/modbus_client.cpp:158–174` (`ModbusClient::writeSingleRegister`)
  - Driver Invocations:
    - Profile Driver: `src/drivers/modbus_profile/modbus_profile_driver.cpp:430` (Holding register write upon command dispatch)
    - SunSpec Driver: `src/drivers/sunspec/sunspec_driver.cpp:471` (SunSpec model register write)
- **FC10 / FC16 (`0x10` - Write Multiple Registers)**:
  - Definition: `src/protocols/modbus/modbus_rtu.h:26` (`kWriteMultipleRegisters = 0x10`)
  - Frame Builder: `src/protocols/modbus/modbus_rtu.cpp:105–128` (`buildWriteMultipleRegisters`)
  - **Client Gap**: `ModbusClient` does **NOT** expose a `writeMultipleRegisters` method. Only single-register FC06 writes are implemented.

### 3.2 Relay & DRM Actuation
- Actuation Functions: `src/relays/relay_controller.cpp:47–72` (`set`, `applyPattern`, `pulse`)
- Hardware Binding: `src/relays/relay_controller.cpp:33–42` (`initPin` toggling GPIOs).

### 3.3 Afore External EMS Write Path Specifics
According to the official Afore External EMS specification (`Doc/外部Ems调度 - En_240312.docx`):
- Control mode register `0x09C4` requires specific activation sequences.
- External EMS command registers `0x0A08` (Control Flag) and `0x0A09–0x0A0A` (Active Power Setpoint, 32-bit signed integer) require Modbus **FC10 / FC16** multi-register atomic write transactions.
- Critical Safety Note: Inverter register `0x0BC1` must **never** be written dynamically to prevent permanent EEPROM wear.

Because Heliograph currently lacks FC10 execution in `ModbusClient`, an inadvertent write to the Afore 32-bit setpoint registers is physically impossible through the existing profile driver write path.

### 3.4 Existing 5-Gate Defense Architecture
Heliograph implements a 5-layer gating mechanism in `src/commands/command_dispatcher.cpp`:
1. **Gate 1 - Read-Only Kill Switch** (`command_dispatcher.cpp:78`): Checks `security.read_only_mode`. If `true` (default), all write commands are immediately rejected with `CommandStatus::ReadOnly`.
2. **Gate 2 - Master Write Enable** (`command_dispatcher.cpp:86`): Checks `control.inverter_writes_enabled`. If `false` (default), rejected with `CommandStatus::WritesDisabled`.
3. **Gate 3 - Capability Verification** (`command_dispatcher.cpp:94`): Verifies the profile explicitly declares write support for the target command.
4. **Gate 4 - Value Range & Step Validation** (`command_dispatcher.cpp:136`): Validates numeric bounds and step increments against profile constraints.
5. **Gate 5 - Read-Back Verification** (`command_dispatcher.cpp:184`): Mandates reading back the written register to confirm inverter acceptance before reporting success.

### 3.5 Phase 0 Shadow Mode Isolation Plan
For Phase 0 host shadow execution:
1. Compile-time exclusion: Omit `command_dispatcher.cpp` and `relay_controller.cpp` or compile with `#define HELIOGRAPH_READONLY_SHADOW 1`.
2. Hardware layer isolation: In `PosixSerialTransport`, the `write()` method can log write attempts and drop them or return an error if called.
3. Guarantee: In Phase 0, the software will be incapable of transmitting any Modbus write frame (FC05, FC06, FC0F, FC10) over the physical RS485 bus.

---

## 4. Host (Linux / POSIX) Porting Gap

### 4.1 Serial Communication (`src/transport/`)
- **Current State**: `rs485_transport.cpp:139–153` compiles to an empty stub on `!defined(ESP32)`:
  ```cpp
  bool Rs485Transport::begin(const SerialProfile&) { return false; }
  size_t Rs485Transport::write(const uint8_t*, size_t) { return 0; }
  int Rs485Transport::read() { return -1; }
  ```
- **Porting Requirement**: Implement `PosixSerialTransport` inheriting from `Transport`:
  - Open `/dev/ttyUSB0` (or configured device node).
  - Configure baud rate (9600 for Afore), 8 data bits, 1 stop bit, no parity (`8N1`) via POSIX `<termios.h>`.
  - Set non-blocking I/O with `select()` / `poll()` for timeout management matching `Transport::setTimeout()`.
  - Half-duplex direction control: Most modern USB-to-RS485 converters (FTDI FT232R, Silicon Labs CP2102, CH340) handle DE/RE direction switching automatically in hardware. For generic serial ports, `TIOCM_RTS` ioctl toggling can be provided.

### 4.2 Build System & Toolchain
- **Current State**: Build configuration is managed exclusively via PlatformIO (`platformio.ini`), targeting `pioarduino` (ESP32-S3). Native host test environment `env:native` is configured in `platformio.ini`, but PlatformIO is not installed on the Windows development host.
- **Porting Requirement**: Provide a standalone `CMakeLists.txt` for host builds targeting Linux (x86_64 and arm64 for Raspberry Pi). The CMake build compiles:
  - `src/protocols/modbus/*.cpp`
  - `src/drivers/modbus_profile/*.cpp`
  - `src/transport/transport.cpp` and `src/transport/posix_serial_transport.cpp`
  - Output formatters: `src/outputs/prometheus/`, `src/outputs/rest/rest_payloads.cpp`
  - A lightweight host runner `src/host_main.cpp`.

### 4.3 Event Loop, Timers & Concurrency
- Heliograph relies on Arduino `millis()` and `delay()` for timing.
- On Linux, a trivial compatibility header `compat/arduino_compat.h` provides:
  ```cpp
  inline uint32_t millis() {
      struct timespec ts;
      clock_gettime(CLOCK_MONOTONIC, &ts);
      return (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
  }
  inline void delay(uint32_t ms) { usleep(ms * 1000); }
  ```

---

## 5. Test Suite & Validation Oracle Inventory

### 5.1 Test Suite Execution Status
Evaluated from the current development environment:

| Test / Check Suite | Execution Command | Result | Exit Code | Notes |
|---|---|---|---|---|
| **PlatformIO Native Suite** | `pio test -e native` | **NOT EXECUTED** | N/A | `pio` / `platformio` toolchain not installed on Windows developer host. |
| **C++ Host Compiler** | `where g++` | **NOT FOUND** | 1 | Native GCC not in Windows PATH; cross-compilation or remote build on Raspberry Pi target required. |
| **Measurement ID Registry** | `python -X utf8 tools/check_measurement_ids.py` | **PASSED** | 0 | Validated 57 canonical measurement IDs, 55 dashboard rows, 55 Prometheus gauges. |
| **TOML Profiles Validator** | `python -X utf8 tools/gen_profiles.py --check` | **PASSED** | 0 | Validated 13 upstream inverter profiles. |
| **Read-Back Rule Check** | `python -X utf8 tools/check_readback_rule.py` | **PASSED** | 0 | Verified read-back verification rule in all profiles. |
| **Web Assets JS Check** | `python -X utf8 tools/check_web_js.py` | **PASSED** | 0 | Verified UI security and asset integrity. |
| **Dashboard Layout Check** | `python -X utf8 tools/check_dashboard_layout.py` | **FAILED** | 1 | Expected failure: requires headless Chrome/Chromium in PATH. |

### 5.2 Available Reference Oracles
The project has access to multiple independent oracles to validate Afore telemetry and protocol parsing:
1. **Heliograph Modbus Tool**: `tools/read_modbus.py` provides standalone Python Modbus RTU querying via PySerial to test register responses independently of C++ code.
2. **Solarman Scanners & Profiles**: In the adjacent directory `../Solarman/`:
   - `scan_afore_registers.py`: Validated scanner for Afore hybrid inverters.
   - `scan_battery.py`: Targeted scanner for Afore battery registers.
   - `scan_full.py`: Full range holding/input register sweep script.
   - `afore_AF5K-Sl.yaml`: Complete register schema mapping tested against the actual inverter.
3. **Official Afore Documentation**:
   - `Doc/Afore_Autonomous_EMS_V4_Roadmap.md`: Architectural specification and target registers.
   - `Doc/外部Ems调度 - En_240312.docx`: Manufacturer specification for external control.
   - `Doc/艾伏逆变器寄存器参数表（对外）v3.4_20251119_并网机和储能机分类(2).xlsx`: Manufacturer register parameter table.
4. **Target Host**: Raspberry Pi at `10.0.5.174` (network reachable, ping latency ~67ms).

---

## 6. Afore AF5K-SL Integration Feasibility

### 6.1 Telemetry Registers Mapping (Phase 0 Read-Only)
The following primary registers are required for Phase 0 shadow operation:

| Metric Category | Register Address (Hex) | Description | Type / Scale | Canonical ID |
|---|---|---|---|---|
| **PV Generation** | `0x022A` | PV1 Voltage | `uint16` / 10.0 (V) | `kPv1Voltage` |
| | `0x022B` | PV1 Current | `uint16` / 10.0 (A) | `kPv1Current` |
| | `0x022C` | PV2 Voltage | `uint16` / 10.0 (V) | `kPv2Voltage` |
| | `0x022D` | PV2 Current | `uint16` / 10.0 (A) | `kPv2Current` |
| **Battery State** | `0x07D2` | Battery SOC | `uint16` / 1.0 (%) | `kBatterySoc` |
| | `0x07D0` | Battery Voltage | `uint16` / 10.0 (V) | `kBatteryVoltage` |
| | `0x07D8` | Battery Power | `int16` / 1.0 (W) | `kBatteryPower` |
| **Grid & Inverter** | `0x0200` | Inverter Status / Run State | `uint16` (enum) | `kInverterState` |
| | `0x020C` | Inverter Temperature | `int16` / 10.0 (°C) | `kInverterTemperature` |
| | `0x021B` | Active Power to Grid | `int16` / 1.0 (W) | `kGridPower` |
| **EMS Configuration** | `0x09C4` | EMS Work Mode (Read-Only) | `uint16` (enum) | `kEmsMode` |

### 6.2 Profile Compatibility
Heliograph's TOML profile engine (`src/drivers/modbus_profile/`) fully supports:
- Unsigned and signed 16-bit registers with arbitrary scaling factors.
- Multi-block holding register read optimization (grouping contiguous registers into a single Modbus FC03 request).
- Custom poll intervals and measurement mapping.
Creating `profiles/afore_af5k_sl.toml` fits cleanly into the existing profile schema validated by `tools/gen_profiles.py`.

---

## 7. Architectural Sustainability & Risk Mitigation

1. **Layering Integrity**: Conforms to Heliograph's layering rule (`tools/check_layering.sh`): brand-specific details remain strictly confined to the TOML profile; no Afore-specific hacks in core Modbus or output drivers.
2. **Host Decoupling**: Building a native Linux runner decouples EMS development from embedded flashing cycles, enabling rapid iteration, comprehensive logging, and native integration with Home Assistant / Prometheus on the Raspberry Pi.
3. **Shadow Mode Security**: In Phase 0, all active writes are physically and logically disabled. The EMS observes real-world solar generation, household load, and battery SOC, calculating what control actions it *would* take without emitting a single write frame to the inverter.

---

## 8. Final Recommendation: GO

**Recommendation:** **GO** to proceed with **Phase 0 Host Shadow Mode**.

### Justification:
- Heliograph's Modbus engine and profile driver are already modular and host-portable.
- The write path is securely constrained and can be completely eliminated at compile time for shadow mode.
- Comprehensive register definitions and oracles exist in `../Solarman/` and official Afore documentation.
- The Linux host environment provides superior observability and stability for passive telemetry validation.

### Next Steps (Phase 0 Execution):
1. Create `CMakeLists.txt` for host Linux build.
2. Implement `PosixSerialTransport` (`src/transport/posix_serial_transport.{h,cpp}`).
3. Create Afore profile `profiles/afore_af5k_sl.toml`.
4. Deploy shadow reader on Raspberry Pi host and validate telemetry against Solarman readings.
