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
