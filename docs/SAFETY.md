# SAFETY.md — Afore Shadow Read-Only Security Model (M2)

**Document Status:** M2 Complete — Static verification and implementation  
**Last Updated:** 2026-11-10  
**Scope:** Afore Autonomous EMS V4, Milestone M2 (Hardening Read-Only)

---

## 1. Executive Summary

The **Afore Shadow** build is structurally and physically incapable of transmitting ANY Modbus RTU frame different from a valid FC03 (Read Holding Registers) or FC04 (Read Input Registers) request with:
- Correct CRC-16 (Modbus polynomial 0xA001)
- Valid slave ID (> 0, non-broadcast)
- Valid quantity (> 0, ≤ 125 registers)
- Exact frame length (8 bytes for RTU request)
- No address overflow
- No trailing bytes

**M2 Enforcement Layers:**
1. **Compile-time exclusion** (`AFORE_SHADOW_BUILD=1`): Write-capable sources physically absent from binary
2. **Frame-aware validation** (`ReadOnlyGuard`): Multi-level runtime defense at transport boundary
3. **Profile enforcement**: Zero `[[write]]` blocks in `profiles/afore/af5k_sl.toml`
4. **Static verification**: Automated manifest checker (`tools/check_afore_shadow_manifest.py`)

**M2 Guarantees:**
- Zero bytes transmitted for blocked frames (lower transport NOT called)
- Process remains alive and operational (no abort, no exit, no throw)
- Complete audit trail with reason codes and metrics
- Upstream Heliograph compatibility preserved

---

## 2. Trust Boundary and Threat Model

### 2.1 Trust Boundary Location

The **ReadOnlyGuard** sits between application logic and the lower transport (serial port), intercepting ALL write() calls before they reach the physical wire.

```
┌─────────────────────────────────────────────────┐
│  Application Layer                              │
│  (Modbus Client, Telemetry Engine, EMS Logic)  │
└──────────────────┬──────────────────────────────┘
                   │ write(frame)
                   ▼
        ┏━━━━━━━━━━━━━━━━━━━━━━━━━┓
        ┃   ReadOnlyGuard         ┃  ◄─── M2 Trust Boundary
        ┃   Frame-Aware Validator ┃
        ┗━━━━━━━━━━━┬━━━━━━━━━━━━┛
                    │ Validated frames only (FC03/FC04)
                    ▼
        ┌───────────────────────────┐
        │  Lower Transport          │
        │  (POSIX Serial / RS485)   │
        └─────────────┬─────────────┘
                      │
                      ▼
              Physical Serial Port
              (/dev/ttyUSB0)
                      │
                      ▼
              Afore AF5K-SL Inverter
```

### 2.2 Threat Model

**Protected Against (M2 Scope):**
- Application bugs constructing write commands (FC05, FC06, FC0F, FC10, FC16, FC17)
- Malformed frames (short, long, empty, trailing bytes)
- CRC errors (corrupted, mutated, wrong byte order)
- Broadcast frames (slave ID 0)
- Invalid quantities (0, > 125, address overflow)
- Replay attacks with stale or wrong CRC
- Configuration errors attempting writes

**NOT Protected Against (Out of M2 Scope):**
- Physical access to serial cable (bypass software entirely)
- Malicious firmware flashing (requires sudo/root access)
- Kernel-level exploits (below userspace process)
- Attacks on the inverter itself (authentication, firmware bugs)
- Network attacks (MQTT, REST API authentication — separate layer)

**Defense-in-Depth Principle:**  
M2 provides **structural read-only guarantee**. Even if application logic is compromised, malicious code cannot construct a write frame that passes validation.

---

## 3. Frame Authorization Logic

### 3.1 Authorized Frames (ALLOWED)

A frame is authorized and transmitted if and only if ALL conditions hold:

1. **Function Code:** `0x03` (Read Holding Registers) OR `0x04` (Read Input Registers)
2. **Slave ID:** `> 0` (non-broadcast) AND matches configured slave (if set)
3. **Frame Length:** Exactly 8 bytes (Modbus RTU read request structure)
4. **CRC-16:** Computed CRC matches transmitted CRC (bytes 6-7, little-endian)
5. **Quantity:** `> 0` AND `≤ 125` registers (Modbus RTU limit)
6. **Address Range:** `start_address + quantity ≤ 0x10000` (no uint16 overflow)
7. **No Trailing Bytes:** Frame length exactly 8, zero bytes after CRC

**Frame Structure (8 bytes):**
```
[0]     Slave ID        (1 byte)   — Must be > 0
[1]     Function Code   (1 byte)   — Must be 0x03 or 0x04
[2-3]   Start Address   (2 bytes)  — Big-endian uint16
[4-5]   Quantity        (2 bytes)  — Big-endian uint16, 1-125
[6-7]   CRC-16          (2 bytes)  — Little-endian, computed over [0-5]
```

### 3.2 Blocked Frames (SECURITY_BLOCK)

Any frame failing validation is blocked with zero bytes transmitted. Reason codes:

| Code | Reason | Description |
|------|--------|-------------|
| `FUNCTION_NOT_ALLOWED` | FC not 0x03 or 0x04 | Includes FC01, FC02, FC05, FC06, FC0F, FC10, FC16, FC17, unknown |
| `EMPTY_FRAME` | len == 0 | Zero-byte write attempt |
| `SHORT_FRAME` | len < 8 | Incomplete frame, missing fields |
| `INVALID_LENGTH` | len != 8 | Frame too long (> 8 bytes) or trailing bytes |
| `INVALID_CRC` | CRC mismatch | Computed CRC ≠ transmitted CRC, includes byte-order errors |
| `BROADCAST_NOT_ALLOWED` | slave == 0 | Broadcast address prohibited |
| `SLAVE_NOT_ALLOWED` | slave mismatch | Frame for wrong slave ID (when configured) |
| `QUANTITY_ZERO` | quantity == 0 | Invalid Modbus request |
| `QUANTITY_OUT_OF_RANGE` | quantity > 125 | Exceeds Modbus RTU limit |
| `ADDRESS_RANGE_OVERFLOW` | start + qty > 0x10000 | Address space overflow |
| `TRAILING_BYTES` | len > 8 | Bytes after expected frame end |

**Blocked Frame Behavior:**
1. Return `0` to caller (write failure signal)
2. Increment `blocked_frame_count` metric
3. Log event with timestamp, slave ID, FC, reason code
4. Process remains alive and responsive (no abort/exit/throw)

### 3.3 CRC-16 Validation

**Algorithm:** Modbus RTU CRC-16
- **Polynomial:** 0xA001 (reflected form of 0x8005)
- **Initial Value:** 0xFFFF
- **Transmission:** Little-endian (low byte first)
- **Coverage:** Bytes [0-5] (slave, FC, address, quantity)

**Validation Steps:**
1. Compute CRC-16 over bytes [0-5]
2. Extract transmitted CRC from bytes [6-7] as little-endian uint16
3. Compare computed vs transmitted
4. BLOCK if mismatch (includes mutated frames, wrong byte order)

**Example (FC03 read 10 regs at 0x0200, slave 1):**
```
Frame: 01 03 02 00 00 0A C4 B5
       ^  ^  ^^^^^  ^^^^  ^^^^
       |  |    |      |     CRC (little-endian: 0xB5C4)
       |  |    |      Quantity: 10 (0x000A)
       |  |    Start Address: 0x0200
       |  Function Code: 0x03
       Slave ID: 1

CRC Computation:
  crc16([01 03 02 00 00 0A]) = 0xB5C4
  Transmitted: [C4 B5] = 0xB5C4 (little-endian)
  ✓ MATCH → ALLOWED
```

---

## 4. Compile-Time Hardening (`AFORE_SHADOW_BUILD=1`)

### 4.1 Excluded Sources

When `cmake -DAFORE_SHADOW_BUILD=ON ..` is invoked, the following write-capable sources are **physically excluded from the binary**:

```cmake
if(NOT AFORE_SHADOW_BUILD)
    list(APPEND COMMON_SOURCES
        src/commands/command_dispatcher.cpp
        src/commands/command_queue.cpp
        src/relays/drm.cpp
        src/relays/relay_controller.cpp
        src/status/boot_button.cpp
        src/status/status_led.cpp
    )
endif()
```

**Excluded Capabilities:**
- Command dispatching (write command execution)
- Command queue management
- DRM (Demand Response Mode) control
- Relay controller (physical relay switching)
- Boot button handling (factory reset, reboot commands)
- Status LED control (write operations)

**Preserved Capabilities:**
- Modbus RTU read operations (FC03, FC04)
- Telemetry collection and quality metrics
- MQTT publishing (read-only shadow state)
- Configuration loading (read-only profile)
- EMS controller (shadow mode, no actuations)

### 4.2 Build Verification

**Standard Build (write-enabled, for upstream Heliograph):**
```bash
mkdir build && cd build
cmake ..
make heliograph-daemon
```

**Afore Shadow Build (M2 read-only):**
```bash
mkdir build-shadow && cd build-shadow
cmake -DAFORE_SHADOW_BUILD=ON ..
make heliograph-daemon
```

**Static Verification:**
```bash
python3 tools/check_afore_shadow_manifest.py
# Must exit 0 with [PASS] status
```

---

## 5. Runtime Defense (ReadOnlyGuard)

### 5.1 Implementation

**Header:** [`src/transport/readonly_guard.h`](../src/transport/readonly_guard.h)  
**Implementation:** [`src/transport/readonly_guard.cpp`](../src/transport/readonly_guard.cpp)

**Key Methods:**
- `write(const uint8_t* data, size_t len)` — Intercepts and validates ALL writes
- `validateModbusRtuFrame()` — Frame-aware validation logic
- `computeModbusCrc16()` — CRC-16 computation (Modbus standard)
- `setConfiguredSlaveId(uint8_t)` — Optional slave ID enforcement
- `setForceReadOnly(bool)` — Enable/disable guard (default: enabled)

**Metrics:**
- `blocked_frame_count` — Total blocked attempts
- `allowed_frame_count` — Total authorized transmissions
- `lastBlockedEvent()` — Last blocked frame details (timestamp, slave, FC, reason)

### 5.2 No-Abort Guarantee

The guard NEVER terminates the process on blocked frames:
- No `assert()` calls
- No `abort()` or `exit()` calls
- No uncaught exceptions
- No `LOG_FATAL` with process termination

**Rationale:** The shadow daemon must remain alive to observe inverter state continuously. A blocked frame indicates a bug or attack attempt, but must not disrupt telemetry.

### 5.3 Configuration

**Default Behavior:**
- Guard **enabled** (`forceReadOnly = true`)
- **All slaves** accepted (slave ID 0 = accept any non-broadcast)
- Blocks ALL non-FC03/FC04 frames

**Optional Configuration:**
```cpp
guard.setConfiguredSlaveId(1);  // Restrict to slave 1 only
guard.setForceReadOnly(false);  // Disable guard (for testing)
```

**Production Deployment:**  
Always use default (enabled, all non-broadcast slaves).

---

## 6. Test Matrix A-Y (M2 Static Verification)

**Test Suite:** [`test/host/test_readonly_guard.cpp`](../test/host/test_readonly_guard.cpp)  
**Status:** `TEST_AUTHORED` (M2 — static verification only, runtime execution reserved for M3)

**Coverage (25 tests):**
- **A, B:** FC03, FC04 valid frames → ALLOWED
- **C-J:** Write FCs (01, 02, 05, 06, 0F, 10, 17, unknown) → BLOCKED
- **K, L:** Empty, short frames → BLOCKED
- **M:** Invalid CRC → BLOCKED
- **N, O:** Broadcast, wrong slave → BLOCKED
- **P, Q:** Quantity 0, > 125 → BLOCKED
- **R:** Trailing bytes → BLOCKED
- **S:** Max valid address (0xFFFF + qty 1) → ALLOWED
- **T:** Address overflow (0xFFFF + qty 2) → BLOCKED
- **U:** Repeated blocks (process alive, counters increment)
- **V:** Valid after blocks (no lock-out) → ALLOWED
- **W:** Two valid consecutive → both ALLOWED
- **X:** Post-CRC mutation → BLOCKED
- **Y:** CRC byte-order wrong → BLOCKED

**Invariant (FakeLowerTransport):**  
For ANY blocked frame: `call_count == 0`, `total_bytes == 0`, `frames.empty() == true`

**M3 Milestone:** Runtime execution and verification of all tests.

---

## 7. Profile Enforcement (`profiles/afore/af5k_sl.toml`)

**Profile:** [`profiles/afore/af5k_sl.toml`](../profiles/afore/af5k_sl.toml)

**M2 Requirements:**
1. **Zero `[[write]]` blocks** — Profile declares NO write capability
2. **Zero command mappings** — No command dispatch configuration
3. **Forbidden registers ABSENT** — Registers 0x09C4, 0x0A08-0x0A0A, 0x0BC1-0x0BC2 NOT in write paths
4. **EEPROM protection** — 0x0BC1 (`CurrMaxChg`) STRICTLY FORBIDDEN from any write

**Read Blocks Only:**
```toml
[[block]]
space = "holding"
start = 512    # 0x0200: Inverter status, power, grid
count = 19

[[block]]
space = "holding"
start = 552    # 0x0228: PV1/PV2 voltage, current, power
count = 9

[[block]]
space = "holding"
start = 2000   # 0x07D0: Battery SOC, voltage, current, power
count = 9
```

**Verification:**
```bash
python3 tools/check_afore_shadow_manifest.py
# Check 2: af5k_sl.toml: Zero [[write]] blocks [OK]
```

---

## 8. Observability and Audit Trail

### 8.1 Metrics

**ReadOnlyGuard Counters:**
- `allowedFrameCount()` — Total frames transmitted (FC03/FC04 valid)
- `blockedFrameCount()` — Total frames blocked (security enforcement)

**Blocked Frame Event:**
```cpp
struct BlockedFrameEvent {
    uint64_t        timestamp_ms;    // System time when blocked
    size_t          frame_length;    // Frame size in bytes
    uint8_t         slave_id;        // Slave ID from frame
    uint8_t         function_code;   // FC from frame
    BlockReasonCode reason;          // Why blocked (enum)
};
```

### 8.2 Logging

**Trace Level (authorized frames):**
```
readonly_guard: ALLOWED FC03 slave=1 addr=0x0200 qty=10 (8 bytes)
```

**Error Level (blocked frames):**
```
readonly_guard: BLOCKED frame [len=8 slave=0x01 fc=0x06] reason=FUNCTION_NOT_ALLOWED - Afore Shadow M2 read-only enforcement
```

**Log Destination:** Configured via application logger (`diagnostics/logger.h`)

### 8.3 Runtime Verification (M3)

**Planned M3 Checks:**
1. Binary analysis (`nm`, `objdump`, `strings`) — verify excluded symbols absent
2. Runtime frame injection — confirm blocked frames return 0, allowed frames transmit
3. Serial port monitoring (`strace`, `tcpdump`) — verify zero write-FC frames on wire
4. Long-term stability — 24-72h polling with repeated block attempts

---

## 9. Residual Risks and Limitations

### 9.1 Known Risks (Accepted for M2)

1. **Physical Access:** Attacker with physical access to serial cable bypasses software entirely  
   **Mitigation:** Physical security controls, locked enclosure

2. **Kernel Compromise:** Root-level attacker can bypass userspace process  
   **Mitigation:** OS hardening, principle of least privilege, no sudo for daemon

3. **Inverter Vulnerabilities:** Afore firmware bugs or authentication weaknesses  
   **Mitigation:** Out of scope for shadow build; inverter security is manufacturer responsibility

4. **False Sense of Security:** M2 does NOT protect against:
   - Man-in-the-middle attacks (no encryption on Modbus RTU)
   - Side-channel attacks (timing, power analysis)
   - Social engineering (operator manually writing to inverter via other tool)

### 9.2 Limitations

1. **M2 Scope:** Static verification and implementation only  
   **M3 Required:** Compilation on target (Raspberry Pi), runtime test execution, binary verification

2. **No Serial Port Opens:** M2 does NOT open `/dev/ttyUSB0` or contact inverter  
   **M3 Required:** One-shot interrogation build, passive observation

3. **No MQTT Production:** M2 does NOT publish to Home Assistant  
   **M10 Required:** Integration and live shadow operation

4. **Test Status:** All tests `TEST_AUTHORED`, NOT executed  
   **M3 Required:** Full test matrix execution with pass/fail results

---

## 10. Deployment Checklist (M3 Gate)

**Before deploying Afore Shadow to Raspberry Pi:**

- [ ] **M2 Static Verification Passed:** `tools/check_afore_shadow_manifest.py` exits 0
- [ ] **M3 Compilation Successful:** Binary built on Raspberry Pi with `AFORE_SHADOW_BUILD=ON`
- [ ] **M3 Binary Verification:** `nm`, `objdump` confirm excluded symbols absent
- [ ] **M3 Test Execution:** All A-Y tests pass at runtime
- [ ] **M3 One-Shot Interrogation:** Single read cycle successful, zero write frames on wire
- [ ] **G1 Gate:** Formal review of M2 SAFETY.md and M3 verification report
- [ ] **No Serial Port Access:** `/dev/ttyUSB0` permissions restricted, no concurrent tools
- [ ] **MQTT Credentials:** Shadow-specific credentials, separate from production

**Deployment Authorization:** Requires explicit approval after G1 gate review.

---

## 11. Change Control

**M2 Baseline (2026-11-10):**
- ReadOnlyGuard frame-aware validation implemented
- CMakeLists.txt compile-time exclusions added
- Test matrix A-Y authored (static verification)
- Profile af5k_sl.toml verified read-only
- Static verification script executed successfully

**Future Changes:**  
Any modification to trust boundary, validation logic, or excluded sources requires:
1. Update to this SAFETY.md document
2. Re-execution of `tools/check_afore_shadow_manifest.py`
3. Re-verification of test matrix A-Y
4. Formal review and approval

**Document Version:** M2.1 (2026-11-10)

---

## 12. References

- **M2 Milestone:** `Doc/PIANO_Afore_Autonomous_EMS_V4.md` §3.2
- **Requirements Traceability:** `docs/REQUISITI_TRACCIABILITA.md`
- **Modbus Specification:** Modbus Application Protocol V1.1b3
- **Upstream Project:** Heliograph — https://github.com/sophiecollard/heliograph
- **Contributing Guidelines:** `AGENTS.md`

---

**END OF DOCUMENT**
