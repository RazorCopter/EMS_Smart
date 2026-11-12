# Manuale Operativo e Guida all'Installazione: Afore Shadow EMS

**Prodotto:** Heliograph / Afore Autonomous EMS  
**Hardware di destinazione:** Waveshare ESP32-S3-RS485-CAN & Orange Pi Zero 3 (ARM64 host)  
**Inverter target:** Afore AF5K-SL Hybrid Inverter (Monofase 5 kW)  
**Modalità operativa:** Phase 0 — Strict Shadow Read-Only Mode  
**Data revisione:** Novembre 2026  

---

## 1. Architettura di Sicurezza e Garanzie Read-Only

Durante la **Phase 0 Shadow Mode**, il sistema opera come osservatore passivo non intrusivo:
1. **Protezione Software Rigida (`ReadOnlyGuard`):**  
   Intercetta ogni byte inviato verso il bus Modbus RTU. Solo le letture FC03 (Read Holding) e FC04 (Read Input) con Slave ID corrispondente all'inverter (default `0x01`), lunghezza esatta di 8 byte e CRC-16 valido vengono inoltrate al transceiver. Qualsiasi tentativo di scrittura (FC05, FC06, FC15, FC16) o frame non autorizzato viene bloccato a livello di guard (`SECURITY_BLOCK`), incrementando i contatori di violazione senza trasmettere alcun byte sul bus fisico.
2. **Nessun Rischio di Alterazione:**  
   Nessun registro di configurazione o setpoint di carica/scarica batteria dell'Afore AF5K-SL può essere modificato durante questa fase.

---

## 2. Cablaggio Hardware e Connessioni RS485

### Pinout Scheda Waveshare ESP32-S3-RS485-CAN
- **Morsettiera A (D+):** Collegare al pin RS485-A della porta seriale inverter Afore AF5K-SL.
- **Morsettiera B (D-):** Collegare al pin RS485-B della porta seriale inverter Afore AF5K-SL.
- **GND (Opzionale/Schermatura):** Collegare alla massa/schermo del cavo twistato per evitare anelli di terra.
- **Resistore di terminazione:** Se l'ESP32 è l'ultimo nodo del bus, inserire il jumper per la resistenza di terminazione a 120 Ω integrata sulla board.

### Parametri di Comunicazione Seriale
- **Baud Rate:** `9600 bps`
- **Data Bits:** `8`
- **Parity:** `None`
- **Stop Bits:** `1`
- **Slave ID Predefinito:** `1` (`0x01`)

---

## 3. Configurazione Software e Deployment

### 3.1 Build ESP32-S3 (PlatformIO)
Per generare l'immagine firmware per la scheda Waveshare ESP32-S3-RS485-CAN:
```bash
pio run -e waveshare-afore-shadow
```
Il binario generato include le definizioni pinout corrette (TX=GPIO17, RX=GPIO18, DE=GPIO21, UART1 in half-duplex RTS automatico) e la `ReadOnlyGuard` vincolata a tempo di compilazione.

### 3.2 Build Host / Gateway Linux (Orange Pi Zero 3 ARM64)
Per compilare la suite EMS su architettura ARM64 o PC Linux:
```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON -DAFORE_SHADOW_BUILD=ON
cmake --build build
```

---

## 4. Strumenti Operativi e Diagnostica Pre-Hardware

### 4.1 Simulatore e Dry-Run One-Shot
Per verificare l'estrazione della telemetria prima di collegare il cavo fisico all'inverter:
```bash
# Esecuzione del test di interrogazione a un colpo
./build/tools/afore_dry_run_query --mock-profile
```

### 4.2 Home Assistant Integration
I file di configurazione Home Assistant generati si trovano in `integrations/homeassistant/`:
- `afore_sensors.yaml`: Definizione completa delle entità Modbus sensor.
- `afore_energy_dashboard.yaml`: Configurazione card Lovelace per monitoraggio flussi di potenza (Solare PV1/PV2, Batteria con carica/scarica bidirezionale, Prelievo/Immissione Rete, Consumi domestici).
- `afore_mqtt_discovery.json`: Autodiscovery MQTT zero-conf con telemetria a 10 Hz.

---

## 5. Convenzione Segni e Bilancio Energetico

Il sistema adotta una convenzione univoca per le potenze:
- **`p_solar` (W):** Sempre $\ge 0$ (produzione dai moduli fotovoltaici stringhe 1 e 2).
- **`p_load` (W):** Sempre $\ge 0$ (consumo elettrico dei carichi domestici).
- **`p_battery` (W):**
  - **Positivo ($> 0$):** Scarica batteria (fornisce energia alla casa).
  - **Negativo ($< 0$):** Carica batteria (accumula energia da solare o rete).
- **`p_grid` (W):**
  - **Positivo ($> 0$):** Prelievo da rete (importazione energia).
  - **Negativo ($< 0$):** Immissione in rete (esportazione energia).

**Formula del Bilancio Energetico Istantaneo:**
$$P_{\text{load}} \approx P_{\text{solar}} + P_{\text{battery}} + P_{\text{grid}}$$
(Discrepanza ammessa dovuta alle perdite termiche e rendimento dell'inverter: tipicamente $< 5\%$).
