# Hardware bring-up plan

First firmware: **V5.1 USB Diagnostics**, proposed environment `waveshare-1ch-v51-diag`. Use the existing pioarduino pin with a dedicated C++17 source list. The diagnostic is standalone, USB-only, and always off-only for AUX. Compile identity, USB logging, reset reason, watchdog, memory stats, read-only RTC and board-safe initialization. Leave RS485 nontransmitting, all network services off, and no config mutation path. Emit reset and firmware/build ID at startup plus a 1 Hz bounded heartbeat even with no USB terminal. Test state below is future work.

## G1. Correlate COM10 and identify the specimen

The ROM/USB portion was completed on 2026-11-18 using the isolated bench unit. Keep the recorded [ROM evidence](02_HARDWARE_BASELINE.md) and finish the visual label, PCB revision and antenna correlation. Repeat the read if the specimen changes or its identity cannot be correlated.

1. Verify the label, revision and antenna variant, USB cable, expected direct 5 V source and no load on AUX. Disconnect RS485 from the inverter and leave AUX terminals open before any reset or bootloader operation.
2. On Windows run `[System.IO.Ports.SerialPort]::GetPortNames()`, `python -m serial.tools.list_ports -v` and check PnP USB Serial/JTAG enumeration. Correlate USB instance with the physical board and note any port change in ROM mode. Record board markings and redacted USB output.
3. Where the port and disconnected bench setup are confirmed, use esptool 5.6.0 in ROM with `--no-stub --before no-reset --after no-reset` after manual entry into download mode and the confirmed port (expected COM10). Start with `flash-id`, which reports chip features and flash ID, then check only public chip data needed for the baseline. Record verbatim revision, embedded PSRAM feature and detected flash size with identifiers redacted. Leave device in agreed safe state, and preserve its contents. If COM10 is absent, stop hardware progress and continue the G2 build work offline.

For the ROM step, the proposed PowerShell invocation after manual entry and physical isolation is `esptool --chip esp32s3 --port COM10 --before no-reset --after no-reset --no-stub flash-id`. Substitute the correlated port only if the same unit enumerates on a different number.

## G2. Reproducible compile, no upload

In a future isolated checkout, verify PIO and package inventory, provision pinned Xtensa compiler/Arduino SDK as needed, then record exact versions. Add `waveshare-1ch-v51-diag` with explicitly selected files and validated flash/PSRAM overrides `16MB`, `qio_opi`, CDC-on-boot and no alternate pin selection. Choose the partition map only after inspecting original layout and defining recovery. Example build once target exists: `pio run -e waveshare-1ch-v51-diag -v`. Produce two clean builds with controlled paths and tool versions, compare image/partition outputs and explain any non-deterministic build fields. Record compiler and framework packages, artifact hashes, ELF/map analysis and size. Image metadata can be inspected with `esptool image-info <firmware.bin>` without opening serial. Use matching S3 targets and verify app slot bounds.

## G3. Bench-only diagnostic qualification

Before an authorized flash in a subsequent mission, verify the image and off-only composition [against G2](10_MASTER_IMPLEMENTATION_PLAN.md). Capture a readback recovery image if supported and authorized, locally protect it because it can contain secrets, record original partition table and hash, and validate recovery procedure. A previous image of unknown safety is eligible for bench recovery only with RS485 and relay terminals disconnected. If recovery/partition plan is unresolved, keep G3 open.

- Verify USB boot logs, board ID and variant, runtime chip/memory capability, clock, reset cause, internal heap and largest block, PSRAM total/free and bounded allocation test. The firmware starts with outputs disabled and no GPIO capable of driving AUX high.
- With isolated bench instruments, run at least 20 cold starts and 20 each manual resets, controlled brownout recoveries, injected watchdog and panic resets. Exercise idle heartbeat with USB connected and disconnected. Use a separately qualified diagnostic fixture hook for deliberate reset faults. Record GPIO47, coil node and NO continuity at adequate trigger bandwidth, starting before power rise. Expect zero contact closures and zero electrical excursions outside the OFF envelope. Capture GPIO21 and A/B for zero diagnostic TX. Any unexpected pulse is a stop condition.
- Read PCF85063 control/calendar bytes at 0x51 using register-pointer reads only. Record reply, oscillator-stop, BCD/UTC decode, retained time across a controlled outage and whether backup is fitted. Missing/invalid RTC must allow continued monotonic operation. Avoid installing demo firmware, which contains control paths.
- Run isolated idle diagnostics ≥30 min and verify heartbeat, memory stability, no unexpected reset and usable reset reason on deliberate tests. Save serial log, reset count and scope captures beside the gate evidence.

## G4. RS485 qualification, separately authorized

The initial register reference is the vendor worksheet `运行信息04` in the v3.4 workbook in `Doc/`, cross-checked against the working-tree TOML. Worksheet rows below are one-indexed. Verify against the specific inverter firmware on G4/G5 and document MSW/LSW evidence.

| Vendor row | FC04 address | Signal | Raw format and normalization |
|---|---|---|---|
| 39 | 507 | Grid voltage | U16 × 0.1 V |
| 45 | 513 | Grid frequency | U16 × 0.01 Hz |
| 51 | 522–523 | Inverter AC active power | S32 W. Validate phase and direction in situ. |
| 55 | 529–530 | Grid meter active power | S32 W, vendor positive import and negative export. |
| 67 | 553–554 | Total PV | U32 W |
| 163 | 2002 | Battery SOC | U16 %. Reject values outside 0–100. |
| 165 | 2004 | Battery voltage | U16 × 0.1 V |
| 166 | 2005 | Battery current | S16 × 0.01 A, vendor positive discharge. TOML still uses +0.01. Decide canonical sign at the single decode boundary and test it. |
| 168 | 2007–2008 | Battery power | S32 W, vendor positive discharge. TOML negates to charge-positive. |
| 190 | 2515 | Heatsink temperature | S16 × 0.1 °C |

The profile proposes 507/24, 552/9, 2000/9 and 2515/1 blocks. Validate both endpoints and device acceptance before adding each block. Narrow or split around unsupported addresses. Keep ambiguous or configuration rows outside the release polling allowlist.

1. Use a known isolated dummy responder first. One master, documented A/B/GND and termination, scope GPIO21 and differential bus, capture request/response, turnarounds and idle/noise. Validate FC04 frames, rejected frames, corrupted CRC, timeout/backoff and reset. Ensure no arbitrary-transmit interface.
2. Only after dummy G4 evidence and explicit physical-read authorization, map the actual inverter/other participants and ensure one master. Resolve Solarman or logger coexistence through topology evidence, with no service or wiring changes without authorization. Verify candidate slave and `9600 8N1`; first permitted request is FC04 for SOC address 2002, count 1. No scan/broadcast. Compare response with inverter display and vendor reference. Stop on unexpected function, collision, bad polarity, unsafe state or sustained failures.
3. Expand to approved read-only blocks incrementally. Record valid CRC/raw bytes, changes, bounded timeouts, passive frame capture and polarity observations. Qualify signs, word order and realistic load bands in G5. Preserve terminal labels, build hash, serial assumptions, counters and trace. Disconnect the bench/plant bridge and revert to the previous qualified diagnostic state if acceptance fails.
