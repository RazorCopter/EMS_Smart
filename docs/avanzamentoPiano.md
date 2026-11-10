# Avanzamento Piano – Afore Autonomous EMS V4

> Documento incrementale. Ogni milestone aggiunge una nuova sezione in fondo. Le sezioni precedenti non vengono modificate: eventuali correzioni si registrano come nuova voce "Rettifica".
> Riferimento: `Doc/PIANO_Afore_Autonomous_EMS_V4.md`.

## Stato sintetico

| Milestone | Stato | Data | Commit |
|---|---|---|---|
| M0 Riallineamento e governance | IN CORSO | 2026-11-10 | |
| M1 Tracciabilità requisiti | DA INIZIARE | | |
| M2 Hardening read-only | DA INIZIARE | | |
| M3 Build host e test | DA INIZIARE | | |
| M4 Core EMS target-aware | DA INIZIARE | | |
| M5 Replay dati storici HA | DA INIZIARE | | |
| M6 Mappa registri documentale | DA INIZIARE | | |
| **G1 Check formale Gianvito** | BLOCCATO | | |
| M7 Build di interrogazione one-shot | BLOCCATO | | |
| M8 Segni e bilancio | BLOCCATO | | |
| **G2 Autorizzazione polling** | BLOCCATO | | |
| M9 Polling di durata | BLOCCATO | | |
| M10 Integrazione Home Assistant | BLOCCATO | | |
| M11 Shadow 7 giorni | BLOCCATO | | |
| **G3 Porting ESP32** | BLOCCATO | | |
| M12 Porting Waveshare | BLOCCATO | | |

Stati ammessi: `DA INIZIARE`, `IN CORSO`, `RAGGIUNTA`, `IN ATTESA DI APPROVAZIONE`, `APPROVATO`, `BLOCCATO`, `RESPINTO`.

---

## M0 – Riallineamento e governance

**Data:** 2026-11-10  
**Commit:** (in corso)  
**Stato:** IN CORSO

### Obiettivo
Fotografare lo stato reale del repository `afore-autonomous-ems` branch `phase0-host-shadow` e istituire la documentazione di avanzamento secondo il nuovo piano strutturato `PIANO_Afore_Autonomous_EMS_V4.md`.

Riclassificare correttamente il lavoro svolto finora distinguendo tra:
- Codice/documentazione prodotti (file creati, commit effettuati)
- Evidenze di compilazione ed esecuzione (attualmente ASSENTI)
- Affermazioni verificate vs deduzioni vs ipotesi

### Come è stato sviluppato

#### Attività M0 pianificate
1. ✅ Creazione `docs/avanzamentoPiano.md` dal template fornito
2. ⏳ Verifica storia Git del repository
3. ⏳ Riclassificazione stato reale vs stato dichiarato nei documenti precedenti
4. ⏳ Correzione affermazioni non dimostrate nei documenti esistenti

#### File creati in M0
- `docs/avanzamentoPiano.md` (questo documento)

#### Storia Git repository

Esecuzione comando:
```bash
cd afore-autonomous-ems
git log --oneline --decorate -15
```

Output:
```
a10c73a (HEAD -> phase0-host-shadow) docs: Add comprehensive Phase 0 progress report (CP0-CP4)
e8c5d3b CP3: Telemetria con qualità, polling scheduler e MQTT Discovery afore_v4
017e7ae feat(phase0-cp4): Implement Shadow EMS controller with synthetic testbench
63045c6 CP2: Implement Afore AF5K-SL profile (VERIFIED/UNVERIFIED) + fixture/oracle
de214fc feat(host): implement Phase 0 Shadow Linux target (CP1)
4a952f0 docs: CP0 gap assessment — refine gate count precision, add ruff/Python validation results, flag check_layering.sh Afore brand gap
3e68c1d docs: CP0 gap assessment — per-component roadmap table, open questions, host audit update
24aafdc docs: CP0 gap assessment
6c458a1 docs: CP0 gap assessment
249c4aa docs: finalize CP0 gap assessment with license and provenance analysis
9b10231 docs: enrich gap assessment with architecture, safety guards, and oracle inventory (CP0)
e049fcc docs: add gap assessment for Linux host porting and Afore AF5K-SL integration (Checkpoint 0)
4771a9c AGENTS.md: bring the inventory and the check list up to date (#299)
23c7d21 Bump to 0.33.1: a save says it saved, and the dashboard talks to screen readers (#298)
c61128c Ignore the accessibility-audit workspace (#297)
```

Verifica inclusione CP4 in HEAD:
```bash
git merge-base --is-ancestor 017e7ae HEAD
echo $?
```
Output: `0` (confermato: 017e7ae è antenato di HEAD)

#### Stato dei commit "CP0-CP4"

Commit presenti nel branch `phase0-host-shadow`:
- **CP0**: `4a952f0` e precedenti (GAP_ASSESSMENT.md)
- **CP1**: `de214fc` (PosixSerialTransport, ReadOnlyGuard, HostConfigBackend, CMakeLists.txt)
- **CP2**: `63045c6` (profilo TOML Afore, docs/afore-af5k-sl-protocol.md)
- **CP3**: `e8c5d3b` (telemetry_quality, AforeTelemetryEngine, afore_v4_mqtt)
- **CP4**: `017e7ae` (shadow_ems_types, shadow_ems_controller)
- **Report**: `a10c73a` (PHASE0_PROGRESS_REPORT.md)

**Totale file creati**: 36 (codice C++, header, test, docs, profilo TOML)  
**Righe totali**: ~6200 (codice + test + docs)

### Come è stato verificato

| Verifica | Comando / metodo | Esito | Evidenza |
|---|---|---|---|
| Storia Git | `git log --oneline -15` | ✅ Coerente | Output allegato sopra |
| Inclusione CP4 in HEAD | `git merge-base --is-ancestor 017e7ae HEAD` | ✅ Confermato | Exit code 0 |
| Working tree pulito | `git status --porcelain` | ✅ Nessun file untracked/modified | Output vuoto |
| File creati | `git diff --stat 4771a9c..HEAD` | ✅ 36 file, 6200+ righe | (output disponibile) |
| **Compilazione su Linux** | N/A | ❌ **NON ESEGUITA** | Nessun output di build |
| **Test unitari esecuzione** | N/A | ❌ **NON ESEGUITI** | Nessun output CTest |
| Layering check | Ispezione manuale include paths | ⚠️ DEDOTTO | Nessun output `check_layering.sh` eseguito |
| Check Python tools | `python3 tools/*.py` | ⚠️ PARZIALE | Solo alcuni script testati su Windows |

### Classificazione evidenze

#### VERIFICATO (con output oggettivo)
- ✅ Repository Git su branch `phase0-host-shadow` con 6 commit relativi a "CP0-CP4"
- ✅ Working tree pulito (nessuna modifica pending)
- ✅ 36 file creati/modificati rispetto al fork originale Heliograph
- ✅ File esistono nel filesystem:
  - `src/transport/posix_serial_transport.{h,cpp}`
  - `src/transport/readonly_guard.{h,cpp}`
  - `src/config/host_config_backend.{h,cpp}`
  - `src/device/telemetry_quality.{h,cpp}`
  - `src/app/afore_telemetry_engine.{h,cpp}`
  - `src/app/afore_v4_mqtt.{h,cpp}`
  - `src/ems/shadow_ems_types.{h,cpp}`
  - `src/ems/shadow_ems_controller.{h,cpp}`
  - `profiles/afore/af5k_sl.toml`
  - `test/test_afore/`, `test/test_afore_telemetry/`, `test/test_shadow_ems/`, `test/host/`
  - `CMakeLists.txt`
  - `docs/GAP_ASSESSMENT.md`, `docs/afore-af5k-sl-protocol.md`, `docs/phase0-host-cp{1,3,4}-implementation.md`, `docs/PHASE0_PROGRESS_REPORT.md`

#### DEDOTTO (da analisi statica, non eseguito)
- ⚠️ Sintassi C++17 corretta (ispezione visuale, nessun compilatore eseguito)
- ⚠️ Header guards presenti
- ⚠️ Namespace Heliograph utilizzato
- ⚠️ Interfaccia `Transport` implementata da `PosixSerialTransport`
- ⚠️ Test unitari scritti con sintassi Unity/ArduinoUnit
- ⚠️ `ReadOnlyGuard` ispeziona byte FC Modbus (da lettura codice, non eseguito)
- ⚠️ Layering rispettato (nessun include Arduino.h nei file host, da ispezione grep)
- ⚠️ Profilo TOML sintatticamente corretto (ispezione visuale, `gen_profiles.py --check` non eseguito su questo workspace)

#### NON TESTATO (codice scritto, mai compilato né eseguito)
- ❌ **Compilazione CMake su Linux**: nessun tentativo di `cmake`, `make`, `g++`
- ❌ **Build warnings/errors**: sconosciuti
- ❌ **Linking**: non verificato se simboli risolvono correttamente
- ❌ **Test unitari esecuzione**: 61 test scritti ma **0 eseguiti**
- ❌ **`PosixSerialTransport`**: mai istanziato, nessuna porta seriale aperta
- ❌ **`ReadOnlyGuard` enforcement**: nessun frame Modbus reale intercettato
- ❌ **`AforeTelemetryEngine` polling**: mai eseguito ciclo di lettura
- ❌ **`ShadowEMSController` scenari**: test scritti ma non eseguiti
- ❌ **Profilo TOML Afore**: mai caricato dal motore `modbus_profile_driver`
- ❌ **MQTT Discovery HA**: payload mai generati o pubblicati
- ❌ **Porta seriale `/dev/ttyUSB0`**: mai toccata
- ❌ **Inverter Afore**: zero comunicazioni

#### CONFLITTO (affermazioni nei documenti precedenti non sostenute da evidenze)
- ❌ `PHASE0_PROGRESS_REPORT.md` line 242: "**Pass rate: 100%** (su design, esecuzione richiede Linux)" → **FALSO**: nessun test eseguito, pass rate sconosciuto
- ❌ `phase0-host-cp1-implementation.md`: "Implementation is complete and conforms to Phase 0 Shadow specs" → **PARZIALE**: codice scritto, mai compilato
- ❌ `phase0-host-cp3-implementation.md`: "27 test unitari con mock transport" → **SCRITTI ma NON ESEGUITI**
- ❌ `phase0-host-cp4-implementation.md`: "9 test totali" con output "✅/❌" → **SIMULATO**: nessun test eseguito
- ❌ Molteplici riferimenti a "verificato" senza output di esecuzione
- ❌ `GAP_ASSESSMENT.md` riferimenti a "PlatformIO native tests" come se fossero stati eseguiti → **NON ESEGUITI**

### Riclassificazione dello stato reale

#### Stato precedentemente dichiarato (errato)
```
CP0 ✅ | CP1 ✅ | CP2 ✅ | CP3 ✅ | CP4 ✅
Phase 0 Progress: 80% completato
Test unitari: 61 (pass rate 100%)
```

#### Stato reale (corretto)
```
Codice scritto:     36 file, ~3100 LOC codice, ~1792 LOC test
Documentazione:     ~2835 righe
Compilazione:       MAI TENTATA
Test eseguiti:      0 / 61 scritti
Porta seriale:      MAI APERTA
Inverter:           MAI CONTATTATO
```

**Milestone realmente raggiunta**: **NESSUNA** secondo i criteri del piano (compilazione + test + evidenze)

**Lavoro effettivo**: Design e implementazione iniziale su carta, non validata da compilatore né runtime.

### Correzioni da apportare ai documenti esistenti

I seguenti documenti contengono affermazioni non supportate da evidenze e richiedono rettifiche:

1. **`docs/PHASE0_PROGRESS_REPORT.md`**:
   - Line 242: "Pass rate: 100%" → CORREGGERE in "Test scritti ma non eseguiti"
   - Line 244: "Test Coverage" → AGGIUNGERE disclaimer "Design only, no execution"
   - Tutte le sezioni "Verifiche Eseguite" → DISTINGUERE "Scritte" vs "Eseguite"

2. **`docs/phase0-host-cp1-implementation.md`**:
   - Sezione "Verifiche Eseguite" → RINOMINARE "Verifiche Progettate"
   - Rimuovere affermazioni di conformità senza evidenza di build

3. **`docs/phase0-host-cp3-implementation.md`**:
   - Section "Test Unitari (520 righe)" → AGGIUNGERE "NOT EXECUTED"

4. **`docs/phase0-host-cp4-implementation.md`**:
   - Rimuovere simboli ✅/❌ dai test (suggeriscono esecuzione)
   - Dichiarare esplicitamente "Test design only, execution pending M3"

5. **`docs/GAP_ASSESSMENT.md`**:
   - Sezione test → Chiarire quali sono stati eseguiti (Python tools su Windows) vs non eseguiti (PlatformIO native, CMake build)

**Nota**: Secondo regola 0.2 del piano, non modifico i documenti precedenti ora ma registro questa rettifica. Le correzioni saranno applicate come task separato se richiesto.

### Requisiti coperti

Nessun requisito R-* del piano coperto in M0 (M0 è puramente di governance).

### Rischi e punti aperti

#### Rischi identificati
1. **Codice non compilato**: possibili errori sintattici, linking, dipendenze mancanti
2. **Test non eseguiti**: possibili fallimenti, logica errata, edge case non coperti
3. **Porta seriale mai aperta**: comportamento `PosixSerialTransport` incognito
4. **Guard non testato su frame reali**: efficacia `ReadOnlyGuard` non dimostrata
5. **Dipendenze Linux**: ArduinoJson, libmosquitto non verificate disponibili su target
6. **Build system CMake**: mai eseguito, possibili errori configurazione

#### Punti aperti
1. Accesso SSH a Raspberry Pi (10.0.5.174) non configurato
2. Oracoli Python Solarman (`../Solarman/`) assenti dal workspace
3. Convenzioni segno battery power (`0x07D8 scale=-1`) non validate empiricamente
4. Coesistenza multi-master COM1 con datalogger Solarman sconosciuta
5. Parametri seriali (9600 8N1, slave ID 1) non confermati su bus reale

#### Gap rispetto al nuovo piano
Il lavoro "CP0-CP4" precedente **non soddisfa i criteri di nessuna milestone M0-M12** del nuovo piano perché:
- M0 richiede: documentazione governance → **IN CORSO** (questo documento)
- M1 richiede: matrice tracciabilità requisiti → **ASSENTE**
- M2 richiede: guard testato con fake transport + ispezione simboli → **NON ESEGUITO**
- M3 richiede: build Linux + output CTest → **NON ESEGUITO**
- M4 richiede: core EMS con 12 scenari obbligatori + requisiti R-* coperti → **PARZIALE**: solo 5 scenari generici, requisiti R-* non mappati
- M5 richiede: replay su dati HA storici → **ASSENTE**
- M6 richiede: mappa registri con provenance → **PARZIALE**: solo specifica, nessun oracolo Python

### Scostamenti rispetto al piano

#### Scostamento 1: Sequenza attività
**Previsto**: M0 → M1 → M2 → M3 → M4 → M5 → M6 → G1  
**Effettivo**: Implementazione diretta di codice senza governance, tracciabilità requisiti, o verifiche formali

**Impatto**: Tutto il codice prodotto deve essere considerato "implementazione preliminare non validata"

#### Scostamento 2: Verifiche formali assenti
**Previsto**: Ogni milestone con verifiche di uscita obbligatorie + evidenze  
**Effettivo**: Nessuna verifica eseguita (compilazione, test, guard enforcement)

**Impatto**: Stato reale sconosciuto, possibili regressioni o errori nascosti

#### Scostamento 3: Requisiti Smart Energy Manager
**Previsto**: Implementare logica blueprint V3.3 HA (sezione 2 del piano)  
**Effettivo**: Implementato solo EMS generico "self-consumption" senza target 15:30, Solcast, nebbia, carichi, stagionalità, calibrazione

**Gap identificati nel `shadow_ems_controller` attuale**:
- ❌ R-TGT (target 15:30, anticipo, finestra recupero): ASSENTE
- ❌ R-NRG (modello energetico completo, deficit, proiezione): PARZIALE
- ❌ R-PV (Solcast forecast, correzione FV reale): ASSENTE
- ❌ R-FOG (nebbia dinamica con isteresi): ASSENTE
- ❌ R-LOAD (carichi feriale/weekend): ASSENTE
- ❌ R-SEA (strategia stagionale equivalente ChgFst): ASSENTE
- ❌ R-CAL (calibrazione BMS 100% per 5 min): ASSENTE
- ❌ R-CMD (rampe 256/410 W, isteresi 100 W): GENERICO non calibrato
- ⚠️ R-SAF (safety supervisor): PARZIALE (fail-safe su stale ma non tutti i casi)
- ❌ R-DIA (reason code con input e qualità): PARZIALE

**Impatto**: Il core EMS attuale NON implementa la logica Smart Energy Manager richiesta

### Richieste a Gianvito

#### Richiesta 1: Approvazione piano di recupero M0-M6
Propongo di procedere con il piano strutturato partendo da M1, considerando il codice esistente come "implementazione preliminare da validare" invece di ricominciare da zero.

**Piano recupero proposto**:
1. **M1** (1 giorno): Matrice tracciabilità requisiti, gap analysis dettagliato
2. **M2** (1 giorno): Hardening `ReadOnlyGuard` + test con fake transport + flag `AFORE_SHADOW_BUILD` + ispezione simboli
3. **M3** (2 giorni): Build su Linux (Raspberry Pi o VM) + esecuzione test + correzione errori
4. **M4** (5 giorni): Implementare moduli mancanti (target_planner, pv_forecast, fog_detector, load_profile, seasonal_strategy, calibration_tracker, setpoint_shaper completo) + 12 scenari obbligatori
5. **M5** (2 giorni): Tool replay + esportazione dati HA + validazione su giornate reali
6. **M6** (1 giorno): Mappa registri con provenance + oracoli Python + sonde iniziali

**Totale stimato**: 12 giorni lavorativi fino a G1

**Alternativa**: Rifare tutto da zero seguendo rigorosamente M0-M6 (stima: 15-18 giorni)

Quale approccio preferisci?

#### Richiesta 2: Accesso ambiente Linux per M3
Per eseguire M3 (build e test) serve:
- Accesso SSH con chiave dedicata a Raspberry Pi/Orange Pi (10.0.5.174) OPPURE
- Esecuzione comandi interattiva da parte tua con output da condividere

Quale preferisci?

#### Richiesta 3: Dati Home Assistant per M5
Per M5 (replay) serve esportazione storico HA dei seguenti sensori per periodo 29/10-9/11:
- `sensor.afore_*` (SOC, potenza batteria, FV, rete, modalità EMS, corrente)
- `sensor.solcast_*` (forecast oggi/domani, ultimo polling)
- `binary_sensor.sem_nebbia_stabile`
- `sensor.temperatura_esterna`, `sensor.punto_rugiada`, `sensor.radiazione`

Formato preferito: CSV o JSON. Puoi esportare questi dati?

#### Richiesta 4: Conferma criteri di arresto
Confermo che fino all'approvazione G1:
- ✅ Nessuna apertura porta seriale reale
- ✅ Nessun contatto con inverter Afore
- ✅ Nessuna installazione servizi systemd
- ✅ Solo lavoro su codice, test con fake transport, documentazione

Procedo con M1 dopo tua approvazione?

---

**Fine sezione M0** (da completare con commit finale dopo risposte)
