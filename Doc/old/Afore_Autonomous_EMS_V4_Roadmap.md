# Afore Autonomous EMS V4

## Documento di architettura, scouting, shadow mode e industrializzazione

**Impianto di riferimento**

- Inverter: Afore AF5K-SL, ibrido monofase da 5 kW
- Batteria: sistema master/slave, capacità complessiva configurata 10,24 kWh
- Monitoraggio attuale: Home Assistant con integrazione Solarman
- Controller ordinato: Waveshare ESP32-S3-Relay-1CH-U con RS485 isolata
- Modalità temporanea inverter: `SelfUse`
- Automazione Home Assistant `Solarman Smart Energy Manager V3.3`: disabilitata
- Obiettivo storico: raggiungere il SOC target entro le 15:30, privilegiando il fotovoltaico

> Stato del documento: progetto in fase di scouting. Nessuna funzione di controllo deve scrivere registri dell'inverter finché Phase 0, acquisizione, shadow mode e validazione non sono completate.

---

## 1. Obiettivo

Realizzare un EMS autonomo, robusto e pilotabile dall'esterno che:

1. legga direttamente dall'Afore i dati energetici in tempo reale;
2. continui a funzionare senza Home Assistant;
3. pubblichi telemetria e diagnostica verso Home Assistant;
4. calcoli in autonomia la strategia energetica;
5. accetti da Home Assistant solo obiettivi e override controllati;
6. applichi sempre i vincoli di sicurezza locali prima degli override;
7. operi inizialmente in **shadow mode**, senza modificare l'inverter;
8. utilizzi in futuro la modalità ufficiale **External EMS**, evitando scritture dinamiche sul parametro permanente della corrente massima.

---

## 2. Principio architetturale

```text
                    HOME ASSISTANT
             supervisione e funzioni smart
           Solcast, nebbia, dashboard, override
                         |
                    MQTT / API
                         |
                         v
              +------------------------+
              | AFORE AUTONOMOUS EMS   |
              |                        |
              | Telemetry Engine       |
              | Energy Balance         |
              | SOC Target Planner     |
              | Safety Supervisor      |
              | Shadow Controller      |
              | Modbus RTU Master      |
              +-----------+------------+
                          |
                     RS485 / Modbus
                          |
                          v
                   AFORE AF5K-SL
                          |
                    BMS master/slave
```

Home Assistant migliora la strategia, ma non deve essere necessario per la continuità operativa. Il controller locale deve poter leggere le misure, calcolare il bilancio, mantenere il watchdog e applicare una politica conservativa anche se Wi-Fi, MQTT o Home Assistant non sono disponibili.

---

# PHASE 0 - Scouting con Raspberry Pi

## 3. Scopo della Phase 0

Prima di sviluppare il firmware ESP32, utilizzare un Raspberry Pi come banco di scouting per:

- verificare il collegamento fisico alla COM1;
- identificare con certezza pin A/B, baud rate, parità, stop bit e slave ID;
- verificare la stabilità delle letture Modbus;
- rilevare eventuali conflitti con il datalogger Solarman;
- confrontare i registri Afore con i sensori Home Assistant esistenti;
- acquisire traffico, latenze, timeout e codici di eccezione;
- definire i blocchi di lettura più efficienti;
- costruire il primo dataset per l'algoritmo shadow;
- evitare qualsiasi scrittura durante lo scouting.

## 3.1 Hardware minimo

- Raspberry Pi con alimentazione stabile;
- adattatore USB-RS485 **isolato galvanicamente**;
- collegamento A/B alla COM1 secondo documentazione Afore;
- GND solo se espressamente previsto dal pinout Afore e dall'interfaccia isolata;
- cavo twistato, corto e correttamente instradato;
- eventuale terminazione 120 ohm solo dopo verifica della topologia e delle terminazioni già presenti;
- accesso SSH al Raspberry Pi;
- acquisizione log su storage locale.

## 3.2 Regole di sicurezza Phase 0

1. Il software deve consentire esclusivamente Modbus Function Code di lettura.
2. Bloccare a livello applicativo FC `0x06`, `0x10` e qualsiasi write.
3. Non collegarsi al bus batteria COM2.
4. Non scollegare o modificare la comunicazione BMS master/slave.
5. Non collegare due master allo stesso bus senza aver chiarito la topologia.
6. Se COM1 è condivisa con il datalogger, interrompere il test al primo segnale di collisione o degrado.
7. Nessun comando External EMS in questa fase.
8. Nessuna modifica di `0x09C4`, `0x0A08`, `0x0A09-0x0A0A` o `0x0BC1`.

## 3.3 Piano di scouting

### Step 0A - Ispezione e cablaggio

- fotografare COM1 e relativi connettori;
- verificare continuità e polarità A/B senza alimentare circuiti aggiuntivi;
- annotare modello e revisione del datalogger Solarman;
- documentare topologia reale: inverter, dongle, Raspberry, terminazioni.

### Step 0B - Identificazione seriale

Testare solo le combinazioni indicate da Afore o già configurate nell'inverter:

- slave ID atteso: da leggere nella configurazione `ComParam`;
- baud rate: da leggere nella configurazione `ComBat`/`ComPar`;
- formato iniziale candidato: 8 data bit, parità e stop bit da confermare;
- timeout iniziale: 500-1000 ms;
- retry iniziali: 0 durante lo scouting, poi massimo 1.

Non eseguire scansioni aggressive di tutti gli slave ID o baud rate sul bus operativo.

### Step 0C - Letture controllate

Leggere inizialmente un registro/blocco alla volta, con intervallo conservativo di 5-10 secondi. Solo dopo stabilità documentata passare a blocchi contigui.

### Step 0D - Confronto Solarman

Per ogni misura confrontare:

- valore raw Modbus;
- regola signed/unsigned;
- fattore di scala;
- unità;
- valore pubblicato da Solarman;
- timestamp e differenza temporale;
- scostamento assoluto e percentuale.

### Step 0E - Test di durata

Eseguire una prova read-only di almeno 24 ore, preferibilmente 72 ore, registrando:

- richieste inviate;
- risposte valide;
- CRC error;
- timeout;
- eccezioni Modbus;
- latenza media, P95 e massima;
- gap nel dataset;
- stato del datalogger Solarman;
- eventuali errori inverter/BMS.

## 3.4 Criteri di uscita Phase 0

La Phase 0 è superata solo se:

- pinout e parametri seriali sono confermati;
- nessuna scrittura è stata eseguita;
- letture valide >= 99,5% per 24 ore;
- nessun degrado osservato su Solarman;
- nessun errore BMS o inverter correlato al test;
- scaling e segno dei registri principali sono verificati;
- il dataset presenta timestamp coerenti;
- è definita la strategia definitiva di accesso al bus per l'ESP32.

Se la porta non consente la coesistenza con Solarman, la decisione architetturale deve essere esplicita: rimozione del dongle, gateway unico oppure accesso alternativo ufficialmente supportato.

---

## 4. Registri iniziali di telemetria

I seguenti indirizzi derivano dalla definizione Solarman già in uso e devono essere verificati durante la Phase 0.

| Misura | Registro | Tipo/scaling attuale | Note |
|---|---:|---|---|
| Potenza FV totale | `0x022A` | U16, 1 W | Verificare saturazione e aggiornamento |
| Potenza FV1 | `0x022D` | U16, 1 W | Stringa 1 |
| Potenza FV2 | `0x0230` | U16, 1 W | Stringa 2 |
| SOC batteria | `0x07D2` | U16, 1% | Confronto con BMS |
| Tensione batteria | `0x07D4` | U16, 0,1 V | |
| Corrente batteria | `0x07D5` | signed, 0,01 A | Verificare codifica reale |
| Potenza batteria | `0x07D8` | signed, 1 W | Negativo osservato in carica |
| Stato batteria | `0x07D0` | enum | 4 charge, 5 discharge, 6 standby nella definizione attuale |
| Potenza rete | `0x0212` | signed, 1 W | Verificare convenzione import/export |
| Potenza uscita | `0x020B` | signed, 1 W | Verificare significato fisico |
| Stato/mode inverter | da verificare | enum | Evitare confusione tra running status ed EMS mode |

### Nota critica su `0x09C4`

Nella configurazione esistente `0x09C4` compare in contesti differenti. Nel protocollo External EMS è il registro `EMSMode`; non deve essere interpretato contemporaneamente come semplice running status senza aver verificato funzione, mappa e versione firmware.

---

## 5. External EMS ufficiale Afore

Il documento `En_240312.docx` descrive due opzioni: SunSpec e comandi Modbus diretti.

### Registri Modbus di controllo

| Funzione | Registro | Valore/struttura |
|---|---:|---|
| Modalità EMS | `0x09C4` | `5` = External EMS |
| Tipo controllo esterno | `0x0A08` | `0` charge/discharge power; `1` inverter power; `2` grid power in development |
| Setpoint potenza | `0x0A09-0x0A0A` | signed 32 bit, W |

Indicazioni Afore:

- configurazione modalità e tipo di controllo impostabili una volta;
- setpoint da ripetere a intervallo inferiore a 2 minuti;
- periodo consigliato 30-60 secondi;
- oltre 2 minuti senza comando l'inverter considera interrotta la comunicazione e usa potenza 0 W;
- il supporto Afore ha indicato External EMS come interfaccia adatta al controllo continuativo;
- per inverter ibrido monofase il documento indica COM1, pin 1/2 A/B;
- il setpoint negativo è stato confermato per la carica e quello positivo per la scarica, da ricontrollare nella prima prova attiva.

### Registri da non utilizzare dinamicamente

`0x0BC1` (`CurrMaxChg`, corrente massima di carica) è un parametro di configurazione. La V3.3 che lo modulava frequentemente è stata disabilitata. Fino all'attivazione controllata di External EMS non devono essere introdotte nuove automazioni che lo riscrivano periodicamente.

---

# PHASE 1 - Firmware ESP32 di acquisizione

## 6. Obiettivo

Portare sul Waveshare la telemetria validata con Raspberry Pi, senza funzioni di scrittura.

## 6.1 Moduli firmware

```text
src/
  app/
  config/
  modbus/
  telemetry/
  validation/
  mqtt/
  api/
  diagnostics/
  storage/
  watchdog/
  shadow/
  tests/
```

### Modbus Driver

- RTU master;
- code path read-only nella build shadow;
- polling scheduler non bloccante;
- CRC e timeout;
- limitazione retry;
- metriche per blocco di registri;
- qualità del dato per ogni misura.

### Telemetry Engine

- timestamp monotonic e wall-clock;
- raw value e valore ingegnerizzato;
- stato `valid`, `stale`, `invalid`, `unavailable`;
- data age;
- range plausibili;
- rilevamento salti e valori congelati.

### Connettività

- Wi-Fi con riconnessione e backoff;
- MQTT con Last Will and Testament;
- REST API locale per stato e configurazione;
- mDNS opzionale;
- nessuna dipendenza dal cloud.

## 6.2 Firmware separati

- `SEM-V4-SHADOW`: letture, calcoli, diagnostica; nessun codice di scrittura inverter.
- `SEM-V4-CONTROLLER`: futura build con External EMS, costruita solo dopo la validazione.

La separazione deve essere a compile time, non solo tramite `control_enabled: false`.

---

# PHASE 2 - Pubblicazione su Home Assistant

## 7. MQTT Discovery

Pubblicare almeno:

### Telemetria Afore

- `sensor.afore_v4_pv_power`
- `sensor.afore_v4_pv1_power`
- `sensor.afore_v4_pv2_power`
- `sensor.afore_v4_grid_power`
- `sensor.afore_v4_battery_power`
- `sensor.afore_v4_battery_soc`
- `sensor.afore_v4_battery_voltage`
- `sensor.afore_v4_battery_current`
- `sensor.afore_v4_battery_status`

### Diagnostica

- `binary_sensor.afore_v4_modbus_online`
- `sensor.afore_v4_modbus_latency_ms`
- `sensor.afore_v4_modbus_error_rate`
- `sensor.afore_v4_last_valid_read`
- `sensor.afore_v4_data_age_s`
- `sensor.afore_v4_firmware_version`
- `sensor.afore_v4_uptime`
- `sensor.afore_v4_safety_state`

### Shadow controller

- `sensor.sem_v4_shadow_requested_power`
- `sensor.sem_v4_shadow_allowed_power`
- `sensor.sem_v4_shadow_projected_soc`
- `sensor.sem_v4_shadow_energy_deficit`
- `sensor.sem_v4_shadow_target_feasibility`
- `sensor.sem_v4_shadow_decision`
- `sensor.sem_v4_shadow_reason`
- `sensor.sem_v4_shadow_confidence`

## 7.1 Recorder

Non registrare indiscriminatamente telemetria a 1 secondo nel database standard di Home Assistant.

- sensori energetici principali: pubblicazione 5 secondi, registrazione HA preferibilmente 10-30 secondi o su variazione significativa;
- SOC: 10-30 secondi;
- metriche diagnostiche: 30-60 secondi;
- eventi decisionali: ogni cambio di stato;
- log grezzi ad alta frequenza: file strutturato o database dedicato.

---

# PHASE 3 - EMS autonomo in Shadow Mode

## 8. Regola fondamentale

Il controller calcola il comando teorico ma non lo applica.

```text
misure -> stima stato -> vincoli -> decisione -> setpoint shadow -> log
                                                        |
                                                   NO ACTUATION
```

## 8.1 Input locali obbligatori

- PV totale e per stringa;
- potenza rete/CT;
- potenza batteria;
- SOC;
- tensione e corrente batteria;
- stato inverter;
- freschezza e qualità di ciascun dato;
- ora locale valida.

## 8.2 Input esterni opzionali

- previsione Solcast;
- stato nebbia;
- target SOC;
- target time;
- strategia `eco`, `balanced`, `target-first`;
- limiti di import/export;
- finestre tariffarie.

Gli input esterni non possono bypassare i limiti locali di sicurezza.

## 8.3 Gerarchia delle priorità

1. **Safety hard limits**: BMS, inverter, tensioni, SOC, dati validi.
2. **Fail-safe e qualità dati**: dati stantii o incoerenti impediscono il controllo.
3. **Anti-import / anti-export** secondo configurazione.
4. **Override autorizzati** da Home Assistant.
5. **SOC target planner**.
6. **Ottimizzazione locale del surplus**.
7. **Previsioni e preferenze**.

## 8.4 Output shadow da registrare

Per ogni decisione:

```json
{
  "mode": "SHADOW",
  "timestamp": "ISO-8601",
  "measurements": {
    "pv_w": 0,
    "grid_w": 0,
    "battery_w": 0,
    "soc_pct": 0,
    "data_age_s": 0
  },
  "planner": {
    "target_soc_pct": 100,
    "target_time": "15:30",
    "energy_deficit_kwh": 0,
    "required_average_w": 0,
    "projected_soc_pct": 0,
    "feasibility": "FEASIBLE"
  },
  "controller": {
    "requested_w": 0,
    "allowed_w": 0,
    "decision": "HOLD",
    "reason": "PV_SURPLUS_AVAILABLE"
  },
  "safety": {
    "state": "OK",
    "write_enabled": false
  }
}
```

---

# PHASE 4 - Raccolta dati e confronto per almeno 7 giorni

## 9. Baseline Afore

Durante lo shadow mode l'inverter resta in `SelfUse`. Si registra ciò che l'EMS interno Afore fa realmente e ciò che il controller avrebbe proposto.

Il confronto corretto non è soltanto:

```text
setpoint shadow vs potenza batteria reale
```

ma:

```text
condizioni iniziali
+ energia disponibile
+ limiti BMS/inverter
+ decisione shadow
+ comportamento osservato Afore
+ risultato energetico
```

## 9.1 KPI

| KPI | Definizione iniziale |
|---|---|
| Disponibilità telemetria | misure valide / misure attese |
| Modbus error rate | errori / richieste |
| Data age P95 | età P95 delle misure di controllo |
| Errore previsione SOC | SOC previsto meno SOC osservato |
| Target hit rate | giorni con target raggiunto entro orario |
| Import during charge | energia importata mentre la batteria carica |
| Setpoint volatility | cambi shadow per ora e variazione media |
| Constraint violations | decisioni scartate dai safety limits |
| EMS disagreement | tempo in cui shadow e Afore seguono strategie diverse |
| Recovery quality | capacità shadow di correggere ritardi senza oscillazioni |

## 9.2 Dataset minimo

Per almeno sette giornate includere:

- una giornata molto soleggiata;
- una giornata variabile;
- una giornata con nebbia o bassa radiazione;
- una giornata con carico domestico improvviso;
- una giornata con SOC iniziale basso;
- almeno un riavvio controllato di Home Assistant;
- una perdita Wi-Fi simulata del controller, senza interrompere Modbus.

---

# PHASE 5 - Hardening e simulazioni di guasto

## 10. Test obbligatori

- timeout Modbus;
- CRC errato;
- registro non disponibile;
- SOC non valido;
- potenza rete con segno invertito;
- timestamp non sincronizzato;
- Wi-Fi assente;
- MQTT assente;
- Home Assistant spento;
- riavvio ESP32;
- brownout;
- setpoint esterno fuori range;
- configurazione corrotta;
- dati congelati;
- sensori contraddittori;
- passaggio giorno/notte;
- raggiungimento SOC massimo/minimo.

## 10.1 Persistenza

Salvare in NVS solo configurazioni modificate realmente:

- parametri seriali;
- limiti verificati;
- capacità batteria;
- target predefiniti;
- credenziali/rete;
- revisione configurazione.

Non salvare continuamente misure, setpoint o heartbeat nella Flash ESP32.

---

# PHASE 6 - Primo test attivo External EMS

## 11. Condizioni di ingresso

La fase attiva è consentita solo dopo:

- Phase 0 completata;
- sette giorni shadow analizzati;
- segno delle potenze verificato;
- limiti inverter/BMS documentati;
- watchdog locale verificato;
- procedura di ritorno a `SelfUse` provata in ambiente controllato;
- nessun conflitto con Solarman;
- build `SEM-V4-CONTROLLER` separata e revisionata;
- possibilità di arresto manuale immediato.

## 11.1 Prima finestra di prova

- presenza fisica sul posto;
- SOC non vicino ai limiti;
- potenza massima iniziale molto conservativa;
- nessuna scarica comandata;
- durata breve;
- monitoraggio di rete, batteria, temperatura ed errori;
- uscita automatica e manuale verso SelfUse;
- log completo di richieste e risposte.

---

# PHASE 7 - Industrializzazione

## 12. Requisiti software

- ESP-IDF con versione fissata;
- repository Git;
- semantic versioning;
- configurazione versionata;
- unit test per planner e safety supervisor;
- test di integrazione Modbus con simulatore;
- static analysis;
- OTA firmato o aggiornamento locale controllato;
- rollback firmware;
- health endpoint;
- log strutturato;
- manuale installazione e recovery.

## 12.1 Requisiti operativi

- morsetti e cablaggio identificati;
- contenitore DIN;
- alimentazione 5 V stabile e protetta;
- antenna esterna posizionata correttamente;
- separazione dai cavi di potenza;
- etichetta hardware/firmware;
- procedura di backup configurazione;
- pulsante o procedura di safe mode;
- comportamento deterministico dopo reboot.

---

## 13. Stato temporaneo dell'impianto

Fino all'arrivo e alla validazione del controller:

- automazione V3.3 disabilitata;
- Afore in `SelfUse`;
- corrente massima di carica/scarica lasciata a valore fisso già accettato dall'impianto;
- nessuna modulazione periodica di `CurrMaxChg`;
- monitoraggio Solarman attivo;
- nessun utilizzo di `ExtEMS`;
- nessuna automazione Sonoff sul pulsante della batteria;
- raccolta baseline dell'EMS interno Afore.

---

## 14. Questioni ancora aperte

1. COM1 può essere utilizzata dal nuovo controller mantenendo il datalogger Solarman?
2. È ammessa la presenza di più master sulla topologia attuale? Presumere di no finché non confermato.
3. Quali sono i parametri seriali effettivi configurati sull'AF5K-SL?
4. Il registro grid power rappresenta import positivo o negativo?
5. Qual è l'esatto comportamento del setpoint External EMS rispetto al prelievo da rete?
6. Come si effettua un ritorno affidabile a SelfUse dopo fault del controller?
7. Quali limiti di carica/scarica sono autorizzati dal BMS master/slave?
8. Qual è la frequenza massima di polling sostenibile senza degradare il bus?
9. SunSpec o Modbus diretto: quale interfaccia offre semantica e fallback migliori?

---

## 15. Decisione architetturale attuale

La strategia approvata è:

1. **Phase 0 con Raspberry Pi in sola lettura.**
2. **Porting sul Waveshare in build shadow senza write path.**
3. **Pubblicazione MQTT dei dati Afore puliti e in tempo reale.**
4. **EMS autonomo locale, migliorato ma non dipendente da Home Assistant.**
5. **Raccolta dati e confronto con l'EMS Afore per almeno una settimana.**
6. **Controllo External EMS soltanto dopo criteri di uscita oggettivi.**

> Prima osserviamo. Poi simuliamo. Poi confrontiamo. Solo alla fine controlliamo.
