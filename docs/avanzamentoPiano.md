# Avanzamento Piano – Afore Autonomous EMS V4

> Documento incrementale. Ogni milestone aggiunge una nuova sezione in fondo. Le sezioni precedenti non vengono modificate: eventuali correzioni si registrano come nuova voce "Rettifica".
> Riferimento: `Doc/PIANO_Afore_Autonomous_EMS_V4.md`.

## Stato sintetico

| Milestone | Stato | Data | Commit |
|---|---|---|---|
| M0 Riallineamento e governance | RAGGIUNTA | 2026-11-10 | `561552d`, `afd1bae`, `cc09922` |
| M1 Tracciabilità requisiti | RAGGIUNTA | 2026-11-10 | commit `docs(m1): add complete Smart EMS requirements traceability` (hash verificabile nel log successivo) |
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

---

## M0 – Chiusura (rettifica e completamento)

**Data:** 2026-11-10
**Stato:** RAGGIUNTA
**Commit:** `561552d` (apertura M0), `afd1bae` (`docs(m0): apply documentation rectifications`), `cc09922` (`docs(m0): complete governance realignment and evidence corrections`)

> Questa sezione si aggiunge alla precedente senza modificarla. Le affermazioni della sezione "M0 – Riallineamento e governance" sopra che risultano superate sono rettificate qui.

### Obiettivo
Applicare ai documenti esistenti le correzioni elencate in "Correzioni da apportare ai documenti esistenti" e chiudere M0 secondo le verifiche di uscita del piano.

### Come è stato sviluppato
Correzioni applicate nel commit `afd1bae` (5 file, 190 inserimenti, 133 cancellazioni):

| File | Correzione |
|---|---|
| `docs/PHASE0_PROGRESS_REPORT.md` | Disclaimer "Design only, no runtime validation" in testa; "Pass rate: 100%" → "Test authored, execution pending M3" con `Executed: 0`; ogni sezione "Verifiche Eseguite" separata in Executed / Static inspection only / Authored, not executed; conclusioni e stato CP1–CP4 riclassificati come authored |
| `docs/phase0-host-cp1-implementation.md` | Status → "Implementation authored, validation pending M3"; "Verifica manuale effettuata" → "Verifiche Progettate"; checklist di conformità senza ✅ |
| `docs/phase0-host-cp3-implementation.md` | "27 test" → "27 test authored, execution pending M3"; target CMake marcato non verificato; esito atteso dichiarato non osservato |
| `docs/phase0-host-cp4-implementation.md` | Rimossi tutti i ✅/❌; aggiunto "Test design only, execution pending M3"; dichiarato "preliminary self-consumption controller, not target-aware Smart Energy Manager"; corretto conflitto doc/codice su `Degraded` (nel codice resta actionable, non va in `SafeIdle`) |
| `docs/GAP_ASSESSMENT.md` | Nuova §6.1.1 Executed vs Not executed (Python su Windows eseguiti; PlatformIO, CMake, `check_layering.sh` non eseguiti); "Fully tested" del codec Modbus riformulato; nota DEDOTTO sulla persistenza EEPROM di `0x0BC1` |

Le sezioni storiche di `avanzamentoPiano.md` non sono state riscritte. Fa eccezione la riga M0 della tabella "Stato sintetico", che è un indice di stato.

### Rettifica: "Milestone realmente raggiunta"
La sezione precedente riporta: *"Milestone realmente raggiunta: **NESSUNA**"*. Rettifica:

> **M0 reached after governance, M1-M6 remain not reached.**
> Il lavoro CP1–CP4 resta codice authored, mai compilato né eseguito. Non soddisfa i criteri di nessuna milestone tecnica (M2–M6).

### Rettifica: conteggi dei commit e dei file
I documenti precedenti riportano conteggi diversi: PHASE0_PROGRESS_REPORT indica "Commits: 5"; la sezione M0 indica "6 commit relativi a CP0-CP4" e "36 file, ~6200 righe". La tabella seguente li riconcilia, con output Git eseguito il 2026-11-10:

```text
git rev-list --count 4771a9c..HEAD     -> 14 (incluso afd1bae)
git log --oneline --reverse 4771a9c..HEAD
e049fcc  CP0 (1/7)
9b10231  CP0 (2/7)
249c4aa  CP0 (3/7)
6c458a1  CP0 (4/7)
24aafdc  CP0 (5/7)
3e68c1d  CP0 (6/7)
4a952f0  CP0 (7/7)
de214fc  CP1
63045c6  CP2
017e7ae  CP4   <- creato PRIMA di CP3
e8c5d3b  CP3
a10c73a  Report Phase 0
561552d  M0 apertura
afd1bae  M0 rettifiche
git diff --shortstat 4771a9c..a10c73a  -> 36 files changed, 7290 insertions(+), 1 deletion(-)
git diff --name-only --diff-filter=A 4771a9c..a10c73a | count -> 34 file nuovi
```

| Grandezza | Valore corretto | Note |
|---|---|---|
| Commit lavoro pre-piano (CP0–CP4 + report) | **12** | 7 CP0 + 1 CP1 + 1 CP2 + 1 CP3 + 1 CP4 + 1 report |
| Il "5" del report | 1 commit per checkpoint CP0–CP4 | Conta i checkpoint, non i commit; esclude il report |
| Il "6" della sezione M0 | 5 checkpoint + report | Conta un solo commit per CP0 |
| Commit governance M0 | 3 | `561552d`, `afd1bae`, commit di chiusura |
| File toccati pre-piano | **36** (34 aggiunti, 2 modificati) | Range `4771a9c..a10c73a` |
| Righe inserite pre-piano | **7290** | Il valore "~6200" era una stima e va considerato superato |
| Ordine cronologico | CP4 (`017e7ae`) precede CP3 (`e8c5d3b`) | Il codice CP4 dipende da `telemetry_quality.h`, aggiunto nel commit CP3: al commit `017e7ae` isolato il target CP4 non è compilabile (DEDOTTO, non verificato) |

### Come è stato verificato

| Verifica | Metodo | Esito | Classificazione |
|---|---|---|---|
| Storia Git coerente, CP4 in HEAD | `git log`, `git merge-base --is-ancestor 017e7ae HEAD` (sezione precedente) | exit 0 | VERIFICATO |
| Commit rettifiche | `git commit` → `afd1bae`, 5 file | exit 0 | VERIFICATO |
| Rimozione "pass rate 100%" | ricerca regex `pass rate` in `docs/` | Rimasta solo la citazione storica in `avanzamentoPiano.md` | VERIFICATO (ricerca) |
| Rimozione ✅/❌ da CP4 doc | ricerca regex `✅\|❌` in `phase0-host-cp4-implementation.md` | 0 occorrenze dopo la correzione | VERIFICATO (ricerca) |
| Build/test C++ | — | Non eseguiti (vietati/impossibili prima di M3) | NON TESTATO |

### Verifiche di uscita M0 (piano §3)
- [x] `avanzamentoPiano.md` creato con sezione M0 (`561552d`, chiuso con `cc09922`)
- [x] Output Git allegato e coerente, CP4 incluso in HEAD
- [x] Nessuna affermazione "verificato" senza evidenza nei documenti CP0–CP4 e nel report (`afd1bae`, `cc09922`)

### Punti aperti (non bloccanti per M0)
- `docs/afore-af5k-sl-protocol.md` e `profiles/afore/af5k_sl.toml` usano "VERIFIED" per indicare una conferma da fonte documentale, non empirica. La riclassificazione con gli stati `VERIFIED_EMPIRICALLY` / `PROVISIONAL` è prevista in **M6**.
- Accesso Linux per M3 ancora assente (SSH exit 255).
- Dati storici HA per M5 non ancora disponibili.

### Requisiti coperti
Nessuno: M0 è solo governance.

---

## M1 – Matrice di tracciabilità completa

**Data:** 2026-11-10
**Stato:** RAGGIUNTA
**Commit:** `docs(m1): add complete Smart EMS requirements traceability` (l'hash non può essere incluso nel commit stesso; è verificabile con `git log -1`)
**Deliverable:** `docs/REQUISITI_TRACCIABILITA.md`

### Obiettivo
Censire tutti i requisiti funzionali del piano V4, collegarli solo a codice effettivamente ispezionato (non per inferenza dal nome file), esplicitare gap, dipendenze, milestone e test pianificati, e mappare ogni funzione CP4 ai requisiti parzialmente coperti.

### Come è stato sviluppato
- Trascritti **59 requisiti** dalla sezione 2 del piano nei 10 gruppi richiesti: R-TGT (7), R-NRG (10), R-PV (6), R-FOG (5), R-LOAD (4), R-SEA (5), R-CAL (3), R-CMD (7), R-DIA (6), R-SAF (6).
- Per ogni requisito compilate le 13 colonne concettuali richieste: ID, Full requirement, Source, Existing file/function, Status, Gap, Target module, Planned unit test, Planned integration test, Dependency, Milestone, Notes (rappresentate verticalmente per leggibilità Markdown).
- Stati ammessi limitati a `ABSENT`, `PARTIAL`, `IMPLEMENTED_NOT_TESTED`, `VERIFIED`; nessun requisito marcato VERIFIED prima di M3.
- Ispezionato il controller CP4 (`src/ems/shadow_ems_controller.{h,cpp}`, `shadow_ems_types.{h,cpp}`), la qualità CP3 e il decoder telemetrico; mappate tutte le funzioni CP4 in una tabella inversa funzione→requisiti.
- Documentati esplicitamente i gap noti: target SOC/time 15:30, buffer 20 min, recovery 60 min, feasibility, Solcast, correzione FV reale, nebbia, carichi weekend, stagionalità, calibrazione BMS, rampe A→W, no-discharge, reason codes, degraded without HA.
- Gap summary organizzato nelle sei sezioni richieste: Implemented but not tested, Partially implemented, Entirely absent, Conflicting behavior, No longer needed, Blocked by missing data.

### Come è stato verificato

| Verifica | Evidenza | Esito | Classificazione |
|---|---|---|---|
| Completezza IDs | ricerca `^### R-[A-Z]+-[0-9]+` nel deliverable | **59** intestazioni univoche | VERIFICATO (controllo testuale) |
| Gruppi | intestazioni sezioni 1–10 | tutti i 10 gruppi presenti | VERIFICATO (ispezione) |
| Campi obbligatori | ogni scheda contiene 12 campi oltre all'intestazione ID | presenti | VERIFICATO (ispezione) |
| Gap summary | sezioni 11.1–11.6 | tutte presenti | VERIFICATO (ispezione) |
| Stato VERIFIED | ricerca campo Status=VERIFIED | **0** requisiti; coerente con vincolo M3+ | VERIFICATO (ricerca) |
| Esecuzione test/build | nessuna | non pertinente per deliverable documentale; nessuna build o test eseguiti | NON TESTATO |

### Evidenze principali
- CP4 è classificato come **preliminary self-consumption controller**, non come Smart Energy Manager target-aware.
- `checkTelemetryQuality()`, `computeRecommendation()`, `applySelfConsumptionStrategy()`, `applyPowerSmoothing()`, `updateDivergence()`, `transitionToFailSafe()` e helper sono mappati a requisiti specifici; nessuna copertura è dedotta dal solo nome del file.
- Conflitto documentato: `Quality::Degraded` è actionable nel codice CP4, mentre la documentazione storica sosteneva `Degraded → SafeIdle`.
- Conflitto documentato: CP4 può raccomandare scarica (`DischargeBattery`), mentre R-CMD-06 richiede default charge-only con `allow_discharge=false`.
- Conflitto documentato: rampa CP4 500 W/s e isteresi 30 W non corrispondono rispettivamente alle rampe 5/8 A (256/410 W) e all'isteresi 100 W richieste.

### Scostamenti
Nessuno rispetto all'obiettivo M1. La matrice usa schede verticali anziché un'unica tabella orizzontale per evitare colonne illeggibili, conservando integralmente tutti i campi richiesti.

### Punti aperti
- I requisiti classificati ABSENT/PARTIAL verranno implementati a partire da M4, dopo hardening M2 e build/test M3.
- Telemetria reale e segni potenza restano bloccati fino a M8; storico carichi fino a M5/M7; input Solcast/HA fino a M10.
- La classificazione `No longer needed` non elimina requisiti: indica solo che alcuni write-path non servono in shadow; la proprietà richiesta resta tracciata.

### Verifiche di uscita M1 (piano §3)
- [x] Tutti i requisiti hanno ID e modulo target
- [x] Ogni gap noto ha test pianificato
- [x] Ogni funzione CP4 è mappata ai requisiti che implementa
- [x] Nessun requisito è `VERIFIED` senza test passato

### Requisiti coperti
Tutti i 59 requisiti del piano sono **tracciati**, non implementati da M1. M1 è documentale e non modifica lo stato tecnico dei requisiti.

---

## Rettifica finale M1 – classificazione segni e fonti

**Data:** 2026-11-10  
**Stato:** DOCUMENTAZIONE CORRETTA, M1 PRONTA PER APPROVAZIONE  

### Obiettivo
Chiudere direttamente il debito documentale residuo senza un ulteriore ciclo dell’agente.

### Modifiche effettuate
- `R-NRG-10`: `IMPLEMENTED_NOT_TESTED`, `Compatibility = ALIGNED`, rischio `CRITICAL_UNVERIFIED_SIGN_CONVENTION`.
- `CONF-06` rimosso e sostituito da `ASSUMPTION-01`.
- M8 limitata alla verifica read-only del segno telemetrico `0x07D8`.
- Verifica del segno dei comandi External EMS `0x0A09–0x0A0A` rinviata esplicitamente al post-G4.
- Fonti normalizzate con `Source type` e `Source reference`.

### Classificazione evidenze
- **VERIFICATO:** modifiche documentali applicate al file corretto; nessun accesso hardware.
- **DEDOTTO:** nessuno.
- **NON TESTATO:** codice C++, build, seriale, inverter.
- **CONFLITTO:** nessun conflitto documentale residuo noto per i punti corretti.

### Vincoli mantenuti
Nessuna modifica al codice, nessuna compilazione, nessuna apertura seriale, nessuna build di interrogazione e nessuna comunicazione con l’inverter.
