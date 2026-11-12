# Home Assistant Integration Package — Afore AF5K-SL Hybrid Inverter

Questo package fornisce l'integrazione pronta all'uso per Home Assistant per l'inverter **Afore AF5K-SL** operante in modalità autonoma/shadow (Phase 0 Read-Only).

---

## 1. Caratteristiche dell'Integrazione

1. **Auto-Discovery MQTT (Consigliata)**:
   - Il demone `heliograph-afore-shadow` pubblica automaticamente le specifiche di discovery conforme allo standard Home Assistant sotto il prefisso `homeassistant/sensor/afore_v4/...`.
   - Vengono registrate solo le metriche verificate (`VERIFIED` nel registro) per evitare falsi dati.
   - Lo stato include metadati di qualità (`Good`, `Degraded`, `Stale`, `Invalid`) e latenza/età del campione in millisecondi.

2. **Configurazione Manuale YAML (Opzionale)**:
   - Se l'auto-discovery non è abilitata sul broker MQTT, è possibile includere direttamente il file [`afore_sensors.yaml`](afore_sensors.yaml) nella propria installazione HA.

3. **Dashboard Lovelace Energetica**:
   - Fornita in [`lovelace_dashboard.yaml`](lovelace_dashboard.yaml).
   - Supporta la card `power-flow-card-plus` per la visualizzazione bidirezionale dei flussi (PV -> Batteria -> Rete -> Carichi domestici).
   - Gauge di stato carica (SOC) batteria con soglie cromatiche.
   - Indicatori real-time di bilancio delle potenze (Milestone M8) e salute bus seriale RS485 (Milestone M9).

4. **Automazioni di Protezione e Allerta**:
   - Definite in [`automations.yaml`](automations.yaml).
   - Allerta per apertura circuit-breaker seriale dopo 5 errori consecutivi.
   - Allerta per discrepanza di bilancio energetico persistente (>250W per più di 1 minuto).

---

## 2. Installazione Rapida

### Passo 1: Configurazione Broker MQTT
Assicurarsi che l'integrazione MQTT ufficiale di Home Assistant sia attiva e che `discovery: true` sia impostato.

### Passo 2: Avvio Demone Heliograph
Avviare il demone con il flag discovery abilitato:
```bash
./build/bin/heliograph-afore-shadow \
    --serial /dev/ttyUSB0 \
    --baud 9600 \
    --slave 1 \
    --mqtt-host 192.168.1.100 \
    --mqtt-port 1883 \
    --enable-discovery
```

### Passo 3: Import della Dashboard
1. Andare su **Impostazioni** > **Pannelli di Controllo (Dashboards)** in Home Assistant.
2. Creare una nuova Dashboard o aggiungere una vista in modalità Raw Editor YAML.
3. Incollare il contenuto di [`lovelace_dashboard.yaml`](lovelace_dashboard.yaml).
