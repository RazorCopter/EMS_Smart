# Afore Autonomous EMS — Baseline Dossier (Milestone M0)

Date: 2026-11-13  
Status: VERIFIED AND ACTIVE  
Target Environment: Raspberry Pi Zero 2 W (Debian 13.7 Trixie aarch64)  
SSH Alias: `afore-m3` (`10.0.5.174`)

---

## 1. Executive Summary & Purpose

This baseline document captures the exact physical, operating system, and software state of the Afore Autonomous EMS testbed before implementing the OpenEMS-aligned C++17 autonomous energy manager. It serves as the reproducible foundation required by Milestone M0 of `PIANO_IMPLEMENTAZIONE_EMS_OPENEMS.md`.

---

## 2. Hardware and Environment State

### 2.1 Host Hardware
- **Model**: Raspberry Pi Zero 2 W Rev 1.0 (`cat /proc/device-tree/model`)
- **CPU**: Broadcom BCM2710A1 quad-core ARM Cortex-A53 (aarch64)
- **RAM**: 512 MB LPDDR2 SDRAM (`MemTotal: ~415 MB`, Swap: `414 MB`)
  - *Critical Constraint*: Simultaneous multi-core C++ compilation (`make -j4`) triggers Linux OOM killer / spontaneous reboot. All native host compilation on device MUST use `make -j1` or `make -j2` max, or cross-compilation.
- **OS**: Linux rasp 6.18.50+rpt-rpi-v8 #1 SMP PREEMPT Debian 1:6.18.50-1+rpt1 (2026-09-11) aarch64
- **Toolchain**: GCC 14.2.0 (`gcc (Debian 14.2.0-19) 14.2.0`), CMake 3.31.6, Python 3.13.x

### 2.2 Serial Bus & Hardware Topology
- **Physical Device**: `/dev/ttyUSB0`
- **Device Symlink**: `/dev/serial/by-id/usb-1a86_USB_Serial-if00-port0` (QinHeng Electronics CH340 serial converter)
- **Permissions**: `crw-rw---- 1 root dialout 188, 0`
- **Modbus Link Parameters**: 9600 baud, 8 data bits, no parity, 1 stop bit (8N1).
- **Target Inverter**: Afore AF5K-SL single-phase 5 kW hybrid inverter, default Modbus Slave ID = 1.
- **Bus Ownership Rule**: Exactly one single process may hold and poll `/dev/ttyUSB0` at any time.

---

## 3. Git and Working Tree State

- **Base Commit**: `1b7ed5556a83045fca6a816686cae775f25fa552`
- **Working Tree Status**:
  - Modified files preserved:
    - `deploy/web_ui.py`
    - `profiles/afore/af5k_sl.toml`
    - `src/web/assets/index_html.h`
    - `src/web/assets/setup_html.h`
  - Untracked files preserved:
    - `Doc/` (contains vendor specifications: `外部Ems调度 - En_240312.docx`, `艾伏逆变器寄存器参数表（对外）v3.4_20251119_并网机和储能机分类(2).xlsx`, `PIANO_IMPLEMENTAZIONE_EMS_OPENEMS.md`, `PROMPT_AGENTE_EMS.txt`)
    - `deploy/live_backend.py` (temporary live read-only bridge)

---

## 4. Current Service & Running Processes

- **Service Unit**: `/etc/systemd/system/afore-shadow.service`
  - Base ExecStart: `/usr/bin/python3 /opt/afore-ems/web_ui.py --port 8080`
- **Active Drop-in**: `/etc/systemd/system/afore-shadow.service.d/live.conf`
  - Overridden ExecStart:
    ```ini
    [Service]
    ExecStart=
    ExecStart=/usr/bin/python3 -u /opt/afore-ems/live_backend.py --profile /opt/afore-ems/live_profile.toml --serial /dev/serial/by-id/usb-1a86_USB_Serial-if00-port0 --baud 9600 --slave 1 --port 8080
    ```
- **Service Status**: `active (running)`, `enabled` across reboots.
- **Network Listener**: `0.0.0.0:8080` (HTTP web GUI and REST API).
- **Current Operational Role**: The service runs `live_backend.py`, which performs periodic read-only Modbus FC04/FC03 acquisition from the inverter and serves live telemetry to the web GUI. It holds `/dev/ttyUSB0`.

---

## 5. Host Test Suite Inventory

The following 11 C++ test targets are registered in `CMakeLists.txt` and verified under CTest / Unity:

1. `readonly-guard-tests` (`heliograph-tests`): Verifies memory-safe read-only Modbus frame enforcement.
2. `posix-serial-tests` (`test-posix-serial`): Tests exclusive POSIX serial device opening and timeout handling.
3. `host-config-tests` (`test-host-config`): Validates JSON configuration parsing and schema validation.
4. `afore-telemetry-tests` (`test-afore-telemetry`): Validates register addresses, scale factors, and signed decoding against official Afore sheet.
5. `endurance-7d-tests` (`test-endurance-7d`): Simulates multi-day power balance and battery degradation limits.
6. `waveshare-hal-tests` (`test-waveshare-hal`): Validates hardware abstraction layer portability.
7. `target-aware-ems-tests` (`test-target-aware-ems`): Tests state transitions for target SOC and scheduled maintenance.
8. `test-ha-replay` (`test-ha-replay`): Replays recorded Home Assistant traces through the EMS model.
9. `shadow-ems-tests` (`test-shadow-ems`): Verifies shadow control decision pipeline without hardware writes.
10. `power-balance-tests` (`test-power-balance`): 24/24 assertions passing for sign conventions (`grid > 0` import, `battery > 0` charge, `pv >= 0`).
11. `secure-provisioning-tests` (`test-secure-provisioning`): Tests secret redaction and credential protection.

---

## 6. Safety & Rollback Procedure

1. **Read-Only Invariant**: All EMS control paths remain strictly in shadow mode until formal write authorization and hardware validation protocol (Milestone M14).
2. **Serial Exclusivity**: Before activating any native C++ acquisition daemon on `/dev/ttyUSB0`, `afore-shadow.service` with `live_backend.py` must be explicitly stopped or replaced in a controlled manner.
3. **Backup**: Configuration `/etc/afore-ems/config.json` is preserved and backed up to `/etc/afore-ems/config.json.bak`.
