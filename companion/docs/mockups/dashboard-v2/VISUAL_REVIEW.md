# Revisione Visiva · Dashboard Live v2 (`dashboard-v2`)

## 1. Riferimenti confrontati
- **Monitor energetico OpenEMS (concettuale)**: disposizione radiale/a croce con inverter al centro, nodi puliti ad alto contrasto, assenza di metadati diagnostici grezzi sul diagramma e immediata leggibilità in < 5 secondi.
- **Mockup premium precedente (`01_dashboard_live_desktop.png`)**: identità visiva scura `#091011`, palette semantica (`#f5b942` FV, `#a78bfa` Casa, `#34d399` Batteria, `#60a5fa` Rete, `#2dd4bf` accento teal, `#38bdf8` SHADOW), banner `SHADOW – Decisioni simulate`.
- **Dashboard attuale (`00_dashboard_before_desktop.png`, `00_dashboard_before_mobile.png`)**: rilevati sovraccarico testuale nei nodi del diagramma (`Misurato FC04`, `Derivato AC`, tensioni/frequenze/età inline), assenza di gerarchia tra KPI percentuali chiave e totali in kWh, falso allarme copertura (`3,6%`–`5,8%`) dovuto al denominatore da mezzanotte in caso di avvio a metà giornata, `"Confronto n.d."` poco esplicativo e lista eventi dominata da occorrenze ripetitive (`Limite Safety applicato: POWER_LIMIT` ripetuto 6 volte).

## 2. Cosa viene mantenuto
- Palette e design tokens dark/light (`tokens.css`) con colori semantici invariati per sorgente (`FV`, `Casa`, `Batteria`, `Rete`, `Safety`, `Fault`, `SHADOW`).
- Banner obbligatorio `SHADOW – Decisioni simulate / Nessun comando applicato all'inverter` e tracciabilità `Scritture Modbus: 0`.
- Principio assoluto **Unknown ≠ Zero**: valori `null` restano assenti/non disponibili, grafici con `connectNulls: false` per evidenziare i gap.
- Link diretti verso `/storico`, `/decisioni` e stato degli 8 sottosistemi.

## 3. Cosa viene sostituito
- **Diagramma rettangolare `PowerFlow`** → **Energy Monitor radiale/a croce (`65%` della riga hero)**:
  - `Fotovoltaico` in alto, `Rete` a sinistra, `Casa` a destra, `Batteria` in basso, `Inverter Afore` al centro su anello orbitale discreto.
  - Rimossi dal canvas testi lunghi (`Misurato FC04`, `Derivato AC`, `V`, `Hz`, età in secondi, messaggi diagnostici); spostati in **popover accessibile** on-click / tastiera per ciascun nodo.
  - Batteria con **SOC ad alta evidenza** e barra di riempimento proporzionale.
- **Card `Stato Impianto & EMS` piatta** → **Card `Decisione EMS` (`35%`)**:
  - Evidenza in linguaggio umano del motivo/vincolo (`Riserva SOC raggiunta: scarica bloccata`, ecc.) invece del solo codice enum grezzo.
  - Pipeline visiva a 3 stadi: `Candidato → Safety-limited → Simulato`, ultima decisione `#seq` e link diretto `"Apri dettaglio decisione →"`.
- **4 KPI uniformi** → **Gerarchia a 2 fasce**:
  - Prima fascia ad alto impatto (`grid-3`): **Autoconsumo** (gauge semicircolare + tooltip formula), **Autosufficienza** (gauge semicircolare + contributo rete), **Batteria** (SOC + carica/scarica + throughput).
  - Seconda fascia compatta (`grid-4`): totali kWh con sparkline orarie reali e spiegazione chiara quando ieri manca (`"Confronto disponibile da domani"`).
- **Coverage fuorviante su 24h** → **Barra Coverage & Qualità esplicita**:
  - Distingue `NON MONITORATO` pre-installazione, `GAP` dopo l'avvio, `INVALIDO` e `UNAVAILABLE`; mostra `"Copertura dal primo campione"` e orario reale di inizio monitoraggio.
  - Vecchi eventi `DECISION_GAP` pre-fix stride contestualizzati/esclusi dai KPI operativi correnti senza cancellare le evidenze storiche.
- **Lista eventi raw (6 righe)** → **Ultimi 3 eventi significativi raggruppati** (es. `"Riserva SOC intervenuta N volte"`) + **8 pill compatte di stato sistema**.
- **Mini grafico statico oggi** → **Grafico compatto `"Andamento ultime 6 ore"`** (`260–300 px`) con selettori rapidi `1h`, `3h`, `6h`, `12h`, `Oggi`, massimo 5 serie (FV, Casa, Batteria, Rete, SOC %).

## 4. Miglioramento della gerarchia visiva (< 5 secondi)
1. **0–2 s**: L'occhio cade sull'**Energy Monitor radiale** (65%) e legge istantaneamente da dove arriva l'energia, dove va e qual è il SOC della batteria.
2. **2–3 s**: La card destra **Decisione EMS** chiarisce subito che il sistema è in **SHADOW** e *perché* sta simulando quel setpoint.
3. **3–5 s**: I due gauge **Autoconsumo / Autosufficienza** e la card **Batteria** danno il bilancio energetico della giornata, supportati subito sotto da trend 6h, coverage trasparente ed eventi raggruppati.

## 5. Seconda revisione visiva (`03_review1_*` → finale)
Dall'ispezione degli screenshot della prima implementazione (`1440×900` dark/light e `390×844`) sono emersi 3 miglioramenti applicati prima del deploy:
1. **Distinzione visiva linea continua (misurato) vs tratteggiata (derivato)**: invece di tratteggiare tutte le linee animate, ogni ramo misurato mantiene una traccia base continua con impulso direzionale animato sovrapposto, mentre `Casa` mantiene il tratteggio discreto sulla traccia base.
2. **Energy Monitor dedicato su smartphone (`<= 680 px`)**: introdotto un SVG radiale proporzionato su `viewBox 340×296` per evitare che il diagramma desktop scalato al 50% riducesse eccessivamente i testi su `390×844`.
3. **Pill `Home Assistant` su singola riga**: riorganizzata la griglia `Stato sistema` da 4 a 2 colonne spaziate per eliminare il wrapping su 2 righe della voce `Home Assistant`.
