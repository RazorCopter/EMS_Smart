# Gate G1 Formal Readiness Dossier & Boundary Halt

**Progetto:** Heliograph / Afore Autonomous EMS  
**Branch:** `g1-readiness-consolidated`  
**Data di Emissione:** 2026-11-11  
**Stato Gate:** **PRONTO PER REVISIONE GOVERNANCE G1 — LAVORO OFFLINE CONCLUSA CON SUCCESSO**  
**Direttiva di Arresto:** **FORMAL HALT BEFORE MILESTONE M7 (NO PHYSICAL RS485 PROBING PERMITTED)**

---

## 1. Executive Summary

Il presente dossier raccoglie e certifica le evidenze tecniche e di sicurezza sviluppate durante le milestone **M0, M1, M2, M3A, M3B, M4, M5 e M6**, consolidando l'intero audit di conformità pre-G1.

Tutte le componenti sono state verificate al **100% in modalità offline**, senza accesso a porte seriali fisiche e senza connessioni Modbus all'inverter Afore AF5K-SL.

---

## 2. Esito Dettagliato delle 5 Verifiche dell'Audit Pre-G1

### Verifica 1: Allineamento Requisiti M4 (Smart EMS)
- **Risoluzione Finestre Temporali:**
  - `targetMinuteOfDay = 930` (15:30).
  - `anticipationMinutes = 20` (deadline operativa 15:10, min 910).
  - `recoveryMinutes = 60` (finestra di recupero 14:30..15:30).
  - Chiarito formalmente che "13:00" non è una regola hardcoded né un target anticipato arbitrario, bensì il risultato dinamico nello scenario `sunny_day` con curva FV estiva.
- **Tracciabilità 1:1:** Matrice documentata in [`docs/M4_REQUIREMENTS_AUDIT.md`](M4_REQUIREMENTS_AUDIT.md) per tutti i 59 requisiti approvati.
- **Test Suite:** 17 scenari CTest implementati in [`test/test_target_aware_ems/test_main.cpp`](../test/test_target_aware_ems/test_main.cpp) con esito **17/17 PASS**.

### Verifica 2: Risoluzione Conflitto FC03/FC04 e Discrepanze Registri (0x07D2 SOC)
- **Risoluzione Documentale del Function Code (FC03 vs FC04):**
  - L'audit riga per riga del documento ufficiale Afore v3.4 (`艾伏逆变器寄存器参数表（对外）v3.4_20251119_并网机和储能机分类(2).xlsx`) certifica che il registro dinamico `0x07D2` (2002 decimale, `电池电量（SOC）`) risiede al rigo 163 del foglio **`运行信息04`** ed è codificato come **Function Code 04 (Read Input Registers)**.
  - Nessuna inferenza generica a FC03 è ammessa. Il profilo [`profiles/afore/af5k_sl.toml`](../profiles/afore/af5k_sl.toml), la fixture sintetica e la documentazione di provenienza sono stati tutti allineati su **FC 0x04** (`space = "input"`).
- **Riconciliazione Valori SOC (27% vs 80%):**
  - Valore 27% (`0x001B`): lettura qualitativa da interfaccia cloud Solarman, **nessun frame binario seriale reale è mai stato archiviato**.
  - Valore 80% (`0x0050`): presente nella fixture sintetica [`test/test_afore/fixtures/af5k_sl_sample.json`](../test/test_afore/fixtures/af5k_sl_sample.json).
  - Classificazione formale: **`PROVISIONAL_FIXTURE_VERIFIED`** in [`docs/AFORE_REGISTER_PROVENANCE.md`](AFORE_REGISTER_PROVENANCE.md) con tabella a 7 colonne per tutti i registri.
- **Separazione Rigorosa G1 / G2 e Piano Operativo M7:**
  - **Eliminati da M7:** Polling periodico SOC ogni 15s, polling potenze ogni 10s, loop ricorrenti, ritrasmissioni automatiche ed endurance (interamente posticipati a post-G2).
  - **Unico Probe M7 Bounded One-Shot autorizzato:**
    * Target: Inverter Afore AF5K-SL, Slave ID: 1
    * Modbus Function: **FC 0x04** (Read Input Registers)
    * Start Address: `0x07D2` (2002), Quantità: 1 registro
    * Frame TX esatto (8 byte): `01 04 07 D2 00 01 90 87`
    * Risposta attesa: 7 byte esatti (`01 04 02 [val_hi] [val_lo] [crc_lo] [crc_hi]`)
    * Validazione CRC-16 Modbus obbligatoria
    * Timeout <= 1000 ms, zero retry, interruzione immediata su errore
    * Lockout scritture verificato con ReadOnlyGuard prima e dopo la richiesta.

### Verifica 3: Audit Dati Storici Home Assistant (M5 Replay)
- **Stato Dati Reali:** Confermato che nel workspace locale non risiedono dump CSV/database reali della produzione HA per il periodo 29/10–09/11. Nessun dato reale è stato falsificato o inventato.
- **Validazione Fixture:** Il tool [`tools/ha_replay.py`](../tools/ha_replay.py) è completamente testato e conforme su 4 scenari realistici sintetici (96 step ciascuno a passo 15 minuti): `sunny_day`, `foggy_day`, `evening_load_spike`, `missing_forecast_day`.
- **Procedura di Importazione:** Documentata in [`docs/M5_HA_DATA_AUDIT.md`](M5_HA_DATA_AUDIT.md). Il replay su storico reale dell'impianto è registrato come verifica pendente a disposizione dell'operatore.

### Verifica 4: Portabilità e Compilazione ARM64 Raspberry Pi
- **Analisi Architetturale:** Codice C++17 puro, standard types `<cstdint>`, endianness agnostico su unpacking big-endian, POSIX standard `<termios.h>` serial I/O.
- **Stato Compilazione:** 100% test superati in ambiente locale Debian GNU Linux x86_64 GCC 13.3.0.
- **Trasparenza Hardware:** Poiché il container di sviluppo non include il cross-compiler ARM64 (`NO_AARCH64_CROSS`), l'esecuzione su silicio Raspberry Pi nativo (Debian 13 trixie aarch64 / GCC 14.2.0) è registrata come **Verifica Pendente Pre-Esecuzione Fisica**.
- **Automazione:** Script one-liner creato in [`scripts/verify_arm64.sh`](../scripts/verify_arm64.sh). Documentazione completa in [`docs/ARM64_PORTABILITY_ASSESSMENT.md`](ARM64_PORTABILITY_ASSESSMENT.md).

### Verifica 5: Audit di Sicurezza e Confini ReadOnlyGuard Completi
- **Politica di Autorizzazione Rigorosa (Allowlist Stretta):**
  Il runtime e il guard `ReadOnlyGuard` sotto `AFORE_SHADOW_BUILD=ON` consentono unicamente ed esclusivamente:
  - **FC 0x03** (Read Holding Registers);
  - **FC 0x04** (Read Input Registers).
- **Blocco Tassativo e Verificato da Test Automatizzati (Matrice Test Host A-Y):**
  Il guard analizza semanticamente il frame Modbus RTU prima di qualsiasi scrittura su filo seriale ed intercetta con `0 byte trasmessi`:
  - **FC 0x01** (Read Coils) -> BLOCKED (`FUNCTION_NOT_ALLOWED`, Test C);
  - **FC 0x02** (Read Discrete Inputs) -> BLOCKED (`FUNCTION_NOT_ALLOWED`, Test D);
  - **FC 0x05** (Write Single Coil) -> BLOCKED (`FUNCTION_NOT_ALLOWED`, Test E);
  - **FC 0x06** (Write Single Register) -> BLOCKED (`FUNCTION_NOT_ALLOWED`, Test F);
  - **FC 0x0F** (Write Multiple Coils) -> BLOCKED (`FUNCTION_NOT_ALLOWED`, Test G);
  - **FC 0x10** (Write Multiple Registers) -> BLOCKED (`FUNCTION_NOT_ALLOWED`, Test H);
  - **FC 0x17** (Read/Write Multiple Registers) -> BLOCKED (`FUNCTION_NOT_ALLOWED`, Test I);
  - **Function Codes sconosciuti** (es. `0x99`) -> BLOCKED (`FUNCTION_NOT_ALLOWED`, Test J);
  - **Slave 0 (Broadcast Modbus)** -> BLOCKED (`BROADCAST_NOT_ALLOWED`, Test N);
  - **Slave ID non corrispondente** -> BLOCKED (`SLAVE_NOT_ALLOWED`, Test O);
  - **Frame vuoti (`len == 0`)** -> BLOCKED (`EMPTY_FRAME`, Test K);
  - **Frame corti (`len < 8`)** -> BLOCKED (`SHORT_FRAME`, Test L);
  - **Frame con byte spuri / trailing bytes (`len != 8`)** -> BLOCKED (`INVALID_LENGTH`, Test R);
  - **CRC-16 Modbus non valido** -> BLOCKED (`INVALID_CRC`, Test M);
  - **CRC byte order errato** -> BLOCKED (`INVALID_CRC`, Test Y);
  - **Quantità zero (`quantity == 0`)** -> BLOCKED (`QUANTITY_ZERO`, Test P);
  - **Quantità fuori limite Modbus RTU (`quantity > 125`)** -> BLOCKED (`QUANTITY_OUT_OF_RANGE`, Test Q);
  - **Overflow spazio indirizzi (`start_address + quantity > 0x10000`)** -> BLOCKED (`ADDRESS_OUT_OF_RANGE`, Test T).
- **Immutabilità Compile-Time (Negative Compile Test Superato):**
  Sotto `-DAFORE_SHADOW_BUILD=1`, il metodo `setForceReadOnly()` non è presente nella classe (racchiuso in `#if !defined(AFORE_SHADOW_BUILD)`). Qualsiasi tentativo di compilare codice che tenti di disattivare il guard fallisce tassativamente a compile-time (verificato via script ed esecuzione del compilatore GCC).
- **Profili TOML & Assenza Rischi EEPROM:**
  [`profiles/afore/af5k_sl.toml`](../profiles/afore/af5k_sl.toml) non espone alcuna capability di scrittura (`write.none()`). Registri di controllo esterno e registri su EEPROM (`0x0BC1`, `0x0BC2`) sono completamente omessi.
- **Rapporto di Audit Completo:** Certificato in [`docs/SECURITY_AUDIT_REPORT.md`](SECURITY_AUDIT_REPORT.md).

---

## 3. Riepilogo File Modificati e Creati nel Branch `g1-readiness-consolidated`

1. [`src/ems/target_aware_ems.h`](../src/ems/target_aware_ems.h): Aggiunto campo `fullSocCalibrationDurationSeconds = 300` a `EMSConfig`.
2. [`src/ems/target_aware_ems.cpp`](../src/ems/target_aware_ems.cpp): Utilizzo del parametro configurabile di calibrazione al posto del valore costante.
3. [`test/test_target_aware_ems/test_main.cpp`](../test/test_target_aware_ems/test_main.cpp): Implementati scenari per audit parametri approvati, finestre 15:30/20m/60m e validazione dati non convertiti a zero (17 scenari totali).
4. [`docs/M4_REQUIREMENTS_AUDIT.md`](M4_REQUIREMENTS_AUDIT.md): Matrice tracciabilità 1:1 per tutti i 59 requisiti e chiarimento 13:00.
5. [`docs/AFORE_REGISTER_PROVENANCE.md`](AFORE_REGISTER_PROVENANCE.md): Riconciliazione 27% vs 80%, riclassificazione `0x07D2` in `PROVISIONAL_FIXTURE_VERIFIED`.
6. [`docs/GAP_ASSESSMENT.md`](GAP_ASSESSMENT.md) & [`docs/PHASE0_PROGRESS_REPORT.md`](PHASE0_PROGRESS_REPORT.md): Allineamento della classificazione provvisoria dei registri.
7. [`docs/M5_HA_DATA_AUDIT.md`](M5_HA_DATA_AUDIT.md): Stato audit esportazioni HA reali e validazione fixture M5.
8. [`scripts/verify_arm64.sh`](../scripts/verify_arm64.sh): Script di validazione nativa su Raspberry Pi target.
9. [`docs/ARM64_PORTABILITY_ASSESSMENT.md`](ARM64_PORTABILITY_ASSESSMENT.md): Documento di conformità e portabilità ARM64.
10. [`docs/SECURITY_AUDIT_REPORT.md`](SECURITY_AUDIT_REPORT.md): Rapporto indipendente sui confini di sicurezza read-only.
11. [`docs/GATE_G1_DOSSIER.md`](GATE_G1_DOSSIER.md): Presente documento di handoff formale.

---

## 4. Direttiva di Arresto Tassativa (Halt Directive)

> **AVVISO DI SICUREZZA E ARRESTO FORMALE:**  
> In conformità alle direttive di progetto, **QUALSIASI ATTIVITÀ RELATIVA ALLA MILESTONE M7 (CONNESSIONE FISICA RS485, APERTURA PORTA SERIALE, PROBING HARDWARE DELL'INVERTER) È FORMALMENTE SOSPESA E INTERDETTA**.  
> Nessun comando di interazione hardware reale deve essere eseguito fino a quando la revisione umana non avrà approvato formalmente il Gate G1 e rilasciato l'autorizzazione al Gate G2.
