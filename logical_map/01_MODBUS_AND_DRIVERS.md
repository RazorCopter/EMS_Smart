# 01 — Strato Fisico RS485, Modbus RTU e Driver Afore

Tutte le comunicazioni RS485 avvengono esclusivamente su `loopTask` (nessun accesso concorrente da `httpd` o `v51_mqtt`).

---

## Protocollo e Trasporto

### `src/protocols/byte_order.h` (52 righe)
- **Scopo**: Helper `constexpr`/`inline` per conversioni big-endian (`u16`, `u32`, `s16`, `s32`) tra word Modbus e tipi host little-endian.

### `src/protocols/modbus/modbus_rtu.h` (84 righe) & `modbus_rtu.cpp` (190 righe)
- **Scopo**: Codec puro Modbus RTU (CRC-16/Modbus, builder e parser frame senza I/O).
- **Simboli chiave**:
  - `buildWriteMultipleRegisters(unit, address, words, count, out, cap, len)` — costruisce frame FC10 (`0x10`) con CRC.
  - Builder/parser per FC03 (`0x03`, Holding Registers) e FC04 (`0x04`, Input Registers), validazione lunghezza minima, unit ID, function code, CRC.

### `src/transport/v51_read_only.h` (26 righe) & `v51_read_only.cpp` (139 righe)
- **Scopo**: Driver seriale `HardwareSerial` RS485 con guardia bus passiva e separazione compile-time tra SHADOW e LIVE.
- **Classe `ReadOnlyModbusTransport`**:
  - `begin(baud)`: inizializza UART RS485 (`9600 8N1`).
  - `observeBus()`: controlla che non ci sia traffico imprevisto sul bus a riposo (rileva un eventuale secondo master RS485 concorrente).
  - `flushInput()`: svuota il buffer RX prima di un tentativo di ripristino bus.
  - `readInputRegisters(slave, addr, count, values)`: lettura **FC04**.
  - **Solo se `#ifdef V51_LIVE_TRIAL`**:
    - `readHoldingRegisters(slave, addr, count, values)`: lettura **FC03**.
    - `writeMultipleFrame(frame, size)`: invia frame FC10 pre-costruito, verifica l'eco di risposta FC10 e incrementa `modbusWriteCount()`.
- **Funzione globale `modbusWriteCount()`**: ritorna `0` (`constexpr`) in SHADOW-only, oppure il contatore reale di scritture FC10 in `V51_LIVE_TRIAL`.

---

## Driver Afore e Attuatore LIVE

### `src/drivers/afore/v51_reader.h` (36 righe) & `v51_reader.cpp` (216 righe)
- **Scopo**: Lettore telemetria FC04 dell'inverter Afore e proprietario del bus RS485 (`ownsBus()`).
- **Classe `AforeReader`** (implementa `afore::ExternalEmsBus` quando `V51_LIVE_TRIAL` è attivo):
  - `start(now)`: avvia il trasporto a `kBaud` (`9600`), attende `5 s` in ascolto passivo (`RS485_LISTEN_ONLY`).
  - `tick(now)`: verifica `observeBus()`, esegue il probe iniziale SoC (`kSocProbe` `2002`), poi polla i blocchi FC04 di `v51_profile_generated.h` con schedulazione **`Priority.HIGH / LOW`** (coppia `507` rete + `2000` batteria ravvicinata, alternando i blocchi secondari `552` PV1/PV2 e `2515` temp) per minimizzare lo sfasamento temporale `|grid.stamp - battery.stamp|`.
  - Decodifica ogni registro in `Sample { value, stamp, Quality }`, calcola `state.load = inverter + grid` (con assorbimento piccolo skew CT fino a `-30 W`), resetta `phase_ = 0` se fallisce il blocco `2000` (per evitare coppie `grid`/`battery` sfasate), gestisce `failures_ >= 3 → state_.readHalted = true` con auto-recovery dopo `60 s` (`haltedAt_`, `flushInput()`, `RS485_RECOVERY_ATTEMPT`).
  - Espone `readHolding()` e `writeFrame()` (protetti da `ownsBus()`).

### `src/device/live_actuator.h` (19 righe)
- **Scopo**: Interfaccia astratta `LiveActuator` usata da `LiveExecutor`:
  - `hasControl()`, `verifyNativeMode()`, `enterNativeMode()`, `enterExternalBatteryControl()`, `writeAndReadBack(int watts)`, `release()`.

### `src/drivers/afore/external_ems_frame.h` (21 righe)
- **Scopo**: `externalPowerFrame(uint8_t unit, int32_t deviceWatts)` — costruisce il frame FC10 da 13 byte per il registro `0x0A09` (`int32` big-endian su 2 registri: `high word` in `0x0A09`, `low word` in `0x0A0A`).

### `src/drivers/afore/external_ems_actuator.h` (128 righe)
- **Scopo**: Implementazione concreta `ExternalEmsActuator final : public v51::LiveActuator` per Afore:
  - Registri: `kMode = 0x09C4` (`5` = ExtEms, `1` = Self-Use), `kControlType = 0x0A08` (`0` = potenza batteria), `kPower = 0x0A09`.
  - `observedNativeMode()`: legge `0x09C4` prima di armare (rifiuta se già `5`).
  - `enterExternalBatteryControl()`: verifica `0x0A08 == 0`, scrive `0x09C4 = 5`, verifica readback FC03.
  - `writeAndReadBack(watts)`: applica `invertSign_`, verifica `0x09C4 == 5 && 0x0A08 == 0`, scrive `0x0A09` via FC10, legge FC03 fino a `3` tentativi su errore I/O (nessun retry se il valore letto differisce), traccia `lastError()` (`MODE_OR_CONTROL_LOST`, `WRITE_FAILED`, `READBACK_IO_FAILED`, `READBACK_MISMATCH`).
  - `release()`: se `0x09C4 == 5`, riscrive `nativeMode_` e verifica readback; se è già `nativeMode_`, evita scritture inutili su EEPROM.

---

## File Legacy nella cartella `src/drivers/afore/` e `src/protocols/modbus/` (Non usati da v5.1)
- `afore_telemetry_engine.{h,cpp}`, `afore_v4_mqtt.{h,cpp}`, `modbus_client.{h,cpp}`: residui delle fasi v4/precedenti esclusi da `build_src_filter = -<*>` in `platformio.ini` (mantenuti solo per compatibilità/riferimento storico nel repo).
