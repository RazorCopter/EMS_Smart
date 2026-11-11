# Afore AF5K-SL — Register Provenance & Phase 0 Conservative Probe Plan

> **STATUS:** Baseline Documentale e Architetturale Pre-G1 (Milestone M6 Consolidata).
> **EXECUTION CONSTRAINT:** 100% OFFLINE DOCUMENTAL VALIDATION. Nessun accesso a porte seriali, nessun probe su bus fisico RS485, nessuna interrogazione hardware. L'interrogazione è formalmente bloccata fino all'approvazione del Gate G1 da parte dell'utente.

---

## 1. Fonti Documentali & Tassonomia dei Registri

La provenienza dei registri dell'inverter ibrido monofase Afore AF5K-SL (5 kW) deriva da quattro fonti primarie e da una fixture sintetica di validazione:

1. **Fonte A1 (Foglio Ufficiale Afore v3.4):**
   `Doc/艾伏逆变器寄存器参数表（对外）v3.4_20251119_并网机和储能机分类(2).xlsx`
   - Foglio **`运行信息04`** (Operational Telemetry): codificato rigorosamente come **Modbus Function Code 04 (Read Input Registers)**. Include tutti i dati operativi in tempo reale: SOC batteria, tensioni, correnti, potenze FV e di rete, temperature.
   - Foglio **`参数读写03-06-10`** (Parameters Read/Write): codificato come **FC 0x03, 0x06, 0x10 (Holding Registers)**. Include configurazioni statiche (numero seriale) e parametri protetti su EEPROM non volatile.
2. **Fonte A2 (Specifica Ufficiale External EMS En_240312):**
   `Doc/外部Ems调度  - En_240312.docx` (External EMS Modbus Protocol Specification). Definisce i registri di controllo esterno e setpoint attivi (dormienti e interdetti in Phase 0).
3. **Fonte B (Roadmap Afore Autonomous EMS V4):**
   `Doc/Afore_Autonomous_EMS_V4_Roadmap.md` (Sezione 4: Telemetria e convenzioni di segno; Sezione 5: Protezione EEPROM).
4. **Fonte C (Integrazione Solarman Home Assistant su Impianto Reale):**
   `afore_AF5K-Sl.yaml` (configurazione Solarman di produzione, impiega FC04 per i sensori di telemetria operativa).
5. **Fixture di Riferimento Sintetica:**
   `test/test_afore/fixtures/af5k_sl_sample.json` (esplicitamente etichettata come fixture di test sintetica offline, allineata a FC04 / `space: "input"`).

---

## 2. Riconciliazione Conflitto FC03 / FC04 e Audit Discrepanze Valori SOC (0x07D2)

### 2.1 Riconciliazione del Function Code (FC03 vs FC04)
- **Problema rilevato:** Versioni preliminari indicavano genericamente FC03 ("holding registers") per l'intera mappa.
- **Risoluzione evidenziale:** L'ispezione analitica riga per riga del documento ufficiale Afore v3.4 dimostra che il registro `0x07D2` (2002 decimale, `电池电量（SOC）`) è posizionato al rigo 163 del foglio **`运行信息04`**, la cui colonna del codice funzione impone inequivocabilmente **Function Code 04 (Read Input Registers)**.
- **Decisione pre-G1:** La proposta operativa per M7 adotta esclusivamente **FC04**. Il profilo TOML `profiles/afore/af5k_sl.toml` e la fixture sintetica sono stati allineati a `space = "input"` (FC04). Nessuna inferenza generica a FC03 è consentita per i dati di telemetria.

### 2.2 Riconciliazione dei Valori SOC (27% vs 80%)
- L'audit approfondito del repository dimostra che:
  1. Il valore **27%** (`0x001B`) derivava da osservazioni visive storiche su dashboard Solarman remota, ma **nessun bus trace grezzo o frame seriale reale è stato salvato o trattenuto nel workspace**.
  2. Il valore **80%** (`0x0050`) proviene dal payload sintetico [`test/test_afore/fixtures/af5k_sl_sample.json`](../test/test_afore/fixtures/af5k_sl_sample.json), creato ad hoc per la validazione offline dell'algoritmo di scaling e delle convenzioni di segno.
- **Conclusione e Riclassificazione:** Nessuno dei due valori può essere dichiarato come "verificato empiricamente su bus hardware". Di conseguenza, il registro `0x07D2` è formalmente classificato come **`PROVISIONAL_FIXTURE_VERIFIED`** (convalidato su fixture sintetica, in attesa di osservazione empirica reale su bus in M7).

---

## 3. Matrice Completa di Provenienza dei Registri (Audit a 7 Colonne)

Di seguito l'audit esaustivo per ogni singolo registro considerato nel piano Afore:

| Address (Hex / Dec) | Planned Function Code | Documentary Source | Fixture Evidence | Empirical Evidence | Confidence | Final Status | Note Tecniche & Semantica |
|---|---|---|---|---|---|---|---|
| **`0x07D2`** (2002) | **FC 0x04** (Input) | A1 (`运行信息04`, R163), B (§4), C | Presente (`af5k_sl_sample.json`, SOC=80%) | Nessun frame bus reale trattenuto (27% da dashboard) | Alta (doc) / Media (hw) | **`PROVISIONAL_FIXTURE_VERIFIED`** | Battery SOC (0–100%, scale 1, U16). Target della singola richiesta bounded one-shot in M7. |
| **`0x07D0`** (2000) | **FC 0x04** (Input) | A1 (`运行信息04`, R161), B (§4) | Presente (BatState=4, Charge) | Nessun frame bus reale | Media | **`PROVISIONAL`** | Battery Running State enum (4=Charge, 5=Discharge, 6=Standby). U16. |
| **`0x07D4`** (2004) | **FC 0x04** (Input) | A1 (`运行信息04`, R165), B (§4), C | Presente (Vbat=520 -> 52.0 V) | Nessun frame bus reale | Media | **`PROVISIONAL`** | Battery Voltage (U16, scale 0.1 V). |
| **`0x07D5`** (2005) | **FC 0x04** (Input) | A1 (`运行信息04`, R166), B (§4), C | Presente (Ibat=-60 -> -0.60 A) | Nessun frame bus reale | Media | **`PROVISIONAL`** | Battery Current (S16, scale 0.01 A, positivo=scarica, negativo=carica). |
| **`0x07D8`** (2008) | **FC 0x04** (Input) | A1 (`运行信息04`, R168), B (§4), C | Presente (Pbat=-200 W) | Nessun frame bus reale | Media | **`PROVISIONAL`** | Battery Power (S32 high/low in R168, o S16 word). Convenzione di segno da validare post-G2. |
| **`0x022A`** (554) | **FC 0x04** (Input) | A1 (`运行信息04`), B (§4), C | Presente (Ppv_total=1200 W) | Nessun frame bus reale | Media | **`PROVISIONAL`** | Total PV DC Power (U16, scale 1 W). |
| **`0x022D`** (557) | **FC 0x04** (Input) | A1 (`运行信息04`, R70), B (§4) | Presente (Ppv1=800 W) | Nessun frame bus reale | Media | **`PROVISIONAL`** | PV String 1 Power (U16, scale 1 W). |
| **`0x0230`** (560) | **FC 0x04** (Input) | A1 (`运行信息04`, R73), B (§4) | Presente (Ppv2=0 W) | Nessun frame bus reale | Media | **`PROVISIONAL`** | PV String 2 Power (U16, scale 1 W). |
| **`0x0228`** (552) | **FC 0x04** (Input) | A1 (`运行信息04`, R68), C | Presente (Vpv1=346.8 V) | Nessun frame bus reale | Media | **`PROVISIONAL`** | PV String 1 Voltage (U16, scale 0.1 V). Offset tabella monofase vs trifase. |
| **`0x0229`** (553) | **FC 0x04** (Input) | A1 (`运行信息04`, R69), C | Presente (Ipv1=2.5 A) | Nessun frame bus reale | Media | **`PROVISIONAL`** | PV String 1 Current (U16, scale 0.1 A). |
| **`0x022E`** (558) | **FC 0x04** (Input) | A1 (`运行信息04`, R71), C | Presente (Vpv2=290.0 V) | Nessun frame bus reale | Media | **`PROVISIONAL`** | PV String 2 Voltage (U16, scale 0.1 V). |
| **`0x022F`** (559) | **FC 0x04** (Input) | A1 (`运行信息04`, R72), C | Presente (Ipv2=1.5 A) | Nessun frame bus reale | Media | **`PROVISIONAL`** | PV String 2 Current (U16, scale 0.1 A). |
| **`0x0212`** (530) | **FC 0x04** (Input) | A1 (`运行信息04`, R55), B (§4), C | Presente (Grid Power) | Nessun frame bus reale | Media | **`PROVISIONAL`** | Grid Active Power (S16/S32, scale 1 W). Segno import/export da confermare post-G2. |
| **`0x020B`** (523) | **FC 0x04** (Input) | A1 (`运行信息04`, R51), B (§4), C | Presente (Pinv=100 W) | Nessun frame bus reale | Media | **`PROVISIONAL`** | Inverter Output Active Power (S16, scale 1 W). |
| **`0x0203`** (515) | **FC 0x04** (Input) | A1 (`运行信息04`, R47) | Presente (Vgrid=231.2 V) | Nessun frame bus reale | Bassa | **`PROVISIONAL`** | Grid AC Voltage (U16, scale 0.1 V). |
| **`0x0205`** (517) | **FC 0x04** (Input) | A1 (`运行信息04`, R49) | Presente (Freq=50.00 Hz) | Nessun frame bus reale | Bassa | **`PROVISIONAL`** | Grid Frequency (U16, scale 0.01 Hz). |
| **`0x020C`** (524) | **FC 0x04** (Input) | A1 (`运行信息04`), B (§4) | Presente (Temp=10.0 °C) | Nessun frame bus reale | Bassa | **`PROVISIONAL`** | Inverter Internal Temperature (S16, scale 0.1 °C). |
| **`0x09C4`** (2500) | FC 0x04 / FC 0x03 | A1 (`运行信息04`, R177) vs A2 (§3.1) | Assente | Nessuna evidenza | Bassa | **`DOCUMENTED_CONFLICTED`** | In A1 (FC04) è `RunState` (0..11). In A2 (FC03/FC06) è `EMS Mode` (5=External EMS). Escluso e bloccato. |
| **`0x0A08`** (2568) | FC 0x03 / FC 0x10 | A2 (§3.2, External EMS) | Assente | Nessuna evidenza | N/A (escluso) | **`DOCUMENTED_CONTROL_EXCLUDED`** | External Control Type. Escluso dal profilo read-only in Phase 0. Nessun probe consentito. |
| **`0x0A09`–`0x0A0A`** | FC 0x03 / FC 0x10 | A2 (§3.3, External EMS) | Assente | Nessuna evidenza | N/A (escluso) | **`DOCUMENTED_CONTROL_EXCLUDED`** | Active Power Setpoint (S32). Escluso dal profilo read-only. Nessun probe consentito. |
| **`0x0BC1`** (3009) | FC 0x03 / FC 0x06 | A1 (`参数读写`, R142), B (§5) | Assente | Nessuna evidenza | N/A (hazard) | **`CONFIGURATION_EXCLUDED`** | `CurrMaxChg` — Scrittura su memoria EEPROM non volatile. PERICOLO WEAR-OUT. Bloccato a vita. |
| **`0x0BC2`** (3010) | FC 0x03 / FC 0x06 | A1 (`参数读写`, R143), B (§5) | Assente | Nessuna evidenza | N/A (hazard) | **`CONFIGURATION_EXCLUDED`** | `CurrMaxDischg` — Scrittura su memoria EEPROM non volatile. PERICOLO WEAR-OUT. Bloccato a vita. |

---

## 4. Separazione Rigorosa Gate G1 e Gate G2: Piano Operativo M7

### 4.1 Rimozione Totale dei Comportamenti Post-G2 da M7
In conformità alle direttive di sicurezza, **tutti i comportamenti ricorrenti, di loop e di endurance sono stati formalmente rimossi dal perimetro pre-G2/M7**:
- ❌ **CANCELLATO:** Polling SOC ogni 15 secondi.
- ❌ **CANCELLATO:** Polling potenze ogni 10 secondi.
- ❌ **CANCELLATO:** Loop continuativi o schedulati.
- ❌ **CANCELLATO:** Ritrasmissioni automatiche (retries).
- ❌ **CANCELLATO:** Test di endurance e prove di carico sul bus.

Tali funzionalità appartengono esclusivamente alla fase **post-G2** (Supervisione Continua e Monitoraggio Ciclico).

### 4.2 Specifica Esatta della Singola Richiesta Bounded One-Shot in M7
La Milestone M7 è autorizzata ad eseguire **una ed una sola** transazione Modbus RTU per validare la presenza hardware dell'inverter:

- **Target Dispositivo:** Inverter Afore AF5K-SL
- **Slave ID:** `0x01` (1)
- **Modbus Function Code:** **`0x04`** (`Read Input Registers`, conforme a Fonte A1 foglio `运行信息04`)
- **Start Address:** `0x07D2` (2002 decimale, Battery SOC)
- **Quantity of Registers:** `0x0001` (1 registro = 2 byte)
- **Frame Trasmesso (TX Payload Esatto, 8 byte inclusivo di CRC16 Modbus):**
  ```
  01 04 07 D2 00 01 90 87
  ```
  *(Dettaglio: `0x01` Slave, `0x04` FC, `0x07 0xD2` Addr, `0x00 0x01` Count, `0x90` CRC Low, `0x87` CRC High)*
- **Risposta Attesa (RX Payload Esatto, 7 byte):**
  ```
  01 04 02 [VALUE_HI] [VALUE_LO] [CRC_LO] [CRC_HI]
  ```
- **Validazione CRC:** Calcolo obbligatorio del CRC-16 Modbus (polinomio `0xA001`, init `0xFFFF`). Se il CRC non coincide, il pacchetto viene scartato istantaneamente.
- **Timeout Transazione:** **<= 1000 ms**. Trascorso 1 secondo senza risposta completa, la transazione viene abortita.
- **Politica di Errore:** **Abort Immediato (Zero Retries).** Nessun tentativo ripetuto o iterativo.
- **Verifica Lockout Scritture:**
  - Controllo pre-trasmissione: interrogazione del `ReadOnlyGuard` a livello 3 per verificare che la trasmissione sia esclusivamente FC04 e non broadcast (`slave != 0`).
  - Controllo post-ricezione: verifica che nessun byte di scrittura sia mai transitato sul bus.

---

## 5. Regole di Convivenza Bus RS485 & Solarman Stick

1. **Assenza di Conflitti:** Esecuzione di una singola query one-shot con gap di silenzio minimo `t_gap >= 200 ms` pre e post esecuzione.
2. **Formato Seriale:** 9600 baud, 8 bit dati, nessuna parità (`parity = "none"`), 1 stop bit (`stop_bits = 1`).
3. **Trip Counter di Sicurezza:** In caso di errore o timeout sulla singola lettura, il runner si arresta immediatamente senza tentare ulteriori operazioni.

---

## 6. Riepilogo Evidenze Pre-G1

- [x] Conflitto FC03/FC04 risolto documentalmente a favore di FC04 (fonte Excel Afore v3.4 foglio `运行信息04`).
- [x] Profilo `profiles/afore/af5k_sl.toml` allineato a `space = "input"` (FC04).
- [x] Fixture sintetica `af5k_sl_sample.json` allineata a FC04 / `space: "input"`.
- [x] Rimozione totale di loop, polling periodico (10s/15s) e prove di endurance da M7.
- [x] Definizione esatta del frame Modbus one-shot TX `01 04 07 D2 00 01 90 87`.
- [x] Arresto formale del sistema in attesa della decisione di Gate G1.
