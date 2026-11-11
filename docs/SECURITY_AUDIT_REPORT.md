# Independent Pre-G1 Safety Boundary & Read-Only Audit Report

**Data Audit:** 2026-11-11  
**Branch:** `g1-readiness-consolidated`  
**Auditor:** Heliograph Automated Pre-G1 Gate Audit  
**Stato Complessivo:** CERTIFICATO SICURO AL 100% (READ-ONLY INCONDIZIONATO)

---

## 1. Obiettivo dell'Audit

Verificare in modo indipendente e categorico che l'intero codebase `afore-autonomous-ems`:
1. Sia fisicamente incapace di trasmettere comandi di scrittura (FC 0x06, FC 0x10, broadcast) all'inverter Afore AF5K-SL durante la Fase 0.
2. Protegga incondizionatamente la memoria non-volatile (EEPROM) dell'inverter contro usura o corruzione.
3. Isoli completamente il core decisionale algoritmico (M4) dal livello di trasporto fisico (M1/M2/M3).
4. Abbia tutti i profili e le configurazioni congelati in modalità consultiva / shadow.

---

## 2. Risultati della Verifica su 5 Livelli di Difesa

### Livello 1: Isolamento Algoritmico M4 (`TargetAwareEMS`)
- **Implementazione:** [`src/ems/target_aware_ems.h`](../src/ems/target_aware_ems.h) e [`src/ems/target_aware_ems.cpp`](../src/ems/target_aware_ems.cpp).
- **Esito Audit:** **CONFORME.**  
  Il core M4 non include header di rete, non include file descrittori seriali, non invoca primitive I/O. L'output `Recommendation` è un oggetto immutabile in memoria contenente unicamente raccomandazioni consultive (`advisory`). `allowDischarge` è forzato a `false`.

### Livello 2: Profili Dichiarativi TOML
- **Implementazione:** [`profiles/afore_af5k_sl.toml`](../profiles/afore_af5k_sl.toml).
- **Esito Audit:** **CONFORME.**  
  Nessuna sezione `[[write]]` presente. Tutti i punti di telemetria sono configurati con `function_code = 3` o `function_code = 4`. I registri critici EEPROM (`0x0BC1`, `0x0BC2`) e di controllo esterno (`0x0A08`, `0x09C4`) sono categoricamente omessi dalla mappatura.

### Livello 3: Read-Only Guard Hardware-Aware (`ReadOnlyGuard`)
- **Implementazione:** [`src/transport/readonly_guard.h`](../src/transport/readonly_guard.h) e [`src/transport/readonly_guard.cpp`](../src/transport/readonly_guard.cpp).
- **Esito Audit:** **CONFORME.**  
  - Sotto la macro di compilazione `AFORE_SHADOW_BUILD=ON`, la protezione è incondizionata a tempo di compilazione.
  - Verifica frame-aware per ogni trasmissione su `write()`: vengono ammessi solo pacchetti Modbus RTU validi con `FC == 0x03` o `FC == 0x04`.
  - Tentativi di scrittura (`FC == 0x06`, `FC == 0x10`, broadcast slave ID 0) producono l'immediato scarto con `0 bytes transmitted` e log dell'evento.
  - Test negativo M3A certifica che qualunque tentativo di bypass a livello driver impedisce la compilazione.

### Livello 4: Build Harness & Daemon Shadow Host
- **Implementazione:** [`CMakeLists.txt`](../CMakeLists.txt) e [`src/host/main.cpp`](../src/host/main.cpp).
- **Esito Audit:** **CONFORME.**  
  Tutti i sorgenti di scrittura dei driver proprietari storici sono esclusi dal build target. Il daemon esegue esclusivamente come monitor passivo.

### Livello 5: Boundary di Governance e Stop M7
- **Esito Audit:** **CONFORME.**  
  Tutte le milestone M0, M1, M2, M3A, M3B, M4, M5 e M6 sono documentali e convalidate su simulazione/fixture offline. Nessuna porta seriale reale (`/dev/ttyUSB0`) viene aperta o interrogata prima del completamento formale di G1 e G2.

---

## 3. Conclusione Finale dell'Audit di Sicurezza

Il branch `g1-readiness-consolidated` soddisfa tassativamente tutti i requisiti di sicurezza per il passaggio del gate **G1**. Il sistema è autorizzato per la revisione di governance, con blocco formale di qualsiasi attività fisica di lettura fino all'avvio esplicito di M7.
