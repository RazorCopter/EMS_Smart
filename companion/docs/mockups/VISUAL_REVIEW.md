# Revisione Visiva Mockup · Afore EMS Companion

## Riferimenti confrontati
1. **Mockup di missione fornito**: dark canvas `#091011`, sidebar sinistra con stato connessioni/SHADOW, Power Flow centrale con card PV/Casa/Rete/Batteria, KPI giornalieri con mini-bar chart, anteprime Storico/Decisioni/Confronto SEM/Stato sistema.
2. **Console ESP32 (`src/v51/dashboard.html`)**: identità Afore (icona pannello + sole, verde acqua `#2dd4bf`/`#73e1b2`), badge obbligatorio `SHADOW – Decisioni simulate`, distinzione `Misurato` vs `Derivato` vs `Non disponibile`.
3. **Riferimento concettuale OpenEMS**: diagramma flussi energetici chiaro con direzione e segno coerente, KPI di autoconsumo e autosufficienza, vista History multi-asse con tooltip sincronizzato.

## Esito prima revisione e correzioni applicate
1. **Dashboard Live (`01_dashboard_live_desktop`)**:
   - *Problema rilevato*: il testo `"52,9 V · +13,4 A · Limite ±1.000 W"` sforava leggermente il bordo destro del nodo Batteria (300 px).
   - *Correzione*: nodo Batteria ampliato a 350 px e testo compattato in `"52,9 V · +13,4 A · Cap ±1 kW"`; nell'implementazione React verrà aggiunto il pannello completo a 8 componenti di **Stato Sistema** (ESP32, MQTT, HA, Modbus, NTP, Collector, Database, AUX1).
2. **Storico (`02_history_desktop`)**:
   - *Problema rilevato*: i pulsanti `Esporta JSON` e `Salva PNG` nell'header andavano a capo su una seconda riga.
   - *Correzione*: selettore temporale compatto nell'header e pulsanti `CSV`, `JSON`, `PNG` spostati nella toolbar del grafico; gap telemetria rappresentati con banda grigia esplicita (mai 0).
3. **Decisioni (`03_decisions_desktop`)**:
   - Gerarchia chiara: timeline strategie in alto, grafico `Candidato vs Safety-limited vs Simulato vs Misurato` con envelope Safety, tabella eventi/decisioni sincronizzata con drawer laterale (`executed=false`, `applied=null`, `modbus_write_count=0`).
4. **Confronto SEM / ESP (`04_sem_esp_comparison_desktop`)**:
   - Banner permanente di distinzione fra controllore reale (`SEM`) e simulazione (`ESP SHADOW`), colori distinti (viola continuo per SEM, azzurro tratteggiato per ESP), criteri di concordanza espliciti.
5. **Mobile (`05_dashboard_mobile`)**:
   - *Problema rilevato*: assenza di frecce direzionali sul diagramma Power Flow compatto.
   - *Correzione*: aggiunte frecce direzionali per tutti e 4 i rami e mantenute linee tratteggiate per `Casa (der.)`, card KPI a 2 colonne e bottom navigation a 5 voci (`Live`, `Storico`, `Decisioni`, `SEM`, `Altro`).
