# Handoff for the next mission

Use this prompt when Mission 2 is authorized.

> Act as the ESP32-S3 bring-up and firmware safety engineer for Afore Autonomous EMS v5.1. Work in `C:/Users/gianv/Documents/Progetti/ESP/afore-autonomous-ems`, on the Waveshare ESP32-S3-Relay-1CH with the integrated antenna and expected Windows COM10. Read the root AGENTS.md and this dossier. Mission 2 starts by closing G1 specimen identification and executing G2 compile-only for `waveshare-1ch-v51-diag`. The physical G3 bench is conditional on explicit authorization for flash, recovery readiness and disconnected wiring. Do not proceed to G4 or the EMS port during this diagnostic slice.

## Starting evidence

- HEAD `1b7ed5556a83045fca6a816686cae775f25fa552`, branch `offline-full-product-completion`, nine tracked modified paths and nontrivial untracked work at Mission 1. Recheck index, path status and fingerprints per [assessment](01_CURRENT_STATE_ASSESSMENT.md) before edits. Use a documented isolated source baseline and preserve the original workspace.
- Official 1CH pin sources agree on UART1 TX17/RX18/RTS21, RTC SCL38/SDA39 at 0x51, and AUX GPIO47 active high. Measured OFF during shutdown and reset is still open. COM10 ROM verified ESP32-S3 rev v0.2, 16 MiB quad flash and 8 MiB embedded PSRAM on 2026-11-18. Finish visual board/antenna correlation. Runtime PSRAM and instrumented reset safety remain open. See [hardware baseline](02_HARDWARE_BASELINE.md).
- Existing `waveshare-afore-shadow` inherits the generic stack. Treat it as a legacy input and avoid building or uploading it as the diagnostic. Its feature flags and guard check do not establish binary safety.

## Execution order

1. Inventory current changes and toolchain. Confirm port/board label if available, only with AUX terminals open and RS485 disconnected. Follow [G1 and G2 instructions](09_HARDWARE_BRINGUP_PLAN.md). If absent, record enumeration output and continue build-only work. Avoid factory demo or legacy release binaries.
2. Create the proposed minimal target with only USB diagnostics, memory/reset/watchdog, read-only RTC helpers, source-allowed board HAL and off-only AUX. Keep Modbus TX, Wi-Fi, MQTT, REST, OTA, NVS writes and DRM absent. Use seed values from the approved board, keep LED status separate from physical evidence, and make heartbeat independent of USB host availability.
3. Run two clean compile-only passes, capture versions, compile/link allowlist, map/ELF/nm, partition layout, metadata, firmware hashes and negative compilation reason. Confirm build/test controls fail for the expected reasons. See [SHADOW safety proofs](04_SHADOW_SAFETY_MODEL.md) and [G2 acceptance](10_MASTER_IMPLEMENTATION_PLAN.md).
4. Prepare the isolated G3 test matrix, capture/recovery procedure and acceptance thresholds. Only carry out physical flash and reset sequences if authorized and recovery is qualified. Verify the relay stays de-energized without ever commanding it ON. Stop on unqualified identity, unknown recovery, pulse or unintended bus activity. Keep the inverter disconnected through this slice.
5. Report exact files, artifacts, results and pending proofs. Include board and toolchain identity, build hashes, scope/serial logs when applicable, remaining blockers and explicit G1/G2/G3 verdicts. Record whether the port was opened, whether a flash took place, whether the bus was connected, and what electrical measurements were performed.

Local control and decision authority belong on this board. Keep all prior host deployment out of the implementation graph. A future bus-read slice requires the G3 result and separate G4 authorization. This handoff ends at diagnostic qualification.
