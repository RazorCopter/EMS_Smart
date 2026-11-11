# M3A Safety Core Validation Report

**Date:** 2026-11-11  
**Target:** afore-m3 (rasp, Debian GNU/Linux 12)  
**Branch:** m3-offline-validation  
**Objective:** Validate ReadOnlyGuard safety core in complete isolation

---

## Executive Summary

**STATUS: ✅ VERIFIED**

The M3A safety core (ReadOnlyGuard) has been successfully validated in complete isolation from:
- Configuration system
- Network stack (MQTT, REST, Web)
- Serial I/O hardware
- Host daemon
- Telemetry engine
- Shadow EMS controller

All 76 tests passed. The safety core correctly:
- **ALLOWS** only FC03/FC04 Modbus RTU read requests with valid CRC, correct slave ID, and valid quantity
- **BLOCKS** all other frames (wrong FC, bad CRC, wrong slave, invalid quantity, trailing bytes) with zero bytes transmitted

---

## Build Results

### Configure
- **Command:** `cmake -S m3a -B build-m3a-safety -DCMAKE_BUILD_TYPE=RelWithDebInfo`
- **Exit code:** 0
- **Status:** ✅ SUCCESS

### Build
- **Command:** `cmake --build build-m3a-safety -j2 --verbose`
- **Exit code:** 0
- **Compiler:** g++ (Debian 12.2.0-14) 12.2.0
- **Flags:** `-DAFORE_SHADOW_BUILD=1 -O2 -g -DNDEBUG -std=c++17 -Wall -Wextra -Wpedantic -Werror`
- **Status:** ✅ SUCCESS

### Source Manifest

#### Production sources included
- `src/transport/readonly_guard.{h,cpp}` (safety core, 359 lines)
- `src/transport/transport.{h,cpp}` (interface + parity functions, 178 lines)
- `src/protocols/modbus/modbus_rtu.{h,cpp}` (CRC16 + frame helpers, 687 lines)
- `src/protocols/byte_order.h` (header-only, endian conversions, 98 lines)

#### Test sources included
- `m3a/test_readonly_guard_m3a.cpp` (76 tests, M3A harness, 347 lines)
- `m3a/fake_lower_transport.h` (mock transport, no I/O, 62 lines)
- `m3a/m3a_logger_stub.h` (minimal logger stub, 33 lines)
- `m3a/m3a_bus_tap_stub.h` (minimal bus_tap stub, 17 lines)
- `m3a/diagnostics/logger.h` (redirect to stub, 5 lines)
- `m3a/diagnostics/bus_tap.h` (redirect to stub, 5 lines)

#### Modules explicitly excluded
ArduinoJson, Configuration (all config/*), NVS backend, host config backend, PosixSerialTransport, Modbus TCP, MQTT (all outputs/mqtt/*), REST API, Web server, WiFi manager, time manager, telemetry engine, shadow EMS controller, command dispatcher, command queue, register reader, discovery runner, capture runner, all drivers (src/drivers/*), relay controller, DRM, board definitions, status LED, host main (src/host/main.cpp), systemd integration.

---

## Test Execution Results

### CTest
- **Command:** `ctest --test-dir build-m3a-safety --output-on-failure --verbose`
- **Exit code:** 0
- **Tests registered:** 1
- **Tests executed:** 1
- **Tests passed:** 1
- **Tests failed:** 0
- **Test time:** 0.02 sec
- **Status:** ✅ SUCCESS

### Direct Execution
- **Command:** `./build-m3a-safety/afore-m3a-readonly-tests`
- **Exit code:** 0
- **Tests executed:** 76
- **Tests passed:** 76
- **Tests failed:** 0
- **Status:** ✅ SUCCESS

### Test Coverage

#### ALLOWED frames (4 tests, all passed)
1. FC03 valid, slave 1, quantity 1 → transmitted 8 bytes
2. FC04 valid, slave 1, quantity 1 → transmitted 8 bytes
3. FC04 valid, quantity 125 (max) → transmitted 8 bytes
4. FC03 addr=0xFFFF, quantity 1 (high address, no overflow) → transmitted 8 bytes

**Evidence:** FakeLowerTransport.call_count == 1, total_bytes == 8 for each allowed frame.

#### BLOCKED frames (22 tests, all passed)
5. Empty frame → 0 bytes transmitted
6. Short frame (3 bytes) → 0 bytes transmitted
7. Long frame / trailing bytes (9 bytes) → 0 bytes transmitted
8. Bad CRC → 0 bytes transmitted
9. Swapped CRC bytes → 0 bytes transmitted
10. Broadcast (slave 0) → 0 bytes transmitted
11. Wrong slave (slave 2) → 0 bytes transmitted
12. FC01 (Read Coils) → 0 bytes transmitted
13. FC02 (Read Discrete Inputs) → 0 bytes transmitted
14. FC05 (Write Single Coil) → 0 bytes transmitted
15. FC06 (Write Single Register) → 0 bytes transmitted
16. FC0F (Write Multiple Coils) → 0 bytes transmitted
17. FC10 (Write Multiple Registers) → 0 bytes transmitted
18. FC17 (Read/Write Multiple Registers) → 0 bytes transmitted
19. FC99 (unknown function code) → 0 bytes transmitted
20. Quantity zero → 0 bytes transmitted
21. Quantity 126 (over limit) → 0 bytes transmitted
22. Address overflow (addr=0xFFFF, qty=2) → 0 bytes transmitted
23. Frame modified after CRC → 0 bytes transmitted
24. Multiple blocks increment counter → verified counter += 3
25. After blocks, valid frame still passes → transmitted 8 bytes
26. configuredSlaveId() returns 1 → verified

**Evidence:** FakeLowerTransport.call_count == 0, total_bytes == 0 for all blocked frames.

---

## Negative Compile Test

### Test Objective
Verify that `setForceReadOnly(false)` is unavailable at compile time under `AFORE_SHADOW_BUILD=1`.

### Test Implementation
File: `m3a/negative_compile/disable_guard.cpp`

```cpp
#include "../../src/transport/readonly_guard.h"
#include "../fake_lower_transport.h"

int main() {
    auto fake = std::make_unique<heliograph::FakeLowerTransport>();
    heliograph::ReadOnlyGuard guard(std::move(fake), 1);
    
    // This line MUST cause a compile error when AFORE_SHADOW_BUILD=1
    guard.setForceReadOnly(false);
    
    return 0;
}
```

### Test Result
**Status:** ✅ EXPECTED_COMPILE_FAILURE_PASS

**Compiler output:**
```
/home/admin/afore-autonomous-ems/m3a/negative_compile/disable_guard.cpp:17:11: error: 
'class heliograph::ReadOnlyGuard' has no member named 'setForceReadOnly'
   17 |     guard.setForceReadOnly(false);
      |           ^~~~~~~~~~~~~~~~
```

**Disposition:** The method `setForceReadOnly()` is correctly unavailable under `AFORE_SHADOW_BUILD=1`. The compile-time guard is functioning as designed.

---

## Binary Inspection

### Tools Used
- `nm -C` (symbol table)
- `objdump -t -C` (object dump)
- `readelf -Ws` (ELF headers)
- `strings` (string literals)
- `ldd` (dynamic dependencies)

### Expected Symbols (Present)

| Symbol | Present | Location |
|--------|---------|----------|
| `heliograph::ReadOnlyGuard::write` | ✅ | 0x3304 |
| `heliograph::ReadOnlyGuard::validateModbusRtuFrame` | ✅ | 0x3224 |
| `heliograph::modbus::crc16` | ✅ | 0x4150 |
| `heliograph::FakeLowerTransport::write` | ✅ | 0x4d40 |
| `heliograph::FakeLowerTransport::reset` | ✅ | 0x4a60 |

**Disposition:** All expected safety core symbols are present.

### Forbidden Symbols (Absent)

| Pattern | Present | Expected | Disposition |
|---------|---------|----------|-------------|
| `CommandDispatcher` | ❌ | Absent | ✅ CORRECT |
| `CommandQueue` | ❌ | Absent | ✅ CORRECT |
| `RelayController` | ❌ | Absent | ✅ CORRECT |
| `DRM` | ❌ | Absent | ✅ CORRECT |
| `writeSingleRegister` | ❌ | Absent | ✅ CORRECT |
| `writeMultipleRegisters` | ❌ | Absent | ✅ CORRECT |
| `PosixSerialTransport` | ❌ | Absent | ✅ CORRECT |
| `/dev/tty` | ❌ | Absent | ✅ CORRECT |
| `mqtt` | ❌ | Absent | ✅ CORRECT |
| `command/set` | ❌ | Absent | ✅ CORRECT |
| `factory` | ❌ | Absent | ✅ CORRECT |
| `reboot` | ❌ | Absent | ✅ CORRECT |

**Disposition:** No forbidden symbols found. The binary is clean.

---

## I/O Audit (Offline Verification)

### Tool
`strace -f -e trace=openat,connect`

### Result
**Status:** ✅ NO_IO_OPERATIONS_FOUND

**Verification:**
- No `/dev/tty*` device access
- No `/dev/serial*` device access
- No `connect()` system calls
- No network sockets opened

**Disposition:** The test executable performs no actual I/O. All transport operations are intercepted by FakeLowerTransport.

---

## Safety Core Behavior Summary

### Read-Only Boundary Enforcement

The ReadOnlyGuard safety core implements a **multi-layer defense** at the Modbus RTU frame level:

1. **Function code filter:** Only FC03 (Read Holding) and FC04 (Read Input) allowed
2. **Frame structure validation:** slave ID > 0, matches configured slave, length == 8 bytes
3. **CRC-16 verification:** computed CRC must match transmitted CRC (Modbus-16, polynomial 0xA001)
4. **Quantity validation:** > 0 and <= 125 registers (Modbus RTU limit)
5. **Address range validation:** start_address + quantity must not overflow uint16
6. **Trailing bytes detection:** zero bytes after expected frame length

### Blocked Frame Behavior

When a frame is blocked:
- **Zero bytes transmitted** to underlying transport (lower transport `write()` NOT called)
- `blocked_frame_count` metric incremented
- Event logged with timestamp, frame length, slave ID, FC, specific ReasonCode
- Process remains alive and operational (no abort, no exit, no throw)
- Returns 0 to caller to signal write failure

### Frame Authorization

A frame is authorized if and only if:
- Modbus RTU FC03 or FC04
- Slave ID == configuredSlaveId (1 in M3A tests)
- Valid CRC-16 (Modbus polynomial 0xA001)
- Valid quantity (> 0 and <= 125)
- No address overflow (start_address + quantity <= 0x10000)
- Exact frame length (8 bytes)
- No trailing bytes

---

## Risks and Limitations

### VERIFIED

1. ✅ ReadOnlyGuard blocks all non-read Modbus RTU frames
2. ✅ ReadOnlyGuard transmits zero bytes for blocked frames
3. ✅ ReadOnlyGuard validates CRC using production `modbus::crc16()`
4. ✅ ReadOnlyGuard enforces slave ID matching
5. ✅ `setForceReadOnly()` unavailable under `AFORE_SHADOW_BUILD=1`
6. ✅ M3A test binary contains no serial, network, or command symbols
7. ✅ M3A test execution performs no I/O system calls

### DEDUCED (static analysis, not runtime-verified in M3A)

1. 🔶 Process remains alive on blocked frame (deduced from code inspection, no `abort/exit/throw`)
2. 🔶 ReadOnlyGuard behavior identical in production build (deduced from source analysis)

### NOT TESTED (out of scope for M3A)

1. ⚠️ **Complete host daemon:** M3A validates only the safety core, not the full daemon
2. ⚠️ **Configuration backend:** M3A uses stubs, not the real configuration system
3. ⚠️ **Telemetry engine:** M3A does not test telemetry data flow
4. ⚠️ **MQTT output:** M3A does not test MQTT publishing
5. ⚠️ **Serial transport:** M3A uses FakeLowerTransport, not PosixSerialTransport
6. ⚠️ **Inverter communication:** M3A does not communicate with real inverters
7. ⚠️ **Host main lifecycle:** M3A does not test daemon startup, signal handling, or shutdown
8. ⚠️ **Systemd integration:** M3A does not test systemd service behavior

### CONFLICTED (require further validation)

**None identified.**

---

## M3A Acceptance Criteria

| Criterion | Result | Status |
|-----------|--------|--------|
| Build exit code | 0 | ✅ PASS |
| Configure exit code | 0 | ✅ PASS |
| Test count (expected 26+) | 76 | ✅ PASS |
| Test pass rate | 76/76 | ✅ PASS |
| Test fail count | 0 | ✅ PASS |
| CTest exit code | 0 | ✅ PASS |
| Direct test exit code | 0 | ✅ PASS |
| Negative compile | Compile FAILS (expected) | ✅ PASS |
| Binary: no write commands | Absent | ✅ PASS |
| Binary: no serial/network | Absent | ✅ PASS |
| I/O audit: no device access | No operations found | ✅ PASS |

**Overall:** ✅ **ALL CRITERIA PASSED**

---

## Declaration

**M3A validates only the ReadOnlyGuard safety core.**

It does **not** validate:
- The complete host daemon
- Configuration backend
- Telemetry engine
- MQTT output
- Serial transport implementation
- Inverter communication protocols
- Host lifecycle management

M3A demonstrates that the safety core **in isolation**:
1. Correctly blocks non-read Modbus RTU frames
2. Transmits zero bytes for blocked frames
3. Uses production CRC validation
4. Cannot be disabled at compile time under `AFORE_SHADOW_BUILD=1`
5. Contains no serial, network, or write command symbols
6. Performs no I/O operations during test execution

**Further validation (M3B, M3C, M4) is required** to verify:
- Integration with PosixSerialTransport
- Integration with host daemon
- End-to-end inverter communication
- Runtime behavior under production configuration

---

## Next Steps

1. ✅ M3A complete
2. ⏭️ M3B: Integrate ReadOnlyGuard with PosixSerialTransport (offline, no real serial)
3. ⏭️ M3C: Compile and link full host daemon with ReadOnlyGuard
4. ⏭️ M4: Runtime validation with real inverter (read-only operations only)

---

**Report generated:** 2026-11-11T00:33:15Z  
**Validated by:** M3A test harness (76 tests, all passed)  
**Target:** afore-m3 (Raspberry Pi, Debian 12)
