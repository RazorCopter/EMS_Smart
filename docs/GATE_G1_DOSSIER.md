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

### Verifica 2: Risoluzione Discrepanze Registri (0x07D2 SOC)
- **Riconciliazione Documentale:**
  - Valore 27% (`0x001B`): emerso durante note preliminari qualitative di setup da dashboard, **nessun frame binario hardware esiste nel repository**.
  - Valore 80% (`0x0050`): presente in [`test/test_afore/fixtures/af5k_sl_sample.json`](../test/test_afore/fixtures/af5k_sl_sample.json), formalmente dichiarata come *"synthetic test fixture for Phase 0 validation"*.
- **Classificazione Corretta:** Riclassificato da improprio `VERIFIED_EMPIRICALLY` a rigoroso **`PROVISIONAL_FIXTURE_VERIFIED`** in [`docs/AFORE_REGISTER_PROVENANCE.md`](AFORE_REGISTER_PROVENANCE.md), [`docs/GAP_ASSESSMENT.md`](GAP_ASSESSMENT.md) e [`docs/PHASE0_PROGRESS_REPORT.md`](PHASE0_PROGRESS_REPORT.md).
- **Probe Plan Phase 0 Confermato:** Solo query FC03 a singolo registro su `0x07D2` dopo G1/G2, inter-frame gap >= 200 ms, timeout 1000 ms, zero retry, stop al 3° errore. Registri pericolosi o EEPROM (`0x0BC1`, `0x0BC2`, `0x0A08`) rigorosamente esclusi.

### Verifica 3: Audit Dati Storici Home Assistant (M5 Replay)
- **Stato Dati Reali:** Confermato che nel workspace locale non risiedono dump CSV/database reali della produzione HA per il periodo 29/10–09/11. Nessun dato reale è stato falsificato o inventato.
- **Validazione Fixture:** Il tool [`tools/ha_replay.py`](../tools/ha_replay.py) è completamente testato e conforme su 4 scenari realistici sintetici (96 step ciascuno a passo 15 minuti): `sunny_day`, `foggy_day`, `evening_load_spike`, `missing_forecast_day`.
- **Procedura di Importazione:** Documentata in [`docs/M5_HA_DATA_AUDIT.md`](M5_HA_DATA_AUDIT.md). Il replay su storico reale dell'impianto è registrato come verifica pendente a disposizione dell'operatore.

### Verifica 4: Portabilità e Compilazione ARM64 Raspberry Pi
- **Analisi Architetturale:** Codice C++17 puro, standard types `<cstdint>`, endianness agnostico su unpacking big-endian, POSIX standard `<termios.h>` serial I/O.
- **Stato Compilazione:** 100% test superati in ambiente locale Debian GNU Linux x86_64 GCC 13.3.0.
- **Trasparenza Hardware:** Poiché il container di sviluppo non include il cross-compiler ARM64 (`NO_AARCH64_CROSS`), l'esecuzione su silicio Raspberry Pi nativo (Debian 13 trixie aarch64 / GCC 14.2.0) è registrata come **Verifica Pendente Pre-Esecuzione Fisica**.
- **Automazione:** Script one-liner creato in [`scripts/verify_arm64.sh`](../scripts/verify_arm64.sh). Documentazione completa in [`docs/ARM64_PORTABILITY_ASSESSMENT.md`](ARM64_PORTABILITY_ASSESSMENT.md).

### Verifica 5: Audit di Sicurezza e Confini Read-Only
- **Livello Read-Only:** Livello 3 attivo e incondizionato sotto macro `AFORE_SHADOW_BUILD=ON` in [`src/transport/readonly_guard.cpp`](../src/transport/readonly_guard.cpp).
- **Filtro Scritture:** FC06, FC16, broadcast slave ID 0 categoricamente rifiutati con 0 byte trasmessi.
- **Profili TOML:** [`profiles/afore_af5k_sl.toml`](../profiles/afore_af5k_sl.toml) non contiene registri di scrittura, né mapping per EEPROM.
- **Rapporto di Audit:** Certificato in [`docs/SECURITY_AUDIT_REPORT.md`](SECURITY_AUDIT_REPORT.md).

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
