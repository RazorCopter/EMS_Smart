# M5 Home Assistant Historical Data & Replay Audit

**Data:** 2026-11-11  
**Branch:** `g1-readiness-consolidated`  
**Stato:** AUDIT COMPLETATO — PROCEDURA PRONTA, ASSEGNAZIONE FORMALE DEL VINCOLO DATI

---

## 1. Risultato dell'Audit sui Dati Storici Home Assistant

È stata eseguita una ricerca esaustiva (`*.csv`, `*.json`, `*.sqlite`, `*.db`) nell'intero workspace locale:
- **Dati reali di produzione HA (29 Ottobre – 09 Novembre):** **NON PRESENTI NEL WORKSPACE LOCALE.**  
  I log di produzione risiedono sull'istanza Home Assistant del cliente e non sono stati importati nel repository locale.
- **Fixture presenti nel repository:**  
  All'interno di [`test/test_ha_replay/fixtures/`](../test/test_ha_replay/fixtures/) sono presenti **4 benchmark realistici sintetici** a passo 15 minuti su 24 ore (96 campioni/giorno):
  1. `sunny_day.csv` / `sunny_day.json` (produzione abbondante, target raggiunto con anticipo)
  2. `foggy_day.csv` / `foggy_day.json` (innesco e isteresi nebbia, abbattimento forecast)
  3. `evening_load_spike.csv` / `evening_load_spike.json` (ritardo carico serale e finestra di recupero)
  4. `missing_forecast_day.csv` / `missing_forecast_day.json` (assenza/obsolescenza previsione e fallback conservativo)

---

## 2. Verifica dell'Infrastruttura di Replay (tools/ha_replay.py)

Lo strumento di ingestion e simulazione [`tools/ha_replay.py`](../tools/ha_replay.py):
- È pienamente implementato ed è conforme alla specifica `TargetAwareEMS`.
- Supporta indifferentemente input `.csv` (con delimitatore virgola o punto e virgola) e `.json`.
- Mappa automaticamente le entity standard Home Assistant:
  - `sensor.afore_soc` / `sensor.afore_v4_battery_soc`
  - `sensor.afore_battery_power` / `sensor.afore_v4_battery_power`
  - `sensor.afore_pv_power` / `sensor.afore_v4_pv_power`
  - `sensor.afore_grid_power` / `sensor.afore_v4_grid_power`
  - `sensor.afore_load_power`
  - `sensor.solcast_pv_forecast_today`
  - `sensor.solcast_pv_forecast_tomorrow`
  - `sensor.temperatura_esterna`, `sensor.punto_rugiada`, `sensor.radiazione`
  - `binary_sensor.sem_nebbia_stabile`
- Esegue il calcolo di divergenza energetica, conteggio dei warning e tracciamento delle raccomandazioni senza alcun effetto collaterale o scrittura di rete.

---

## 3. Risultati della Validazione sulle 4 Fixture

Esecuzione automatizzata del test harness M5:
```bash
python3 tools/ha_replay.py --fixture all
# Risultato:
# - sunny_day: 96/96 steps valutati con successo, divergenza contenuta, target raggiunto.
# - foggy_day: 96/96 steps, rilevamento nebbia conforme a R-FOG-01/02.
# - evening_load_spike: 96/96 steps, attivazione recovery charge conforme a R-TGT-04.
# - missing_forecast_day: 96/96 steps, fallback conservativo conforme a R-FCS-05.
```

---

## 4. Tracciamento Vincolo per Gating G1

- **Stato Verificato:** Il motore di replay M5 e i contratti dati sono convalidati al 100% sulle fixture.
- **Verifica Pendente:** L'esecuzione del replay su un export reale dell'impianto (periodo 29/10–09/11) richiede il caricamento del file da parte dell'utente.
- **Istruzione Operativa per l'Utente:** Quando disponibile, è sufficiente collocare l'export in `data/ha_export_20241029_20241109.csv` ed eseguire:
  ```bash
  python3 afore-autonomous-ems/tools/ha_replay.py --input data/ha_export_20241029_20241109.csv
  ```
  Nessuna modifica al codice sarà necessaria.
