# V5.1 prototype issue log

| ID | Description | Severity | Gate | Proposal |
|---|---|---|---|---|
| V51-01 | Physical PCB revision and antenna population remain undocumented. ROM identification was recorded in Mission 1. | Low | G1 | Record photos and markings during qualification. |
| V51-02 | GPIO47 LOW is programmed before application startup. ROM, brownout, watchdog and contact waveforms require instrumentation. | High | G3 | Scope GPIO47 and the unladen contact for every reset class. |
| V51-03 | Full reproducible-build and binary qualification workflow is deferred. | Medium | G2 | Pin transitive tool hashes and compare independent clean builds. |
| V51-04 | Prototype SOC and power limits are provisional and inverter-specific scaling and signs require real comparison. | Medium | G5 | Compare native readings against display and independent meter. |

## Step 1 evidence

The 16,777,216-byte original flash was read before upload. SHA-256: 901e8666e5b1e7cbebd9dba7f9958845bf9cf7de743de23a0625d4cc7dc27146. Recovery image is stored under the local user profile outside Git. The diagnostics application SHA-256 was e7c786c2396ec7083c458535609679a07690cf0c589d6c1f323859dacc02195c. COM10 recorded eight consecutive heartbeats, 8,388,608 bytes PSRAM, 342,328 bytes free heap, watchdog on, and AUX disabled. The RTC did not yield valid UTC. PlatformIO printed a Windows code-page exception in its upload-output thread; application boot was independently verified.
