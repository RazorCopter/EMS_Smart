# Piano di sviluppo – Afore Autonomous EMS V4

**Proprietario del progetto:** Gianvito Bleve
**Repository:** `afore-autonomous-ems` (fork di Heliograph), branch `phase0-host-shadow`
**Impianto:** Afore AF5K-SL monofase 5 kW, batteria master/slave 10,24 kWh, COM1 RS485
**Stato inverter durante tutto il piano:** `SelfUse`, nessuna automazione che scriva registri
**Documento di avanzamento obbligatorio:** `docs/avanzamentoPiano.md`

---

## 0. Regole di governo del piano (vincolanti per l'agente)

1. **Il piano si esegue per milestone, in ordine.** Non si avvia una milestone se la precedente non ha superato tutte le verifiche.
2. **A ogni milestone raggiunta l'agente aggiorna `docs/avanzamentoPiano.md`** aggiungendo una sezione (mai riscrivendo le precedenti) con: obiettivo, come è stato sviluppato, come è stato verificato, evidenze, scostamenti, punti aperti, commit.
3. **Distinzione obbligatoria delle evidenze** in ogni sezione: `VERIFICATO` (eseguito con output allegato), `DEDOTTO` (analisi statica o ragionamento), `NON TESTATO`, `CONFLITTO`. È vietato scrivere "completato" o "pass 100%" per codice non compilato o test non eseguiti.
4. **Gate formali.** I gate G1, G2, G3, G4 richiedono un'approvazione scritta di Gianvito. Al raggiungimento di un gate l'agente **si ferma**, aggiorna `avanzamentoPiano.md` con lo stato `IN ATTESA DI APPROVAZIONE` e non esegue alcuna attività successiva.
5. **Build di interrogazione bloccata.** Fino all'approvazione del gate G1 è vietato: aprire la porta seriale reale, eseguire binari collegati all'inverter, creare o installare la build di interrogazione, installare servizi systemd. È consentito soltanto lavoro su codice, test con fake transport, replay di dati e documentazione.
6. **Read-only assoluto in tutta la Phase 0.** Ammessi soltanto FC03 e FC04 completi e con CRC valido. Mai scritture su `0x09C4`, `0x0A08`, `0x0A09–0x0A0A`, `0x0BC1`, `0x0BC2` o qualsiasi altro registro. Nessuna scansione di slave ID/baud/indirizzi.
7. **Nessuna credenziale nel repository** (`.env`, token MQTT, password Wi-Fi esclusi).
8. **Ogni milestone termina con un commit** dedicato il cui hash viene riportato in `avanzamentoPiano.md`.

---

## 1. Obiettivo complessivo

Realizzare un EMS autonomo, prima simulato su Raspberry Pi e poi portato sulla Waveshare ESP32-S3-Relay-1CH-U, che:

- legge direttamente dall'Afore la telemetria via Modbus RTU in sola lettura;
- implementa **la logica Smart Energy Manager già definita in Home Assistant (blueprint V3.3)**, corretta e tradotta da "limite di corrente" a "setpoint di potenza" compatibile con External EMS;
- funziona anche senza Home Assistant, usando HA solo come fonte di informazioni smart (Solcast, nebbia) e di override;
- opera in **shadow mode**: calcola cosa farebbe, non lo applica;
- produce dati per confrontare per almeno 7 giorni le proprie decisioni con il comportamento reale dell'EMS interno Afore;
- solo in una fase successiva e separata (fuori da questo piano) potrà controllare l'inverter tramite External EMS.

---

## 2. Requisiti funzionali Smart Energy Manager (da Home Assistant)

Questi requisiti derivano dalla blueprint `Solarman Smart Energy Manager V3.2/V3.3` e dalle decisioni prese durante il collaudo. Il codice Heliograph non li contiene: vanno implementati nel modulo EMS.

### 2.1 Obiettivo di carica

| ID | Requisito | Valore di riferimento |
|---|---|---|
| R-TGT-01 | SOC target configurabile | default 100% |
| R-TGT-02 | Orario target configurabile | default 15:30 |
| R-TGT-03 | Anticipo operativo sul target | 20 min |
| R-TGT-04 | Finestra di recupero finale (priorità massima al target) | 60 min |
| R-TGT-05 | Usare la potenza minima compatibile con il raggiungimento del target | – |
| R-TGT-06 | Se il SOC scende sotto target prima dell'orario (es. lavastoviglie dopo il 100%), riprendere la ricarica | comportamento osservato 8/11 e accettato |
| R-TGT-07 | Dichiarare esplicitamente target non raggiungibile quando energia/limiti non bastano | stato `NOT_FEASIBLE` |

### 2.2 Modello energetico

| ID | Requisito | Valore |
|---|---|---|
| R-NRG-01 | Capacità batteria | 10,24 kWh |
| R-NRG-02 | Tensione nominale (per conversioni A↔W) | 51,2 V |
| R-NRG-03 | Efficienza di carica | 0,90 |
| R-NRG-04 | Margine di sicurezza sul fabbisogno | 1,2 |
| R-NRG-05 | Deficit = (SOC target − SOC) × capacità | kWh |
| R-NRG-06 | Potenza media richiesta = deficit / ore effettive / efficienza | W |
| R-NRG-07 | Proiezione SOC al target dal ritmo reale di carica | % |
| R-NRG-08 | Feedback sul ritmo reale: ritardo se potenza reale < richiesta × 0,85 | tolleranza 0,85 |
| R-NRG-09 | Stato critico se scostamento proiettato ≥ 8 punti SOC o richiesta ≥ 90% del massimo | – |
| R-NRG-10 | Potenza batteria Afore: negativa in carica, positiva in scarica | da confermare empiricamente |

### 2.3 Previsione fotovoltaica (Solcast via Home Assistant)

| ID | Requisito | Valore |
|---|---|---|
| R-PV-01 | Usare `detailedForecast` a slot di 30 min, stima selezionabile | `pv_estimate` (alternative 10/90) |
| R-PV-02 | Età massima previsione valida | 14 h |
| R-PV-03 | Surplus utile = previsione corretta − carico domestico, solo fino all'orario target | kWh |
| R-PV-04 | Correzione con FV reale (rapporto reale/previsto) sulle prossime ore | orizzonte 2 h, minimo 0,5, massimo 1 |
| R-PV-05 | Senza Solcast valido: modalità conservativa (priorità target, nessuna ottimizzazione aggressiva) | – |
| R-PV-06 | Il controller deve funzionare anche senza Solcast usando solo dati locali | degrado dichiarato |

### 2.4 Nebbia dinamica

| ID | Requisito | Valore |
|---|---|---|
| R-FOG-01 | Nebbia se (T − Td) ≤ 1,5 °C e radiazione < 100 W/m² | – |
| R-FOG-02 | Uscita nebbia se (T − Td) ≥ 2,5 °C, radiazione ≥ 180 W/m², FV/Solcast ≥ 0,75 | isteresi |
| R-FOG-03 | Preferire il sensore stabilizzato `binary_sensor.sem_nebbia_stabile` | fallback sul calcolo istantaneo |
| R-FOG-04 | Con nebbia: fattore FV sulle prossime ore | 0,3 |
| R-FOG-05 | Nessun orario rigido di dissoluzione nebbia | solo diagnostica |

### 2.5 Carichi domestici

| ID | Requisito | Valore |
|---|---|---|
| R-LOAD-01 | Carico feriale stimato | 0,6 kW |
| R-LOAD-02 | Carico weekend stimato | 0,9 kW |
| R-LOAD-03 | Giorni weekend configurabili | sab, dom |
| R-LOAD-04 | Quando validato, sostituire la stima con il carico misurato dal bilancio | fase successiva |

### 2.6 Strategia stagionale (equivalente ChgFst)

| ID | Requisito | Valore |
|---|---|---|
| R-SEA-01 | Valutazione giornaliera della priorità di carica | 08:30 |
| R-SEA-02 | Stagione invernale configurabile | 25/10 – 28/02 |
| R-SEA-03 | Attivare priorità carica se surplus oggi < deficit al 100% × 1,1 **e** domani non favorevole | margine 1,1 |
| R-SEA-04 | Domani favorevole se surplus domani ≥ 7 kWh e SOC ≥ SOC minimo per domani | SOC min 30% |
| R-SEA-05 | Nella V4 shadow la decisione "ChgFst" è registrata come stato strategico, non come cambio modalità inverter | – |

### 2.7 Calibrazione BMS

| ID | Requisito | Valore |
|---|---|---|
| R-CAL-01 | Considerare calibrata la batteria con SOC pieno stabile | 100% per 5 min |
| R-CAL-02 | Dopo calibrazione, ritorno alla strategia ordinaria (equivalente ritorno SelfUse) | – |
| R-CAL-03 | Il profilo longevità (es. SOC max 95%) è opzionale e non deve impedire la calibrazione al 100% | – |

### 2.8 Stabilità e comandi (preparazione External EMS)

| ID | Requisito | Valore |
|---|---|---|
| R-CMD-01 | Setpoint espresso in W (carica negativa secondo convenzione Afore, da confermare) | – |
| R-CMD-02 | Rampa massima ordinaria in salita/discesa (traduzione di 5 A / 8 A per ciclo) | ≈ 256 W / 410 W per ciclo |
| R-CMD-03 | Isteresi minima per variazione setpoint (traduzione di 2 A) | ≈ 100 W |
| R-CMD-04 | In stato critico nessuna rampa lenta | – |
| R-CMD-05 | Limiti min/max carica configurabili (equivalente 5–50 A) | ≈ 256–2560 W, da validare col BMS |
| R-CMD-06 | Nella Phase 0 nessuna scarica comandata nel setpoint primario | `allow_discharge = false` |
| R-CMD-07 | Heartbeat futuro External EMS 30–60 s, timeout Afore 2 min → 0 W | solo progettazione |
| R-CMD-08 | Ritorno controllato a SelfUse in caso di perdita supervisione | solo progettazione |

### 2.9 Notifiche e diagnostica

| ID | Requisito |
|---|---|
| R-DIA-01 | Riepilogo serale (20:13): modalità, SOC, previsione domani, stato nebbia |
| R-DIA-02 | Evento all'attivazione della priorità di carica |
| R-DIA-03 | Evento a calibrazione completata |
| R-DIA-04 | Evento previsione Solcast non valida |
| R-DIA-05 | Ogni decisione con `decision`, `reason`, `confidence`, input usati e loro qualità |

### 2.10 Qualità dati e sicurezza

| ID | Requisito |
|---|---|
| R-SAF-01 | Un dato mancante non è mai zero (`unknown ≠ 0`) |
| R-SAF-02 | Stati qualità: VALID, STALE, INVALID, OUT_OF_RANGE, COMM_ERROR, UNAVAILABLE, UNVERIFIED |
| R-SAF-03 | SOC/potenza batteria/FV non validi → decisione `DATA_INVALID`, setpoint nullo |
| R-SAF-04 | Override da HA non possono superare limiti di sicurezza |
| R-SAF-05 | Gerarchia: sicurezza > validità dati > limiti BMS/inverter > anti-import > override > target > ottimizzazione |
| R-SAF-06 | Il funzionamento senza HA/MQTT deve continuare con dati locali |

---

## 3. Milestone

Ogni milestone indica obiettivo, attività, deliverable e **verifiche di uscita**. L'agente riporta tutto in `avanzamentoPiano.md`.

### M0 – Riallineamento e governance

**Obiettivo:** fotografare lo stato reale del repository e istituire la documentazione di avanzamento.

**Attività**
- Creare `docs/avanzamentoPiano.md` dal template fornito.
- Verificare la storia Git (`git log --oneline --decorate -15`, `git show --stat` dei commit CP0–CP4, `git merge-base --is-ancestor 017e7ae HEAD`).
- Riclassificare lo stato reale: CP0 completato; CP1–CP4 implementati ma non compilati né eseguiti.
- Correggere nei documenti le affermazioni non dimostrate ("compiles cleanly", "pass rate 100%", "0x0BC1 scrive sempre EEPROM").

**Verifiche di uscita**
- [ ] `avanzamentoPiano.md` creato con sezione M0.
- [ ] Output Git allegato e coerente (CP4 incluso in HEAD).
- [ ] Nessuna affermazione "verificato" senza evidenza.

### M1 – Matrice di tracciabilità dei requisiti

**Obiettivo:** collegare ogni requisito della sezione 2 al codice esistente o da sviluppare.

**Attività**
- Creare `docs/REQUISITI_TRACCIABILITA.md` con colonne: ID requisito, descrizione, fonte (blueprint V3.3 / decisione progetto / Afore), modulo previsto, stato (`ASSENTE`, `PARZIALE`, `IMPLEMENTATO`, `VERIFICATO`), test che lo copre.
- Confrontare l'attuale `shadow_ems_controller` con i requisiti: identificare i gap (target 15:30, Solcast, nebbia, carichi, stagionalità, calibrazione, scarica da escludere).

**Verifiche di uscita**
- [ ] Tutti gli ID della sezione 2 presenti nella matrice.
- [ ] Ogni requisito ha un modulo di destinazione e un test previsto.
- [ ] Elenco gap del CP4 attuale documentato.

### M2 – Hardening della barriera read-only

**Obiettivo:** dimostrare con test che nessun frame diverso da FC03/FC04 valido può raggiungere la seriale.

**Attività**
- Correggere `ReadOnlyGuard`: niente passthrough di frame corti; blocco di frame malformati, CRC errato, FC01/02/05/06/0F/10/16/17 e sconosciuti.
- Implementare un fake lower transport che conta i byte ricevuti.
- Introdurre il flag di compilazione `AFORE_SHADOW_BUILD` che esclude dispatcher, command queue, relè, DRM, ingress REST/MQTT di comando, OTA, writeSingleRegister.
- Evento `SECURITY_BLOCK` senza terminare il processo.

**Verifiche di uscita**
- [ ] Test per ogni caso con conteggio byte: consentiti = lunghezza esatta; bloccati = 0 byte e nessuna chiamata a `::write()`.
- [ ] Ispezione simboli del binario shadow (`nm`/`objdump`): nessun simbolo di dispatcher, relè o write client.
- [ ] Output di test allegato in `avanzamentoPiano.md` (eseguito in M3).

### M3 – Build host e test (senza seriale)

**Obiettivo:** dimostrare che il codice compila ed è testato su Linux.

**Attività**
- Ottenere accesso al Raspberry tramite chiave SSH dedicata e revocabile, oppure esecuzione interattiva da parte di Gianvito.
- Audit ambiente (OS, architettura, g++, cmake, python, gruppi utente, `/dev/serial/by-id`) senza modificare il sistema.
- Compilare: `cmake -S . -B build-host -DAFORE_SHADOW_BUILD=ON -DCMAKE_BUILD_TYPE=RelWithDebInfo`, `cmake --build build-host -j2`, `ctest --test-dir build-host --output-on-failure`.
- Flag: `-std=c++17 -Wall -Wextra -Wpedantic -Werror`.

**Vincolo:** in M3 il binario non viene collegato alla porta seriale.

**Verifiche di uscita**
- [ ] Build senza warning.
- [ ] Tutti i test eseguiti; numero test passati/falliti riportato con log.
- [ ] Eventuali pacchetti mancanti segnalati, non installati senza autorizzazione.

### M4 – Core EMS target-aware (logica pura)

**Obiettivo:** implementare la logica della sezione 2 come moduli C++ portabili e testati, indipendenti dall'hardware.

**Moduli**
- `energy_model` (R-NRG)
- `target_planner` (R-TGT)
- `pv_forecast` con correzione FV reale e scadenza (R-PV)
- `fog_detector` con isteresi e sensore stabilizzato (R-FOG)
- `load_profile` feriale/weekend (R-LOAD)
- `seasonal_strategy` equivalente ChgFst (R-SEA)
- `calibration_tracker` (R-CAL)
- `setpoint_shaper` rampe, isteresi, criticità (R-CMD)
- `safety_supervisor` (R-SAF)
- `decision_log` con reason code (R-DIA-05)

**Scenari di test obbligatori**
1. Giornata soleggiata, SOC basso.
2. Giornata variabile.
3. Nebbia mattutina che si dissolve.
4. Nebbia persistente, target non raggiungibile.
5. SOC 100% alle 13:00, poi lavastoviglie: ripresa ricarica prima delle 15:30.
6. SOC 19% con FV 2,7 kWh previsti (caso reale 9/11): `NOT_FEASIBLE` dichiarato.
7. Solcast scaduto o assente.
8. Domani favorevole vs sfavorevole alla valutazione delle 08:30.
9. Weekend con carico 0,9 kW.
10. Dati SOC/FV/batteria non validi.
11. Override HA fuori limiti.
12. Calibrazione 100% per 5 minuti.

**Verifiche di uscita**
- [ ] Ogni requisito R-* collegato ad almeno un test passato.
- [ ] Nessuna scarica nel setpoint primario.
- [ ] Matrice di tracciabilità aggiornata a `VERIFICATO` per i requisiti coperti.

### M5 – Replay su dati storici Home Assistant

**Obiettivo:** validare la logica su giornate reali prima di toccare il bus.

**Attività**
- Esportare da Home Assistant lo storico (SOC, potenza batteria, FV, rete, Solcast, nebbia, corrente, modalità EMS) dei giorni disponibili, inclusi 29/10–9/11.
- Tool di replay che esegue il core EMS sullo storico, senza accesso seriale.
- Confronto con: comportamento V3.3 registrato e comportamento SelfUse nativo.

**Verifiche di uscita**
- [ ] Report per giornata: ora del 100% reale vs proiettata, SOC alle 15:30, decisioni shadow, reason, eventuale `NOT_FEASIBLE`.
- [ ] Spiegazione degli scostamenti più rilevanti.
- [ ] Nessuna oscillazione anomala del setpoint (numero variazioni/ora riportato).

### M6 – Validazione documentale della mappa registri

**Obiettivo:** preparare la mappa da usare sul bus, senza ancora interrogarlo.

**Attività**
- `docs/AFORE_REGISTER_PROVENANCE.md`: indirizzo, FC, tipo, scala, significato, fonte (Excel Afore v3.4, En_240312, YAML Solarman), stato.
- Stati: `VERIFIED_EMPIRICALLY` (solo 0x07D2), `PROVISIONAL`, `DOCUMENTED_CONFLICTED` (0x09C4), `DOCUMENTED_CONTROL_EXCLUDED` (0x0A08–0x0A0A), `CONFIGURATION_EXCLUDED` (0x0BC1).
- Lista sonde iniziali: singole letture `0x07D2`, `0x07D8`, `0x022A`, `0x0212`.
- Recuperare o ricreare gli oracoli Python Solarman.

**Verifiche di uscita**
- [ ] Nessun blocco di lettura attraversa registri non verificati.
- [ ] Polling previsto conservativo: SOC 15 s, potenze 10 s, retry 0, una richiesta alla volta, ≥200 ms tra frame, arresto dopo 3 errori.

---

### 🛑 GATE G1 – Check formale prima della build di interrogazione

**L'agente si ferma.** In `avanzamentoPiano.md` riporta lo stato `G1 – IN ATTESA DI APPROVAZIONE` con:
- riepilogo M0–M6;
- esiti di build, test, guard, replay;
- elenco rischi residui;
- piano preciso della build di interrogazione (comandi, registri, frequenze, criteri di stop).

**Solo dopo approvazione scritta di Gianvito**, ed eventuali correzioni richieste, si crea la build di interrogazione.

---

### M7 – Build di interrogazione one-shot

**Obiettivo:** prima comunicazione reale del codice C++ con l'inverter.

**Attività**
- Identificare il device stabile `/dev/serial/by-id/...`.
- Verificare che nessun altro processo usi la porta e chiarire la coesistenza con il dongle Solarman.
- Una lettura SOC con oracolo Python, chiusura porta; una lettura SOC con C++, chiusura porta.
- Confronto: frame richiesta/risposta, CRC, raw, valore, latenza.
- Poi le altre tre sonde singole, una per volta.

**Verifiche di uscita**
- [ ] SOC identico tra Python e C++ (letture ravvicinate).
- [ ] Contatore `SECURITY_BLOCK` = 0.
- [ ] Nessun errore inverter/BMS, nessun degrado sul monitoraggio Solarman.

### M8 – Convenzioni dei segni e bilancio energetico

**Obiettivo:** confermare segni e coerenza del bilancio.

**Attività**
- Osservare un evento di import certo e uno di export certo; verificare segno batteria in carica e scarica.
- Calcolare errore di chiusura del bilancio.

**Verifiche di uscita**
- [ ] Segno rete e batteria documentati con evidenza.
- [ ] Errore di bilancio entro soglia configurata (warning 150 W, invalid 500 W).

### 🛑 GATE G2 – Autorizzazione al polling continuo

Approvazione di Gianvito su esiti M7–M8 prima di qualsiasi polling ripetuto.

### M9 – Polling conservativo di durata (24–72 h)

**Verifiche di uscita**
- [ ] Letture valide ≥ 99,5% su 24 h.
- [ ] Latenza media/P95/max, timeout, CRC error riportati.
- [ ] Nessun impatto su Solarman, inverter, BMS.

### M10 – Integrazione Home Assistant

**Obiettivo:** pubblicare telemetria e decisioni shadow e ricevere gli input smart da HA.

**Attività**
- MQTT Discovery prefisso `afore_v4`, entità con qualità ed età dato.
- Ingresso da HA: Solcast oggi/domani, ultimo polling Solcast, nebbia stabilizzata, temperatura, punto di rugiada, radiazione, override (target SOC, orario, strategia).
- Comportamento senza HA/MQTT verificato.

**Verifiche di uscita**
- [ ] Entità visibili e coerenti con i sensori Solarman.
- [ ] Perdita MQTT: polling e calcolo proseguono, decisione degradata dichiarata.
- [ ] Override fuori limite rifiutati.

### M11 – Shadow 7 giorni e confronto con EMS nativo Afore

**KPI giornalieri**
- ora del 100% e SOC alle 15:30 (reale);
- SOC proiettato vs osservato;
- energia caricata/scaricata, import durante la carica;
- variazioni del setpoint shadow per ora;
- casi in cui lo shadow avrebbe anticipato o ridotto la carica;
- casi `NOT_FEASIBLE`, `DATA_INVALID`, blocchi safety;
- % telemetria valida.

**Verifiche di uscita**
- [ ] 7 giorni consecutivi con dataset completo, inclusa almeno una giornata di nebbia o bassa produzione.
- [ ] Report comparativo con distinzione osservato / interpretazione / ipotesi.

### 🛑 GATE G3 – Porting su ESP32

### M12 – Porting Waveshare ESP32-S3 in shadow

**Verifiche di uscita**
- [ ] Stesso core EMS, stessi test passati sul target.
- [ ] Replay dello stesso dataset: decisioni identiche tra Raspberry ed ESP32.
- [ ] Watchdog, riconnessione Wi-Fi/MQTT, nessuna scrittura persistente frequente su Flash ESP32.

### 🛑 GATE G4 – Controllo attivo External EMS

Fuori dallo scopo di questo piano. Richiede build separata `SEM-V4-CONTROLLER`, prova presidiata, ritorno a SelfUse verificato e nuovo piano dedicato.

---

## 4. Definizione di "milestone raggiunta"

Una milestone è raggiunta solo se:
1. tutte le verifiche di uscita sono spuntate con evidenza;
2. il commit è stato creato;
3. la sezione in `avanzamentoPiano.md` è stata aggiunta;
4. non esistono test di sicurezza falliti.

## 5. Condizioni di arresto immediato

- Qualunque frame diverso da FC03/FC04 tentato verso la seriale.
- Errori o allarmi su inverter, BMS o datalogger Solarman.
- Parametri seriali non confermati.
- Richiesta di installare pacchetti o modificare il sistema non autorizzata.
- Raggiungimento di un gate.
