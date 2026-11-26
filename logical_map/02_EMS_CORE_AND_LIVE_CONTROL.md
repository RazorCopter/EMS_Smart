# 02 — Boot, Motore Decisionale EMS, Esecuzione LIVE & Test Host

---

## Entry Point e Stato Telemetria

### `src/v51/main.cpp` (122 righe)
- **Scopo**: `initVariant()`, `setup()` e `loop()` su `loopTask` (stack `12288 B`).
- **Flusso**:
  - `initVariant()` / `setup()`: forza subito `GPIO47` (`AUX1`) a `0`, avvia `Serial`, `Wire` (I2C RTC), hardware watchdog `esp_task_wdt` (`10 s`), e `v51::beginRemote()`.
  - `loop()`: attende `11 s` + `ESP_OTA_IMG_VALID` prima di avviare `reader.start()`, chiama `reader.tick()`, `serviceAux()`.
  - Ogni `1000 ms`: emette log `SHADOW/LIVE heartbeat` ed `EnergyState`, chiama `tickEms(state, lastHeartbeat, sequence, false)`, esegue (se `V51_LIVE_TRIAL`) `serviceLiveTrial(reader, decision, lastHeartbeat, healthy)` (richiedendo `clock.utc` solo se `needsClock` per strategie predittive o `Target` giornaliero), e poi chiama `recordEmsDecision(...)` con `liveTrialExecuted()` e `liveTrialAppliedWatts()`.
  - Ad ogni iterazione: `tickRemote(...)`, `esp_task_wdt_reset()`, `delay(20)`.

### `src/v51/energy_state.h` (32 righe)
- **Scopo**: Strutture dati pure `Quality { Missing, Valid, ReadError, Invalid, Stale }`, `Sample { value, stamp, status }` (stale dopo `5000 ms`) ed `EnergyState` (tutte le misure DC/AC/batteria + `goodReads`, `errors`, `readHalted`).

---

## Motore Decisionale e Runtime

### `src/v51/shadow_core.h` (836 righe)
- **Scopo**: Core decisionale puro (`StrategyManager`, `Policy`, `Decision`, `PredictiveInputs`, `PredictiveCycleState`), compilabile su host senza dipendenze ESP32.
- **Strategie (`Strategy`)**: `SelfUse (0)`, `ChargeFirst (1)`, `Native (2)`, `Safe (3)`, `Target (4)`, `Maintenance (5)`.
- **`Policy` & `Policy::valid()`**: contiene tutti i limiti SoC/tensione/potenza/corrente, parametri predittivi/stagionali (`autoSeason`, `summerFloor`, `winterFloor`, `fullHoldSec`, `lowSunKwh`, `anticipationMin`), parametri di smorzamento (`dampingW`, `dampingPct`), parametri Trainer (`trainerSettleMs`, `trainerAbsW`, `trainerPct`, `trainerTimeoutMs`, `trainerGapMs`, `trainerSec`, `trainerCritical*`, `trainerRecovery*`, `trainerChargeW`, `trainerDischargeW`) e parametri delega nativa (`delegateNativeSelfUse`, `nativeSwitchCooldownSec`, `maxModeSwitchesPerDay`).
- **`StrategyManager::tick()`**:
  - Valida freschezza e coerenza temporale di `soc`, `grid`, `battery` (`|grid.stamp - battery.stamp| <= min(staleMs, 2000)`).
  - Calcola `surplus = round(*batt - *grid)` e `balance = surplus + gridTarget`.
  - Se `predictive == 1` su `SelfUse`/`ChargeFirst`, esegue `tickPredictive()`: floor stagionale, `fogSticky_` (nebbia/radiazione), profilo carico feriale/weekend, finestra `deadlineMinute` (es. 15:30), e macchina a stati `CyclePhase { SelfUse, GentleCharge, CaptureChargeFirst, FullHold }`.
  - In `ChargeFirst` con `surplusOnly == 0` e `gridCharge == 0`, usa il bilancio netto lato AC dell'inverter (`battery_w + inverter_w`, con fallback a `e.pv` se `e.inverter` non è disponibile) come tetto diurno (`d.upper`), mantenendo `inverter_w >= 0` (`grid_w == house_load`) senza raddrizzare AC dalla rete e azzerandosi di notte.
  - `clampAndFinalize()`: applica limiti `[lower, upper]`, smorzamento `dampingPct`/`dampingW` (bidirezionale in `SelfUse`, asimmetrico solo in risalita per `ChargeFirst`/`Target` `!gridPermitted`), `ramp` (`W/s`), `deadband`, e assegna `disposition` esplicita.

### `src/v51/ems_runtime.h` (14 righe) & `ems_runtime.cpp` (640 righe)
- **Scopo**: Ponte tra `Preferences` (NVS), `clock_service`, `m5` (input HA), `m4` (audit/MQTT) e `StrategyManager`.
- **Funzioni chiave**:
  - `loadPolicy()`: carica `ems-policy-v5` da NVS (con migrazione automatica da v2/v3/v4) e `ems-pred-st` (`PredictiveCycleState`).
  - `savedPolicy()`, `policyJson()`, `applyPolicyForm()`: lettura/aggiornamento atomico (o temporaneo con lease) della `Policy`.
  - `tickEms(state, now, seq, recordNow)` & `recordEmsDecision(...)`: legge `PredictiveInputs` da `m5`, chiama `StrategyManager::tick()`, salva in NVS eventuali cambi di `PredictiveCycleState`, e registra la riga JSON di audit con i valori reali di `executed` e `applied`.
  - `registerEms()`: registra `/api/v1/ems/config`, `/api/v1/ems/policy` (GET/POST), `/api/v1/ems/replay`, `/api/v1/ems/replay/m9b`.

---

## Esecuzione LIVE e Sorveglianza Fisica

### `src/v51/live_executor.h` (110 righe)
- **Scopo**: Macchina a stati volatile che governa `LiveActuator`:
  - `arm(now, leaseMs, startNative)` & `updateLease(leaseMs)`: arma solo in RAM (mai persistito su flash) e aggiorna dinamicamente la lease se cambia `trainer_s`.
  - `tick(decision, decisionAt, now, maxDecisionAgeMs)`: controlla `hasControl()`, `leaseMs_`, `kWatchdogMs` (`120 s`), validità `Decision` (`<= maxDecisionAgeMs`, non `Safe`/`Native`, entro `[lower, upper]`), rinnova `0x0A09` ogni `kHeartbeatMs` (`30 s`) se invariato.
  - Supporta `delegateNative(now)` / `resumeExternal(now)` (stato `NativeDelegated`, controllo FC03 ogni `30 s`).
  - `disarm()` / `stop()`: ripristina `nativeMode` (o verifica `verifyNativeMode()` se già delegato) e transiziona a `Shadow` (o `ReleaseFailed`).

### `src/v51/trainer_response.h` (147 righe)
- **Scopo**: Sorveglianza fisica chiusa tra setpoint confermato (`0x0A09`) e potenza batteria misurata (`battery_w`).
- **Meccanismi**:
  - `confirmed(watts, now)`: traccia `changed_`, `started_` e `pendingSince_` (timer indipendente azzerato solo su `Matched`/`Recovered`).
  - `verificationSetpoint(...)`: se `pendingAgeMs` si avvicina a `trainerGapMs`, mantiene fermo il setpoint corrente per aprire una finestra osservabile, oppure gestisce il target ridotto di recovery.
  - `observe(measured, stamp, now)`: verifica `settled` (`>= trainerSettleMs`), tolleranza adattiva `max(trainerAbsW, |watts| * trainerPct / 100)`, rilevazione guasto severo/segno opposto (`SEVERE_RESPONSE` su 2 campioni consecutivi), `Warning`, tentativo `Recover` a potenza ridotta (`trainerRecoveryPct`) e `Mismatch` su timeout (`STABLE_COMMAND_TIMEOUT`, `VERIFICATION_GAP_TIMEOUT`, `RECOVERY_TIMEOUT`).

### `src/v51/live_trial.h` (23 righe) & `live_trial.cpp` (313 righe)
- **Scopo**: Orchestratore thread-safe (`trialMutex`) tra comandi HTTP (`requestLiveTrial`, `configureLiveTrainer`), `LiveExecutor`, `TrainerResponse` e `ExternalEmsActuator`.
- **Dettagli chiave**:
  - Definisce `V51_BUILD_ID` (`"v5.1-live-trial-18"`), `liveTrialExecuted()` e `liveTrialAppliedWatts()`.
  - Ricarica `trialPolicy = activePolicy()` (rispettando anche eventuali override) e aggiorna `executor->updateLease(...)` ad ogni tick.
  - Gestisce la delega sicura a `Self-Use` nativo **solo** se `!trainer && delegateNativeSelfUse == 1`, `gridTarget == 0`, `safetyReason == "NONE"`, `soc >= effectiveFloor + max(hysteresis, 3)` e budget EEPROM (`cooldownRemaining == 0`, `dailyModeSwitches < maxModeSwitchesPerDay`); in caso di cooldown attivo resta semplicemente in `ExtEms` senza abortire `LIVE`, e riprende `ExtEms` subito su `soc <= effectiveFloor + 1` o cambio strategia.

---

## Test Unitari Host

### `test/v51_shadow_core/main.cpp` (553 righe)
- **Scopo**: Suite C++17 host per `StrategyManager`, `Policy::valid()`, DST `dailySeconds`, stagionalità, `FullHold`, smorzamento `SelfUse`/`ChargeFirst`, `surplusOnly = 0` diurno/notturno e `disposition`.

### `test/v51_live_executor/main.cpp` (278 righe)
- **Scopo**: Suite C++17 host per `externalPowerFrame` (esempi protocollo Afore), `ExternalEmsActuator` (takeover, retry FC03, mismatch, release), `LiveExecutor` (lease, watchdog, delega nativa) e `TrainerResponse` (settle, gap timeout con setpoint variabili, severe, recovery).
