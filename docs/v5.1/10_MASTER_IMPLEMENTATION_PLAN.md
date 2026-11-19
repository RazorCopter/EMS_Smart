# Master implementation plan

Mission 1 delivers this design and records the as-found repository. G0 documentation is complete after dossier verification. G1 verified Espressif USB Serial/JTAG and chip/flash identification in ROM on COM10. Visual label, PCB revision and antenna correlation remain open. G2 through G11 are planned. Paths below are proposed for the implementation missions and additions pair with matching headers when needed. G2 compile-only preparation can proceed in parallel with physical G1 recovery. Each gate records owner, date, verdict, source/config and firmware hash in its evidence sheet. Dependencies govern release promotion.

## G0 · Repository baseline verified

- **Objective.** Freeze HEAD, index and existing local work.
- **Prerequisites.** Read the mission and repository instructions, resolve the checkout path.
- **Reuse.** Git history, documents, tests and vendor references.
- **Existing files to modify.** None.
- **Files to create.** The fourteen Mission 1 documents, including this gate record.
- **Work.** Inventory files and provenance, hash the as-found content, review legacy startup and classify findings.
- **Tests.** Git status/diff, profile check, legacy static Shadow manifest and simulator unit suite.
- **Acceptance criteria.** Baseline identity and limitations recorded, and only the planned dossier added.
- **Evidence.** HEAD 1b7ed555, nine modified tracked files before the audit, empty staged diff and no gitlinks. See assessment for content hash and local checks.
- **Risks.** Dirty-tree changes can be mistaken for branch contents. Compare HEAD and the effective filesystem.
- **Rollback.** Discard an isolated experiment and return to the recorded checkout. Never revert the shared working tree.
- **Definition of Done.** Assessment, source inventory and doc-only diff verified. Baseline documentation complete in Mission 1.

## G1 · Hardware and pinout verified

- **Objective.** Match vendor design evidence to the actual ESP32-S3-Relay-1CH specimen.
- **Prerequisites.** G0; unit available over USB, inverter disconnected, AUX open before reset.
- **Reuse.** Vendor manual/schematic/demo and local board and UART constants.
- **Existing files to modify.** 02_HARDWARE_BASELINE.md and board status metadata only if discrepancy is confirmed in the implementation mission.
- **Files to create.** Gate evidence record including board photos/IDs and esptool log.
- **Work.** Verify model/antenna/PCB, UART1 pins and RTC/AUX design assignment. Enumerate COM, enter ROM safely, read flash ID and chip features.
- **Tests.** USB/PnP enumeration and non-destructive ROM inspection per bring-up plan.
- **Acceptance criteria.** Reproduce S3 rev v0.2, 16 MiB and embedded 8 MiB, or resolve a mismatch explicitly. Tie pin dossier to the actual board and defer runtime electrical proof to G3/G4.
- **Evidence.** Marked board photos, sanitized port/ROM log and schematic/demo hashes.
- **Risks.** Wrong COM identity or variant. COM10 identified the S3/flash profile in ROM. Finish the visual correlation before binding the pin map to a production image.
- **Rollback.** Leave wiring isolated and exit ROM to the agreed safe state without changing flash.
- **Definition of Done.** G1 closes when the specimen and documentation match. G2 offline may progress while COM10 is unavailable.

## G2 · Reproducible minimal ESP32 build

- **Objective.** Create a USB-only build with a dedicated source allowlist and auditable metadata.
- **Prerequisites.** G0; official G1 baseline. Physical confirmation before hardware use.
- **Reuse.** PlatformIO platform pin, board constants and known USB/PSRAM settings.
- **Existing files to modify.** platformio.ini; src/boards/board.h only if required to select the verified target.
- **Files to create.** src/diag/main.cpp; src/diag/embedded_build_info.h; tools/check_v51_elf.py; evidence build manifest; optional new partition CSV after original layout inspection.
- **Work.** Provision pinned Xtensa SDK; compile standalone composition; retain ELF+map; check identity, partitions, hashes and allowlist.
- **Tests.** Two clean compile-only runs; image-info; positive/negative compile controls; forbidden object/reference and route scan.
- **Acceptance criteria.** Both builds succeed and provenance/hashes reconcile. No legacy commands, relay ON path, or UART TX in diagnostic composition.
- **Evidence.** Compiler/SDK versions, commands, source manifest, size, map/nm artifacts, partition and binary hashes.
- **Risks.** Incorrect flash mode, C3 artifact confused for S3, hidden include dependencies or non-reproducible toolchain.
- **Rollback.** Use last verified SDK/source manifest or keep build gate closed until corrected.
- **Definition of Done.** Build diagnostics can be reproduced from the documented inputs and audited without board access.

## G3 · Safe diagnostic firmware

- **Objective.** Verify the approved diagnostic image on a disconnected bench.
- **Prerequisites.** G1/G2; authorized flash, recoverable original image/layout, measurement instrumentation.
- **Reuse.** Board HAL, RTC decoder and reset/watchdog diagnostics.
- **Existing files to modify.** src/diag/main.cpp; build allowlist and board adapter as needed.
- **Files to create.** src/aux/aux_off_only.cpp and .h; src/diag/rtc_readonly.cpp; bench acceptance record.
- **Work.** Verify backup/recovery, off-only setup, USB heartbeat, RTC and memory; exercise reset types with isolated instruments.
- **Tests.** 20 cycles each for cold, reset, watchdog, panic and brownout; ≥30 min idle; scope GPIO47/coil/contact and RS485.
- **Acceptance criteria.** Zero relay pulse/closure or bus TX, expected reset reasons and stable logs, 16 MiB flash and 8 MiB usable PSRAM. RTC either validates or is diagnosed and safely unavailable.
- **Evidence.** Artifact hash, logs, trigger settings and scope traces tied to this board, original layout and protected recovery hash.
- **Risks.** Transient GPIO high, unknown backup or wrong partition layout.
- **Rollback.** Disconnect everything; only restore an approved recovery image on the disconnected bench.
- **Definition of Done.** Bench report signed off and runtime diagnostics/safe electrical state qualified.

## G4 · Read-only RS485 validated

- **Objective.** Prove the only physical communication is the bounded FC04 path.
- **Prerequisites.** G3, dummy responder; table and signed permission for first physical read on the installed inverter.
- **Reuse.** RTU CRC/codec, UART HAL and guard validation patterns; Afore profile/provenance.
- **Existing files to modify.** profiles/afore/af5k_sl.toml following evidence; platformio.ini for SHADOW source filter.
- **Files to create.** src/transport/read_only_modbus_transport.cpp and .h; src/drivers/afore/afore_read_driver.cpp and .h; src/shadow/main.cpp; test/test_v51_read_only/test_main.cpp.
- **Work.** Isolate UART capability, confirm RTS and echo handling, start with SOC register in dummy bench then approved inverter, qualify later blocks one at a time.
- **Tests.** Frame allowlist and rejection suite, spy TX, timeout/CRC/broadcast, independent scope/decoder, compare SOC with display.
- **Acceptance criteria.** 100% captured requests are allowed reads; zero writes and AUX activity. ≥100 consecutive valid dummy FC04 exchanges and ≥20 approved physical SOC responses with expected agreement before broadening.
- **Evidence.** Raw frames, bus topology, trace, firmware hash, counters and roll-forward permit.
- **Risks.** Second master, wrong slave, swapped A/B, word-order/sign errors.
- **Rollback.** Disconnect ESP32's bus and boot qualified G3 image on bench; preserve inverter native configuration.
- **Definition of Done.** One-owner topology and repeatable qualified read-only traffic proven at the intended installation.

## G5 · Energy State validated

- **Objective.** Deliver one coherent, typed and quality-annotated state source.
- **Prerequisites.** G4 and access to real read traces plus independent reference observations.
- **Reuse.** Canonical measurements, StateStore, RTU helpers, pure balance model and worksheet.
- **Existing files to modify.** Profile rows, reader mapping and snapshot integration; retire other telemetry paths from build selection.
- **Files to create.** src/ems/energy_state.cpp and .h; test/test_v51_energy_state/test_main.cpp; redacted profile-native fixtures.
- **Work.** Qualify sample timing, units, word order, signed values, quality and topology. Age data during backoff. Surface raw status and capability confidence.
- **Tests.** Golden FC04 decode, deterministic time/overflow, skew, invalid BMS/field, recharge/discharge, import/export, AC balance check against independent reference.
- **Acceptance criteria.** Canonical signs match all observed sign cases. Unknowns remain null/invalid, and eligible fields never exceed staleness/skew bounds. Derived load is tagged.
- **Evidence.** Vendor mapping diff, raw frames paired to observed states, quality sequence, before/after unit comparison.
- **Risks.** Conflicting offsets, silent zero fill, derived load circularity or stale cache.
- **Rollback.** Revert unqualified rows or disable dependent strategies; retain raw snapshots.
- **Definition of Done.** State contract and profile certified for safe SHADOW decisions within measured coverage.

## G6 · EMS Core SHADOW end to end

- **Objective.** Run real local scheduling, strategies, safety and simulated execution on ESP.
- **Prerequisites.** G5; stand-alone contract tests and binary-proof setup.
- **Reuse.** Selected math from target-aware, shadow and strategy-engine modules; HA replay scenarios and constraint snippets.
- **Existing files to modify.** src/shadow/main.cpp, CMakeLists.txt and platformio.ini; consolidate selected existing pure strategy files.
- **Files to create.** src/ems/command_arbiter.cpp, safety_engine.cpp, local_scheduler.cpp, shadow_command_executor.cpp and headers; src/ems/decision_audit.cpp and .h; test/test_v51_shadow/test_main.cpp.
- **Work.** Start with Self Use, Charge First, Native Inverter and Safe Mode. Add Target SOC and Maintenance, then Grid Optimized Charge and Time Of Use with replayed advice and qualified fallbacks. Route all branches through one arbiter.
- **Tests.** Golden decision and property tests, reserve and conflicting caps/ramp, stale transitions, bounded forecasts, schedule/time faults, end-to-end simulated event and lower sink spies.
- **Acceptance criteria.** Every cycle produces audit, uses measured feedback, handles stale/offline data, obeys final safety bands, executed=false and zero writes. No actuator paths compile.
- **Evidence.** Host/ESP replay comparisons, arbiter traces, test matrix, memory/timing and ELF/physical trace.
- **Risks.** Conflicting authorities, missing BMS info, runaway optimizer or sign reversal.
- **Rollback.** Select Native Inverter or Safe Mode and use last qualified image on bench; preserve raw measurements.
- **Definition of Done.** All eight strategies are distinctly modeled, tested within supported scope and independently reproducible. Optional-service outages are covered.

## G7 · MQTT and Home Assistant

- **Objective.** Add optional advisory input and consume/observe without granting control.
- **Prerequisites.** G6; private test broker, bounded advice schema and time policy.
- **Reuse.** MQTT transport, discovery, HA replay fixtures.
- **Existing files to modify.** src/shadow/main.cpp, src/outputs/mqtt/mqtt_payloads.cpp if compatible, integrations/homeassistant/README.md.
- **Files to create.** src/outputs/mqtt/shadow_mqtt.cpp and .h; src/ems/advice_lease.cpp and .h; integrations/homeassistant/afore_v51.yaml; test/test_v51_advice/test_main.cpp.
- **Work.** Define topics, broker rights, LWT, bounded publication, authenticated advice and receipt. Apply QoS 1, reject retained/replayed overrides, require finite TTL, local scope and config policy.
- **Tests.** Broker absent at boot, disconnect/reconnect, stale forecasts, duplicates, out-of-order, unauthorized and oversized payloads, local cadence, disabled AUX denial.
- **Acceptance criteria.** Local core remains responsive. No retained override is accepted or persists across reboot. Published decision schema and allowed inputs agree with REST.
- **Evidence.** Redacted broker trace, HA discovery state, expiry/audit examples, offline comparison and ACL configuration.
- **Risks.** Blocking IO, lease revival, spoofed advice or unbounded queues.
- **Rollback.** Disable broker connection or advisory intake and continue with local policy.
- **Definition of Done.** HA remains optional under outages, and advice affects only permitted SHADOW decisions.

## G8 · REST API and Web Setup

- **Objective.** Expose status, decisions and atomic local setup.
- **Prerequisites.** G6 and shared G7 message semantics. Authorized Wi-Fi/config bench.
- **Reuse.** Config validation, NVS primitives, safe provisioning and existing read-only output shapes.
- **Existing files to modify.** src/shadow/main.cpp, src/config/configuration.cpp and src/config/nvs_backend.cpp if needed for shared abstractions.
- **Files to create.** src/outputs/rest/shadow_rest_api.cpp and .h; src/web/assets/setup_v51_html.h; test/test_v51_config/test_main.cpp.
- **Work.** Expose paged audit/health/AUX capabilities and controlled editable policy thresholds; require auth, revision and bounded validation; commit only validated settings.
- **Tests.** Schema, conflicting edits, credential redaction, power-loss during commit, invalid clock range, UI on small screens, legacy/forbidden field denials, MQTT/REST payload parity.
- **Acceptance criteria.** One previous valid generation recoverable. Read-only observation, guarded edits only. No route to LIVE/actuate or raw bus.
- **Evidence.** API route inventory and test logs, revision/reboot evidence, representative setup capture.
- **Risks.** Secret exposure, invalid persistence, cross-service config conflict.
- **Rollback.** Restore previous valid settings generation or use qualified USB setup recovery.
- **Definition of Done.** Minimal setup configures standalone behavior and audit reads agree with MQTT.

## G9 · External Decision Analytics

- **Objective.** Persist and compare observed behavior with audit and modeled counterfactuals.
- **Prerequisites.** G7/G8 and versioned audit examples; independent external container host.
- **Reuse.** Shared schema, fixture/replay datasets and optional published metrics.
- **Existing files to modify.** docs/v5.1/07_DECISION_AUDIT_SCHEMA.md only if a versioned contract change is necessary.
- **Files to create.** analytics/Dockerfile, analytics/compose.yaml, analytics/README.md, analytics/schema.sql, analytics/ingest.py, analytics/views.py and analytics/tests/.
- **Work.** Implement durable ingestion, deduplication, gaps and history with CSV/JSON export. Provide simple graphs and advanced configuration via authenticated ESP API, with role separation.
- **Tests.** Duplicate and out-of-order events, reboot/sequence gaps, export provenance, external outage, backup/restore and no ESP cycle regressions.
- **Acceptance criteria.** One stored event per key, loss visible, plots/export identify measured and simulated fields, disabled container leaves local control timing unaffected.
- **Evidence.** Ingest parity fixtures, startup health, query/export samples, restart and outage logs.
- **Risks.** Comparison interpreted as causation, ingestion lag, exposing credentials.
- **Rollback.** Stop the container; restore its own data store independently of ESP.
- **Definition of Done.** Analytics reads qualified audit data and has no Modbus or autonomous-control responsibility.

## G10 · Medium-term SHADOW observation

- **Objective.** Qualify a named SHADOW release based on measured stability and scenario coverage.
- **Prerequisites.** G6–G9, sensor audit, fixed firmware/profile/config hashes and permission for read-only observation.
- **Reuse.** Existing audit, replay, health and trace capture; no new planner.
- **Existing files to modify.** docs/v5.1/12_RISK_REGISTER.md and the per-release gate record as evidence arrives.
- **Files to create.** Observation workbook or exported tables and scenario evidence report in the future qualification workspace.
- **Work.** Run instrumentation against explicit denominator windows, overlapping bus capture and scenario sequence below. Analyze observed and counterfactual fields with gap flagging.
- **Tests.** Apply quantitative thresholds and scenario tests below. Verify disconnected operation and restore system availability.
- **Acceptance criteria.** All safety gates and required coverage pass, independent capture substantiates zero writes, every metric has evidence and unexpected gaps/errors resolved.
- **Evidence.** Timestamped logs, counters, captures, builds, annotated scenarios, gaps, exceptions and signed review.
- **Risks.** Elapsed time mistaken for coverage, lost audit, solar/tariff scenario absent.
- **Rollback.** Keep SHADOW only, disable affected strategy or return to qualified local observation; repeat failed scenarios.
- **Definition of Done.** Scenario and metric report accepted with gaps explained and blockers cleared. Observation time alone cannot qualify.

## G11 · Separate future LIVE evaluation

- **Objective.** Decide whether evidence supports a separate LIVE qualification project.
- **Prerequisites.** G10 plus explicit owner authorization and separate technical/regulatory review.
- **Reuse.** Read-only traces, v5.1 audit/Safety model and independently qualified fixtures.
- **Existing files to modify.** None for v5.1 firmware or deployment.
- **Files to create.** Separate proposal and validation dossier with hardware, policy, interlock and recovery criteria.
- **Work.** Review manufacturer register semantics, BMS permissions, native control fallback, safety controls and attended qualification scope. Record go/no-go.
- **Tests.** Review simulation and fault injection evidence only in v5.1. Any hardware actuation test belongs to separately authorized scope.
- **Acceptance criteria.** Decision records unresolved register/plant hazards and criteria for a distinct LIVE binary and recovery plan.
- **Evidence.** Signed decision, reviewed risks and traceability to G10 artifacts.
- **Risks.** Using SHADOW as authorization, unsupported control register, per-cycle EEPROM wear or unresolved external constraints.
- **Rollback.** Retain current qualified SHADOW image and scope; close evaluation with no-go when evidence is insufficient.
- **Definition of Done.** Separate decision is documented. v5.1 release remains SHADOW-only.

## G10 candidate thresholds and required scenario coverage

All values below are provisional until G3–G6 characterize the platform and plant. Freeze thresholds, sampling rates and exclusions before observation. Take a proposed minimum seven-calendar-day window with all scenarios below. Missing or incomplete scenarios extend the window. Record planned fault injections separately in denominators, with full reporting including downtime.

| Metric | Initial criterion and evidence |
|---|---|
| Availability | ≥99% of nominal operating time with eligible essential telemetry; count time in 1 s bins including involuntary downtime. Publish denominators for normal and injected-condition windows. |
| Valid frames/samples | ≥99.5% complete valid scheduled transactions in normal windows and ≥99% essential field samples valid; include timeouts, corrupt, stale and missed polls. |
| Maximum data age | Critical decision inputs ≤5 s age and ≤2 s inter-source skew whenever eligible. Zero decisions using expired inputs. Enter Safe Mode by next 1 s decision after threshold or fault. |
| Decision count by strategy | Record selected and effective strategies; ≥100 audited decisions per enabled strategy across real observations or separately tagged deterministic replay. Include at least ten audited mode transitions. |
| Setpoint churn and oscillation | During stable windows with grid/load variation ≤100 W over 5 min, proposed ≤12 changes >100 W per hour, ≤2 meaningful sign reversals per 10 min and no sustained alternating ≥100 W sequence lasting >30 s. Report percentiles separately for transient events and stratify by strategy. |
| Safety interventions | Count each constraint and intervention duration. All triggered bounds persist in audit and zero final plans violate them. Report false veto reviews and injection recall (100% expected hazard cases blocked/limited). |
| SOC violations avoided | Track candidate violations clipped to feasible bands, ≥10 replay/bench boundary cases for floor/ceiling, allowed recovery and incompatible caps. Zero simulated discharge at/below reserve or simulated charge above ceiling. Real SOC and native behavior remain independent observations. |
| Target reachability | At least 5 reachable and 5 unreachable replay schedules with solver/oracle agreement and explicit reasons. Report measured native SOC at deadline separately. Historical counterfactual reaches target only in its documented model. |
| Forecast dependency | Test missing, valid, delayed, stale and contradictory advice including DST. PV/load forecast and tariff policy default TTL ≤3 h to confirm, with independent slot validity. Zero expired forecast usage and fallback by next decision cycle. Count decisions and forecast ages by provenance. |
| MQTT, Wi-Fi and optional services | ≥3 losses and restores each for MQTT and Wi-Fi, one ≥30 min per service, boot without either, plus container absent ≥24 h. Test HA absent and no Internet. Observe same local eligibility and cadence at equivalent inputs, deterministic lease expiry and ring gap accounting. |
| Mock decisions and actual writes | Record SIMULATED/NO_ACTION/BLOCKED counts and compare to decision counts. 100% executed=false, 0 effective Modbus writes in firmware counters and external capture. Full-window independent monitoring or equivalent qualified coverage with explicit gaps is required to pass this safety claim. |
| AUX attempts | At least 2 per ingress in bench/replay including legacy pathways, 100% denial and audit, 0 observed energizations or contact closure. Record mode/feedback semantics and G3 traces. |
| Resets and watchdog | Zero unexpected resets/crashes/watchdog trips during normal observation. All deliberate resets have known cause, new boot ID and safe startup evidence. |
| Memory and fragmentation | Initial minimum free internal heap ≥64 KiB, largest free internal block ≥32 KiB, ratio largest/free ≥0.35 under representative peak and ≤10% loss in stationary post-warmup baseline over window. Measure PSRAM separately, and retain per-task stack high-water ≥1 KiB after unit conversion. |
| Cycle timing | 1 s nominal, p99 core decision processing ≤50 ms, max ≤100 ms excluding asynchronous I/O. p99 inter-tick jitter ≤100 ms, zero missed deadlines >2 s excluding qualified reboot. Measure poll and optimizer independently, including degraded networking. |
| Audit completeness | Proposed ≥99.9% persisted decision records when analytics reachable, with 100% known losses represented as gaps, zero hidden loss or duplicate keys. Quantify offline ring overwrites and UTC gaps. |

Coverage requires daytime surplus, evening/night deficit, import and export, charge and discharge, stable and rapidly changing loads/PV, low and high SOC, maintenance timeout/abort, target reachability outcomes, sunny and poor forecasts, flat/negative tariffs, stale and missing data, unknown clock, corrupt/silent RTU, lease expiry, queue saturation, connectivity outages, reset and denied AUX. Plant evidence covers normal native behavior on actual hardware. Unsafe, infeasible or unobserved combinations use bench and replay labeled accordingly. Review counterfactual assumptions against real tracks and never infer actual effectiveness from simulated setpoints.
