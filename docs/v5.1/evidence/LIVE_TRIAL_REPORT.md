# Live Trial & Trainer Verification Report (2026-11-21)

Target device: `10.0.0.110` (`waveshare-1ch-v51-live-trial`)  
Inverter: Afore Hybrid (`af5k_sl`), RS485 Modbus RTU (`0x09C4` mode, `0x0A08` control type, `0x0A09` active power setpoint).

---

## 1. Test eseguiti ed evidenze raccolte

### Test 1 — Primo avvio LIVE + Trainer ±200 W (`v5.1-live-trial-1`, `invert_live_sign = false`)
- **Esito**: **Interrotto automaticamente dal Trainer dopo ~11 s** (`LIVE_TRAINER_MISMATCH_NATIVE_RESTORED`), native mode (`SELF_USE`) ripristinato correttamente.
- **Evidenza** (`live_trial_capture.ndjson`, 136 campioni):
  - Setpoint comandato: `+200 W` (carica).
  - Potenza batteria misurata (`battery_w`): **`-188 W` .. `-196 W`** (scarica!).
- **Diagnosi**: La polarità di scrittura su `0x0A09` dell'inverter è **opposta** rispetto alla convenzione interna EMS. Il Trainer ha individuato l'inversione entro la deadline di 10 s e ha protetto l'impianto rientrando in `SHADOW`.

---

### Test 2 — LIVE + Trainer ±200 W con `invert_live_sign = true` (`v5.1-live-trial-1`)
- **Esito**: **Inseguimento corretto in modulo e segno**, ma **falso mismatch al campione 44** (`09:49:04Z`, `LIVE_TRAINER_MISMATCH_NATIVE_RESTORED`).
- **Evidenza** (`live_trial_capture_2.ndjson`, 158 campioni):
  - `09:47:41 – 09:48:53Z`: ha inseguito correttamente `200 W` (`battery_w = +210..+226 W`), `0 W` (`battery_w = -1..-3 W`), `200 W` (`battery_w = +207..+209 W`).
  - `09:48:56 – 09:49:04Z`: su oscillazioni rapide di carico (`156 → 200 → 179 → 200 W`), al campione 44 il setpoint era `200 W` e la batteria era a **`202 W`** (errore `2 W`), ma è scattato il timeout.
- **Bug individuato (`src/v51/trainer_response.h`)**:
  - `TrainerResponse::confirmed()` aggiornava `changed_ = now` (resettando la finestra di settling di `3 s`) ad ogni cambio setpoint, **ma non resettava `started_ = now`** (deadline di `10 s`). Così, dopo cambi ravvicinati, il nuovo setpoint non aveva nemmeno i `3 s` di settling prima dello scadere dei `10 s`.

---

### Test 3 — Certificazione Trainer 5 minuti ±200 W post-fix (`v5.1-live-trial-1` patchato)
- **Esito**: **SUPERATO AL 100%** (`LIVE_TRAINER_COMPLETE_NATIVE_RESTORED`).
- **Evidenza** (`live_trial_capture_3.ndjson`, 159 campioni totali, **133 campioni consecutivi in `LIVE`**):
  - `09:59:21 – 10:04:23Z`: nessun mismatch, rinnovi periodici ogni 30 s e transizioni `200 W ↔ 0 W` gestite correttamente (`battery_w = +209..+214 W` a setpoint `200 W`).
  - Ripristino automatico del native mode (`0x09C4 = 1`, `SELF_USE`) allo scadere dei 300 s.
- **Verifica portale inverter**: confermato visivamente che durante `LIVE` l'inverter passa in `ExtEms` (`0x09C4 = 5`), `External EMS Mode = Controlled charging and discharging` (`0x0A08 = 0`), `Power Controlled by External EMS = -200` (`0x0A09`).

---

### Test 4 — Trainer "spinto" a piena potenza (no cap 200 W, finestra 30 min, `v5.1-live-trial-2`)
- **Modifiche applicate in `v5.1-live-trial-2`**:
  - `V51_BUILD_ID` aggiornato a `"v5.1-live-trial-2"`.
  - Rimosso il clamp `±200 W` in `src/v51/live_trial.cpp` (usa i bound completi di Safety) e finestra Trainer portata a `1800 s` (30 min).
  - Tolleranza Trainer in `src/v51/trainer_response.h` scalata a `max(60 W, 15% del setpoint)`.
- **Esito**: **10m 33s in `LIVE` senza mismatch del Trainer**, interrotto alle `10:35:52Z` per **`LIVE_TRIAL_FAULT_STOPPED`** (`ReadbackFailed`), con ripristino riuscito del native mode.
- **Evidenza** (`live_trial_capture_30m_v2.ndjson`, 623 campioni totali, **278 campioni in `LIVE`**):
  - `live_trial_w`: `568 W – 1857 W` (media `1292.7 W`), **551 scritture Modbus confermate**.
  - Errore `battery_w - w`: media `+80.9 W`, errore assoluto medio `120.9 W` → **0 mismatch Trainer**.
  - Oscillazione ciclo-su-ciclo: salto medio consecutivo `|ΔW| = 165.8 W` (mediana `103 W`, max `712 W`), `grid_w` tra `-400 W` e `+444 W` (media `+56.1 W`).

---

## 2. Problemi aperti individuati

1. **Oscillazione / Hunting a piena potenza (`|ΔW| = 165.8 W`, `grid_w` `-400..+444 W`)**
   - **Causa**: somma di tre ritardi fisici (~1–2 s complessivi tra scrittura `0x0A09` e aggiornamento di `grid_w`/`battery_w`: polling a blocchi ogni 400 ms → `ages_ms` fino a 1.5 s, RTOS inverter 300–800 ms, rampa interna BMS/inverter 1–2 s) combinata con correzione al 100% (guadagno `1.0`) ad ogni ciclo da `1 s`.
   - Il controllore reagisce al proprio ritardo di attuazione anche a carico costante.

2. **Frequenza eccessiva di scritture Modbus (~55 scritture/min, 551 in 10.5 min) e stop su singolo glitch (`LIVE_TRIAL_FAULT_STOPPED`)**
   - A causa dell'oscillazione continua di `simulated_w`, quasi ogni secondo viene inviata una nuova scrittura FC10 + readback FC03 su `0x0A09`.
   - In `LiveExecutor::tick()` (`src/v51/live_executor.h`), un singolo `writeAndReadBack()` fallito su 550+ transazioni causa subito `ReadbackFailed` e arresta la sessione `LIVE`.

3. **Badge UI `SHADOW · 0 SCRITTURE` non aggiornato in `LIVE`**
   - Nella dashboard web, la pillola in alto a destra continua a mostrare `SHADOW · 0 SCRITTURE` anche quando `/api/v1/manual` è in `LIVE` con scritture attive.

---

## 3. Soluzioni proposte (per consulto prima di intervenire)

1. **Smorzamento anello di controllo (mantenendo polling e reazione a `1 s`)**:
   - **Deadband su errore piccolo di `grid_w`** (es. `±50 W`): se `|grid_w| ≤ 50 W`, mantenere il setpoint invariato senza riscrivere `0x0A09`. Su un carico reale da `1500 W`, `grid_w` esce subito dalla deadband e reagisce al ciclo da `1 s` successivo.
   - **Guadagno proporzionale smorzato (`P = 0.5..0.6` invece di `1.0`)** o filtro EMA su `grid_w` nella correzione di carica/scarica, per convergere in 2–3 s senza rimbalzare `-300 ↔ +300 W`.
2. **Resilienza RS485 su `writeAndReadBack`**:
   - Valutare **1 retry immediato** su errore transiente di readback prima di dichiarare `ReadbackFailed` e uscire in `SHADOW` (mantenendo il fail-closed su doppio fallimento o perdita controllo bus).
3. **Allineamento UI badge**:
   - Aggiornare la pillola header in `dashboard.html` dai campi `operating_mode` e `modbus_write_count` di `/api/v1/manual` (o `/api/v1/health`).
