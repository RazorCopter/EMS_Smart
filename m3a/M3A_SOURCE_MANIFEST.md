# M3A Source Manifest

## Objective

Validate ReadOnlyGuard safety core in complete isolation from:
- Configuration system
- Network stack  
- MQTT
- Serial I/O
- Host daemon
- Telemetry engine
- Shadow EMS

## Production Sources Included

### Safety Core
- `src/transport/readonly_guard.cpp` (189 lines)
- `src/transport/readonly_guard.h` (170 lines)

### Transport Abstraction
- `src/transport/transport.h` (interface, 146 lines)
- `src/transport/transport.cpp` (tap implementation)

### Modbus Protocol
- `src/protocols/modbus/modbus_rtu.cpp` (CRC16, frame helpers)
- `src/protocols/modbus/modbus_rtu.h` (protocol constants)

### Utility
- `src/protocols/byte_order.h` (header-only, endian conversions)

## Test Sources Included

- `m3a/test_readonly_guard_m3a.cpp` (26 tests, M3A harness)
- `m3a/fake_lower_transport.h` (mock transport, no I/O)
- `m3a/m3a_logger_stub.h` (minimal logger, no dependencies)

## Modules Explicitly EXCLUDED

### Configuration & Storage
- ArduinoJson (entire library)
- Configuration (all config/*.{h,cpp})
- NVS backend
- Host config backend
- State store

### Network & I/O
- PosixSerialTransport (real serial, `/dev/tty*`)
- Modbus TCP server
- MQTT (all outputs/mqtt/*.{h,cpp})
- REST API
- Web server
- WiFi manager
- Time manager

### Application Logic
- Telemetry engine (`afore_telemetry_engine.*`)
- Shadow EMS controller (`shadow_ems_controller.*`)
- Command dispatcher
- Command queue
- Register reader
- Discovery runner
- Capture runner

### Drivers
- All drivers in `src/drivers/*` (mock, modbus_profile, sunspec, etc.)

### Hardware
- Relay controller
- DRM
- Board definitions
- Status LED

### System
- Host main (`src/host/main.cpp`)
- systemd integration

## Rationale

M3A verifies the **read-only boundary enforcement** of the safety core.

This requires:
1. ReadOnlyGuard implementation
2. Transport interface (abstract)
3. Modbus CRC computation (production function)
4. Frame structure validation logic
5. Fake transport (test double, no I/O)

Everything else is **excluded** to prove the safety core:
- Does not depend on configuration at link time
- Does not depend on network at link time  
- Does not perform actual serial/network I/O
- Compiles independently of host daemon
- Contains no references to write commands (FC06, FC10)

## Build Characteristics

- **C++17** standard
- **AFORE_SHADOW_BUILD=1** (setForceReadOnly disabled at compile time)
- **No external dependencies** (no FetchContent, no ArduinoJson)
- **Explicit source lists** (no glob, no automatic inclusion)
- **Target-local settings** (no global compile options leaking)

## Expected Binary Properties

### Present (Expected)
- `heliograph::ReadOnlyGuard::write`
- `heliograph::ReadOnlyGuard::validateModbusRtuFrame`
- `heliograph::modbus::crc16`
- `FakeLowerTransport`
- M3A test harness symbols

### Absent (Required)
- `CommandDispatcher`
- `CommandQueue`
- `RelayController`
- `DRM`
- `writeSingleRegister` / `writeMultipleRegisters` (application-level)
- `PosixSerialTransport`
- `/dev/tty` string literals
- `mqtt` namespace symbols
- `command/set` topic strings
- `factory` / `reboot` strings

## Verification Method

1. **Build verification**: CMake configure + build exits 0
2. **Test execution**: CTest + direct run exits 0, all tests pass
3. **Negative compile**: `setForceReadOnly(false)` fails to compile
4. **Binary inspection**: `nm`, `objdump`, `readelf`, `strings` confirm exclusions
5. **I/O audit**: `strace` confirms no `/dev/tty*`, no `connect()`

## M3A Acceptance Criteria

- Build: exit 0
- Configure: exit 0  
- Test count: 26 executed
- Test pass: 26/26
- Test fail: 0/26
- CTest: exit 0
- Direct run: exit 0
- Negative compile: compile FAILS (expected)
- Binary: no write commands, no serial, no network
- I/O: no actual device access during test execution
