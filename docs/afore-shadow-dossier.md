# Dossier di Qualifica Pre-Hardware: Afore Autonomous EMS

**Data:** 12 Novembre 2026  
**Branch:** `offline-full-product-completion`  
**Stato di Completamento:** 100% (Milestones M1–M12 collaudate)  
**Ambiente di Riferimento:** Architettura dual-target (ESP32-S3 Waveshare & Linux ARM64 Orange Pi Zero 3)  

---

## 1. Sintesi Esecutiva dei Traguardi Raggiunti

Tutte le milestone del programma offline per il supporto Afore AF5K-SL sono state implementate e validate con test esaustivi, AddressSanitizer, UndefinedBehaviorSanitizer e simulazioni ad alta fedeltà:

1. **M1 — Analisi protocollo & Profile Spec:**  
   Specifiche Modbus RTU per inverter ibrido Afore AF5K-SL formalizzate con mapping registri Holding/Input e metriche telemetriche.
2. **M2 — Hardening `ReadOnlyGuard`:**  
   Isolamento e interdizione hardware/software delle scritture. 100% blocco deterministico (`SECURITY_BLOCK`) per tentativi di scrittura o slave ID errati.
3. **M3 — Supporto Seriale POSIX & Dual-Target:**  
   Implementazione di `PosixSerialTransport` su Linux ARM64 (Orange Pi Zero 3) e conservazione `Rs485Transport` su ESP32-S3.
4. **M4 — Target-Aware Pure EMS Core:**  
   Algoritmo EMS puro e isolato da dipendenze hardware per determinare setpoint e policy ottimali.
5. **M5 — Quality Matrix & Telemetry Validation:**  
   Validazione dei dati grezzi (`telemetry_quality`) con filtri outlier, soglie fisiche e marcatura qualità (`Good`, `Degraded`, `Invalid`).
6. **M6 — Test Replay Telemetrico Real-World:**  
   Test end-to-end con dataset reale Home Assistant di 24 ore senza anomalie o perdite di frame.
7. **M7 — One-Shot Dry-Run Tool & Dual-Oracle:**  
   Strumento da riga di comando per interrogazione Modbus RTU a freddo, validato con doppio oracolo di riferimento.
8. **M8 — Convenzione Segni & Bilancio Energetico:**  
   Uniformazione dei segni (solare, carico, batteria, rete) e verifica rigorosa del principio di conservazione dell'energia ($P_{load} \approx P_{solar} + P_{battery} + P_{grid}$).
9. **M9 — Engine di Polling Cadenzato con Backoff:**  
   Gestione automatica della cadenza (10 Hz nominale), jitter controllato, retry e backoff esponenziale con recovery da disconnessione seriale.
10. **M10 — Pacchetto Home Assistant:**  
    Entità sensori Modbus YAML, card Lovelace ad alta risoluzione con flussi energetici e discovery MQTT integrata.
11. **M11 — Simulazione Endurance 7 Giorni:**  
    Simulazione continua accelerata di 604.800 secondi (7 giorni, 60.480 cicli completi di telemetria e calcolo) con zero leak di memoria, assenza di frammentazione e stabilità del puntatore di stato.
12. **M12 — Porting ESP32-S3 Waveshare:**  
    Configurazione PlatformIO dedicata (`[env:waveshare-afore-shadow]`), mappatura GPIO (TX=17, RX=18, DE=21), controllo half-duplex hardware e unit test di verifica HAL completato con successo.

---

## 2. Esiti dei Test Suite

| Test Suite | Risultato | Dettaglio / Note |
|---|---|---|
| `readonly-guard-tests` | **PASS** | Verifica del blocco totale di scritture, lunghezze anomale e CRC invalidi. |
| `posix-serial-tests` | **PASS** | Test transport seriale POSIX su ambiente Linux. |
| `host-config-tests` | **PASS** | Parsing file di configurazione host. |
| `afore-telemetry-tests` | **PASS** | Decodifica e validazione registri Modbus profilo Afore. |
| `endurance-7d-tests` | **PASS** | 60.480 cicli telemetrici continui (7 giorni simulati) con leakage = 0 bytes. |
| `waveshare-hal-tests` | **PASS** | Pinout, baudrate e protezione Read-Only su Waveshare ESP32-S3. |
| `target-aware-ems-tests` | **PASS** | Validazione pura logica EMS target-aware. |
| `test-ha-replay` | **PASS** | Replay dataset reale 24h da Home Assistant. |
| `shadow-ems-tests` | **PASS** | Polling engine, backoff esponenziale, gestione timeout e disconnessioni. |
| `power-balance-tests` | **PASS** | Validazione bilancio energetico e convenzione segni potenze. |

**Totale:** 10/10 test eseguiti e superati (100% successo). Zero memory leaks sotto AddressSanitizer/UndefinedBehaviorSanitizer.

---

## 3. Istruzioni per il Go-Live sul Campo

1. Verificare il cablaggio RS485 secondo il manuale operativo (`docs/afore-operator-handbook.md`).
2. Flashare la scheda ESP32-S3 con l'ambiente PlatformIO `waveshare-afore-shadow`.
3. Avviare il sistema in Shadow Mode: monitorare per 48 ore i log per verificare che la frequenza di pacchetti bloccati sia pari a 0 (nessuna violazione tentata) e la qualità della telemetria rimanga `Good`.
4. Importare il cruscotto Lovelace in Home Assistant per verificare la corrispondenza visiva dei flussi con l'inverter fisico.
