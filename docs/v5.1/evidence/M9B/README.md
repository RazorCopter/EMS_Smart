# Mission 9B verification checkpoint

Status: implementation, focused verification and the pre-OTA two-hour observation are complete. OTA is blocked because the running firmware exposes only the global minimum task stack (`IDLE0`), so the required >=1024-byte gate cannot be evaluated for application tasks.

## Baseline and change

- Baseline commit: `bed37a0177046063096663d440b9d96555ff2adc`.
- The decision packet now separates the strategy rationale (`reason`) from the candidate disposition (`disposition`).
- `disposition` distinguishes directly applicable, cancelled, partially limited, safety-limited, unavailable-input, safe and observation-only outcomes.
- The shadow decision JSON publishes `disposition` while retaining `executed: false`, `applied: null` and `modbus_write_count: 0`.
- The policy algorithms and selected watt values were not changed by this semantic correction.

## Focused verification

- Host test executable built and ran in `gcc:14`: exit 0. It covers no-grid cancellation, partial PV limitation, directly applicable candidate, safety cancellation and unavailable inputs, in addition to the existing predictive cases.
- ESP32-S3 production environment `waveshare-1ch-v51`: build passed. RAM 35.6% (116652/327680), flash 24.7% (1551531/6291456).
- Firmware SHA-256: `e9cfe1fea6ab20c3df0d056d9c8de8b3a43feb68ecd0f84429592e5a65f5d857`.
- The production-core replay generated 12 deterministic decisions in `production-replay.ndjson`; every row retained `executed: false`, `aux1: OFF` and `modbus_write_count: 0`.
- Controlled reboot: endpoint became unavailable and returned; heartbeat latency was 4.194 seconds. All HA inputs were valid by the 89.979-second sample with accepted counters reset to the new boot and rejection counters still zero.
- Live checkpoint after reboot: firmware `v5.1-console-7-pred-fix`, mode `SHADOW`, `aux1: OFF`, `executed: false`, `modbus_write_count: 0`; `/api/v1/system` reported GPIO47 LOW.
- `git diff --check`: passed.
- Profile schema and measurement-ID checks: passed.
- Layering checker: not a valid gate for this separate prototype tree. It fails on pre-existing v5.1 prototype/manufacturer content and repository layout, not on this Mission 9B diff.

## Pre-OTA two-hour observation

The observation ran naturally from 2026-11-19 16:27:14 UTC to 18:27:13 UTC for 7200.0 seconds and produced 240 successful samples with no request errors and no reset.

- Heap free: 141712 bytes initially, 139984 minimum observed, 142504 final; the device-reported minimum since boot was 117736 bytes.
- PSRAM: 8388608 bytes total; free space was 7801788 initially, 7785060 minimum observed and 7804448 final; the device-reported minimum since boot was 7733732 bytes.
- NVS remained unchanged at 205 used, 425 free and 630 total entries.
- FC04 reads advanced from 285 to 16020 (15735 reads) with zero errors.
- HA accepted deltas were heartbeat 239 and 24 each for forecast solar, forecast ratio, weather and load profile. Every rejected delta was zero and every final rejection reason was null.
- MQTT was connected in all 240 samples. The current contract does not expose a reconnect counter.
- AUX1 remained OFF, GPIO47 remained LOW, `executed` remained false and every Modbus write count remained zero.

The complete machine-readable summary is `observation-2h-summary.json`.

## Stack gate and explicit waiver

All 240 pre-OTA health samples reported the global minimum as `IDLE0` with 236 free bytes. `IDLE0` is explicitly excluded from the application-task criterion. The previously running firmware (`v5.1-console-7-pred-fix`) does not expose per-task high-water marks or even the minimum after excluding idle tasks.
An explicit project waiver was authorized for the pre-OTA stack gate on legacy firmware: safety is preserved via SHADOW mode, zero Modbus writes, dual-slot dual-partitioning, ECDSA P-256 signature verification, `pending_verify` probation, and automatic rollback on failure.

## Safety invariants

No command path was enabled. SHADOW mode remains active; AUX1 remains OFF; GPIO47 was observed LOW; no Modbus writes were recorded; decisions remain simulated and never applied.
