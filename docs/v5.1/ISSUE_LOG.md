# V5.1 prototype issue log

| ID | Description | Severity | Gate | Proposal |
|---|---|---|---|---|
| V51-01 | Physical PCB revision and antenna population remain undocumented. ROM identification was recorded in Mission 1. | Low | G1 | Record photos and markings during qualification. |
| V51-02 | GPIO47 LOW is programmed before application startup. ROM, brownout, watchdog and contact waveforms require instrumentation. | High | G3 | Scope GPIO47 and the unladen contact for every reset class. |
| V51-03 | Full reproducible-build and binary qualification workflow is deferred. | Medium | G2 | Pin transitive tool hashes and compare independent clean builds. |
| V51-04 | Prototype SOC and power limits are provisional and inverter-specific scaling and signs require real comparison. | Medium | G5 | Compare native readings against display and independent meter. |

| V51-05 | Mission 2 Step 1 revision explicitly supersedes the earlier OTA exclusion in the v5.1 dossier. | High | G2/G3 | Limit network upload to authenticated, ECDSA-signed application images on the inactive slot. The signed image requires an authenticated STA health check before confirmation. Prove successful OTA and bootloader rollback with the board disconnected from RS485. |
| V51-06 | RTC replies over I2C but reports oscillator-stop. | Low | G3 | Check backup supply and set trusted UTC in a later authorized step. Keep reporting unknown UTC until valid. |
| V51-07 | HTTP Basic and setup POST carry credentials in cleartext at the HTTP layer. | Medium | G8 | Limit prototype access to a trusted local network. Qualify TLS or a trusted management tunnel before wider deployment. |
| V51-08 | The earlier USB upload console hit an encoding error on Windows. | Low | G2 | Use PYTHONIOENCODING=utf-8 and verify the uploader digest and boot log. |
| V51-09 | The health contract reports only the global minimum task stack. During Mission 9B all 240 samples named IDLE0 (236 free bytes), so the required per-application-task >=1024-byte pre-OTA gate cannot be evaluated. | High | G2/G3 | Expose per-task high-water marks, or at minimum the minimum after excluding IDLE tasks, in an authenticated diagnostic endpoint; repeat the two-hour gate before OTA. |

## Step 1 evidence

The 16,777,216-byte original flash was read before upload. SHA-256: 901e8666e5b1e7cbebd9dba7f9958845bf9cf7de743de23a0625d4cc7dc27146. Recovery image is stored under the local user profile outside Git. The diagnostics application SHA-256 was e7c786c2396ec7083c458535609679a07690cf0c589d6c1f323859dacc02195c. COM10 recorded eight consecutive heartbeats, 8,388,608 bytes PSRAM, 342,328 bytes free heap, watchdog on, and AUX disabled. The RTC did not yield valid UTC. PlatformIO printed a Windows code-page exception in its upload-output thread; application boot was independently verified.

## Revised Step 1 network operation

- mDNS hostname: afore-ems.local. The password-controlled first-run AP is Afore-EMS-Setup, with setup at http://192.168.4.1/.
- The device stores station settings and a device-specific management key in its own NVS namespace. The recovery file and signing private key are kept outside Git in the Windows user profile.
- The endpoint /api/v1/health exposes the running slot, OTA image state, uptime, reset reason, RSSI, heap and PSRAM. /api/v1/logs provides the latest 64 diagnostic records. Both require the management password.
- The final USB image contains two 6 MiB OTA slots and an otadata partition. The prebuilt ESP32-S3 Arduino SDK enables app rollback. The firmware defers boot confirmation until a healthy loop, STA and a successful authenticated health response. Test uploads use tools/v51_ota.py and its deliberate-failure environment.
- The next physical phase starts when the user confirms installation. From that point do not use USB. Check network reachability and record RSSI, uptime and reset cause before activating the read-only bus. Escalate RSSI below -75 dBm.

### Revised Step 1 bench checkpoint

The remote-access image built and was written/verified via USB. App SHA-256: 6f6fbf8ac7f191a65b6a5bb816dcc4f291540fb9633dad1ccea8ab39931aa74c. USB heartbeat showed reset reason 11, 229,344 bytes free heap, 8,388,608 bytes PSRAM, watchdog on, GPIO47 = 0, and no Modbus activity. The deliberately unconfirmed test image built with SHA-256 1382c341c75e0c2baa5735604d47a11faa2f02f88a34628e949f6fc87a7b3358. STA, HTTP log access, network OTA and rollback must be demonstrated after Wi-Fi setup and before Step 1B.

### Revised Step 1 acceptance — 2026-11-18

| Check | Bench result |
|---|---|
| STA and remote logs | Authenticated health and heartbeat log endpoints responded over Wi-Fi. The device moved from 10.0.5.125 to the reserved 10.0.0.110 after OTA reboot. |
| mDNS | A direct multicast query received the A record for afore-ems.local from 10.0.0.110. The Windows resolver temporarily retained 10.0.5.125 after the reservation changed. |
| OTA rejection | An unauthenticated upload returned HTTP 401 and an authenticated unsigned upload returned HTTP 400. |
| Automatic rollback | Signed deliberate-failure image booted in app1 as pending_verify, restarted before confirmation and returned to v5.1-remote-1 on app0. |
| Successful OTA | Signed v5.1-remote-1 image booted in app1 as pending_verify and transitioned to valid. The remote log recorded the boot confirmation. Follow-up health at 26.3 s uptime returned reset reason 3, RSSI -74 dBm and 227,652 bytes free heap. |
| SHADOW boundary | All observed health responses reported AUX1 DISABLED, executed=false and zero Modbus writes. Heartbeats reported GPIO47 LOW. The isolated hardware run had no RS485 action. |

Step 1 accepted. Initial bench RSSI reached -80 dBm before the DHCP move; post-OTA samples were -74 dBm at the final verified endpoint. Recheck RSSI, reset reason, uptime and reachability in the installation location. Installation and later firmware changes are pending user confirmation.

### Step 1B and Step 2 acceptance

Installed STA 10.0.0.110 showed -69 to -68 dBm, reset reason 1, 227,548 B free heap and 8 MiB PSRAM. The existing image was valid on app1 and previous bench rollback was verified. The signed FC04 image booted valid on app0 with -70 dBm. SHA-256 7a633afb300cd825f072f6a69214dd440ce2900c4f702618f9f44f8c00c2387e.

After five seconds of passive listening, the unit read FC04 2002/1 followed by 507/24, 552/9, 2000/9 and 2515/1. Zero initial errors. Raw SOC 0018 -> 24%, grid ffff fffd -> -3 W, PV 0000 001a -> 26 W, battery 0000 0275 scaled -1 -> -629 W, voltage 0916 x0.1 -> 232.6 V, frequency 1385 x0.01 -> 49.97 Hz, heatsink 014d x0.1 -> 33.3 C. The load value of 543 W is derived from the AC balance and requires meter comparison. Remote evidence is saved in .pio/step2-fc04.log within the prototype worktree.

| ID | Description | Severity | Gate | Proposal |
|---|---|---|---|---|
| V51-09 | Passive detection stops on unscheduled traffic or foreign frames, but a quiet second master and wire-level proof require capture. | Medium | G4 | Validate topology and add independent capture in qualification. |
| V51-10 | Raw words and corresponding scale are logged for the initial block round. Ongoing logs contain decoded snapshots. | Low | G5 | Add bounded raw diagnostic history if needed. |

### Step 3 acceptance

Host assertions passed for SELF_USE, CHARGE_FIRST, NATIVE_INVERTER and SAFE_MODE, including stale input, bus halt and the post-ramp SOC floor/ceiling regression. The signed image v5.1-shadow-1 (SHA-256 13088b3dd01d29a95d41d9d156363a7a32b00229701c56c5a6d6161ddb2a415f) booted valid on app1 at 10.0.0.110, leaving FC04 checkpoint app0 available. Final audit at 39.5 s: 52 valid reads, zero errors, SOC 23%, grid -3 W, battery -679 W, candidate -676 W, simulated -683 W, executed=false. Six consecutive health samples had increasing uptime and -67 to -63 dBm, with no unexpected reset. ELF scan did not find legacy write encoders, relay controller or profile driver execution. Raw evidence is in .pio/step3-shadow.log.

| ID | Description | Severity | Gate | Proposal |
|---|---|---|---|---|
| V51-11 | SELF_USE is the fixed runtime strategy in this build. SAFE_MODE is selected on unusable inputs. CHARGE_FIRST and NATIVE_INVERTER are implemented and host tested. | Medium | G7/G8 | Expose strategy and threshold selection via authenticated configuration, then verify each strategy on the ESP32. |
| V51-12 | Simplified decision audit shares the 64-record diagnostic ring. Timestamp uses heartbeat sequence/uptime, with RTC UTC currently unavailable. | Low | G6/G8 | Add a decision-only API and typed, longer-lived records. Replace the missing age placeholder with null. |
| V51-13 | MQTT, broker and Wi-Fi outage recovery, and the 30-minute observation are still pending. | Medium | G7/G10 | Complete the connectivity and verification steps using the real plant. |

### Mission 3 issues

| ID | Description | Severity | Gate | Proposal |
|---|---|---|---|---|
| V51-14 | Battery capacity (5,000 Wh) and charging efficiency (90%) are provisional defaults. Planner feasibility uses the currently available power, which varies with measured surplus. | Medium | G6 | Set validated values after battery identification and compare against battery energy data. |
| V51-15 | Maintenance hold is simulated. Automatic battery calibration and any battery-specific charge/hold protocol lack verified vendor behavior. | High | G11 | Review battery documentation before implementing any calibration logic. |
| V51-16 | Relative deadlines and hold timers use monotonic uptime; a reboot or configuration change starts a new session. | Medium | G6 | Qualify checkpointed session recovery and optional UTC deadlines after a trusted clock is available. |
| V51-17 | SOC/limits are persisted; lead-time and maintenance lifecycle were replayed on the production ESP32 firmware. Fault injection and real battery control remain outside SHADOW. | Low | G10 | Expand coverage of real environmental and communications faults. |
| V51-18 | Independent load metering and signed battery-to-AC loss calibration are pending. Small discrepancies in the instantaneous balance are expected because the sources have different capture windows. | Medium | G5/G10 | Evaluate separately with synchronized and independently measured reference data. |

### Mission 4 issues

| ID | Description | Severity | Gate | Proposal |
|---|---|---|---|---|
| V51-19 | SOH, battery temperature, grid current, inverter running status and cumulative energy lack a qualified binding in the current FC04 profile. | Medium | G5 | Validate manufacturer address, scale and quality on the installed inverter before publishing. |
| V51-20 | Discovery was verified with a real broker and schema inspection; no production Home Assistant endpoint or production broker was configured for this mission. | Medium | G7 | Point HA and ESP to the same deployment broker and check entity onboarding. |
| V51-21 | TLS requires both a valid clock and an ESP certificate-bundle chain. Positive TLS testing with the final broker and private CA provisioning are pending. | Medium | G7 | Test TLS with the installed broker and add private CA handling if required. |
| V51-22 | Each settings section is atomic and has rollback. Whole-file restore spans EMS and MQTT namespaces and is not power-loss atomic across sections. Runtime revision counters restart on boot. | Medium | G8 | Add a persistent shared generation and a single bundle promotion if needed. |
| V51-23 | Changing device ID or discovery prefix can leave the previous identifiers retained on the broker. Deduplication cache holds the latest 32 message IDs in RAM. | Low | G7 | Treat device ID as stable and remove old retained configs during migration; quantify command burst limits. |
| V51-24 | Audit persists in PSRAM until reboot. The structured decision body carries core SOC/grid quality and no separate long-term raw trace. | Low | G8 | Extend correlation and cross-field quality metadata when analytics are added. |

### Mission 5 issues

| ID | Description | Severity | Gate | Proposal |
|---|---|---|---|---|
| V51-25 | SEM forecast surplus-to-target, next-day deferral, and fog compensation are intentionally not part of TARGET_SOC yet. Push inputs are validated, visible and audited as unused. | Medium | Next mission | Adapt and replay the existing SEM logic before letting any HA input influence a decision. |
| V51-26 | There is no relay contact feedback. Software restart and pin-level checks do not qualify every watchdog, ROM startup or brownout transient. | Medium | G3 | Document contact monitoring and scope the unloaded unit during separate bench qualification. |
| V51-27 | HA package and Obsidian patch are delivered as files; installation, rendered card and actual HA switch are not validated in a running HA instance. | Low | G7 | Install on the owner side and validate entity IDs; MQTT protocol tests use the equivalent Discovery envelopes. |
| V51-28 | The HA/Solarman battery profile sets 10,240 Wh, 51.2 V nominal and 50 A. Independent 1,000 W caps remain active when more restrictive. Charge efficiency remains a separate provisional setting. | Low | G5 | Review measured limits and efficiency against the battery specification before changing policy. |
| V51-29 | Browser tooltip and sliders cover the requested forms; explicit minimum/maximum consistency across settings is still enforced server-side. Day schedule uses the Europe/Rome clock with a relative fallback. | Low | G8 | Add a formatted time picker and more cross-field inline diagnostics if needed. |

### Missione 5B

| ID | Descrizione | Gravità | Gate | Proposta |
|---|---|---|---|---|
| V51-30 | Il firmware incorpora le regole POSIX future del database IANA 2025b. Le variazioni istituzionali successive richiedono un aggiornamento del dataset. | Bassa | G8 | Rigenerare la tabella quando cambiano le zone usate sul campo. |
| V51-31 | Verificati avvio, sincronizzazione su richiesta, boot con RTC e i due server fallback separatamente. Il ciclo di sei ore e la perdita fisica del Wi-Fi non hanno una prova di durata dedicata. | Bassa | G8 | Acquisire un log di sei ore e ripetere il test con il Wi-Fi temporaneamente interrotto. |

### Missione 6B

| ID | Descrizione | Gravità | Gate | Proposta |
|---|---|---|---|---|
| V51-32 | Completata la matrice dei comandi dalla UI HA. La scadenza Solcast è verificata su payload reale riprodotto con TTL di 10 s; manca una prova di scadenza naturale a 14 ore. | Bassa | G7 | Campionare la scadenza a 14 ore in un test di durata separato. |
| V51-33 | L’ACK di configurazione può riportare la strategia effettiva del ciclo precedente. Il pannello conferma lo stato effettivo con la decisione successiva. | Bassa | G8 | Mantenere esplicita la semantica dei campi nelle integrazioni. |
