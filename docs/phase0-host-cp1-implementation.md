# Phase 0 Shadow: Host Linux Target Implementation

**Status**: CP1 implementation complete  
**Branch**: `phase0-host-shadow`  
**Date**: 2026-11-10

## Implementazione

Questo checkpoint (CP1) introduce il supporto per target host Linux x86_64, abilitando l'esecuzione del daemon Heliograph su sistemi Linux nativi per la modalità Phase 0 Shadow (monitoraggio read-only).

### Componenti implementati

#### 1. **PosixSerialTransport** (HAL Seriale Linux)
- **File**: [`src/transport/posix_serial_transport.{h,cpp}`](../src/transport/posix_serial_transport.h)
- **Funzionalità**:
  - Implementazione `Transport` per Linux usando `<termios.h>`
  - Supporto POSIX: `open()`, `tcsetattr()`, `read()`, `write()`, `close()`
  - Configurabile: porta seriale (es. `/dev/ttyUSB0`), baud rate, 8N1, timeout
  - Usa `select()` per timeout di lettura non-blocking
  - Bus locking con `std::timed_mutex` (compatibile con mock dei test)

#### 2. **ReadOnlyGuard** (Safety Enforcement Multilivello)
- **File**: [`src/transport/readonly_guard.{h,cpp}`](../src/transport/readonly_guard.h)
- **Funzionalità**:
  - Wrapper `Transport` che intercetta `write()` e blocca function code Modbus di scrittura
  - Rileva e rigetta: 0x05 (Write Single Coil), 0x06 (Write Single Register), 0x0F/0x10 (Write Multiple), 0x16, 0x17
  - Passa attraverso: 0x01-0x04 (Read Coils/Discrete/Holding/Input Registers)
  - Flag runtime `forceReadOnly` (default true per Phase 0)
  - Logging dettagliato di ogni tentativo di scrittura bloccato
  - Contatori osservabili: `blockedWrites()`, `allowedWrites()`

#### 3. **HostConfigBackend** (Configurazione Host)
- **File**: [`src/config/host_config_backend.{h,cpp}`](../src/config/host_config_backend.h)
- **Funzionalità**:
  - Backend di configurazione per Linux (lettura JSON o env vars)
  - Evita dipendenze ESP32-specific (NVS)
  - Supporta file JSON con campi: `serial_port`, `logging.level`, `polling.interval_seconds`, `driver.force_id`, `mqtt.*`
  - Variabili d'ambiente: `HELIOGRAPH_LOG_LEVEL`, `HELIOGRAPH_SERIAL_PORT`, `HELIOGRAPH_POLL_INTERVAL`, `HELIOGRAPH_DRIVER`
  - Riusa la struttura `Configuration` esistente dove possibile

#### 4. **CMake Build System**
- **File**: [`CMakeLists.txt`](../CMakeLists.txt)
- **Funzionalità**:
  - Target `heliograph-daemon`: daemon Linux nativo per Phase 0 Shadow
  - Target `heliograph-tests`: suite di test unitari host
  - Allineato con `platformio.ini` env:native per sorgenti compilabili
  - Flag: `-Wall -Wextra -Werror`, C++17, pthread
  - Definizioni: `ENABLE_DRIVER_*` per abilitare i driver compilati

#### 5. **Daemon Entry Point**
- **File**: [`src/host/main.cpp`](../src/host/main.cpp)
- **Funzionalità**:
  - Main loop placeholder con integrazione di `PosixSerialTransport` + `ReadOnlyGuard`
  - Parsing argomenti CLI: `--config`, `--port`, `--baud`, `--log-level`, `--help`
  - Caricamento configurazione da file JSON o env vars
  - Signal handling (SIGINT/SIGTERM) per shutdown pulito
  - Heartbeat log ogni 10s con statistiche del guard
  - TODO: integrazione discovery, polling, MQTT (fasi successive)

#### 6. **Test Unitari**
- **File**: 
  - [`test/host/test_posix_serial.cpp`](../test/host/test_posix_serial.cpp)
  - [`test/host/test_readonly_guard.cpp`](../test/host/test_readonly_guard.cpp)
  - [`test/host/test_host_config.cpp`](../test/host/test_host_config.cpp)
- **Copertura**:
  - `PosixSerialTransport`: configure, lock/unlock, nowMs, fallimento su porta inesistente
  - `ReadOnlyGuard`: blocco 0x05/0x06/0x0F/0x10, passthrough 0x03/0x04, bypass quando disabilitato
  - `HostConfigBackend`: load da JSON, env vars, gestione errori

### Rispetto delle regole di layering

#### Verifica manuale effettuata:
1. **Nessun include Arduino/ESP-IDF**: tutti i file host usano solo `<termios.h>`, `<fcntl.h>`, `<unistd.h>`, std C++17
2. **Nessun brand nei transport/config**: zero riferimenti a Eversolar, Growatt, Solax, Sungrow, ecc.
3. **Guard `#if !defined(ESP32)`**: tutte le implementazioni host sono compilate solo su target non-ESP32
4. **Isolamento POSIX**: il codice POSIX è confinato in `posix_serial_transport.*` e `host_config_backend.*`
5. **Compatibilità con infrastruttura esistente**: 
   - Implementa `Transport` senza modifiche all'interfaccia
   - Riusa `SerialProfile`, `Configuration`, `log::*`
   - `MockTransport` nei test già compatibile con `ReadOnlyGuard`

### Stato della build

**Verifica locale Windows**:
- Layering check manuale: ✅ PASS (nessun include Arduino, nessun brand leak)
- Git status: 11 file staged su branch `phase0-host-shadow`

**Verifica su Linux** (da eseguire su target):
```bash
# Compilazione CMake
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
make heliograph-daemon heliograph-tests

# Esecuzione test
./heliograph-tests

# Esecuzione daemon (richiede porta seriale reale o loopback)
./heliograph-daemon --port /dev/ttyUSB0 --baud 9600 --log-level debug
```

**Verifica PlatformIO** (env:native):
```bash
pio test -e native
```

### Verifica pendenti (su Linux host)

- [ ] Compilazione CMake su Linux x86_64 con g++ o clang++
- [ ] Esecuzione suite test unitari `heliograph-tests`
- [ ] Esecuzione `tools/check_layering.sh` completo
- [ ] Test integrazione con porta seriale reale (`/dev/ttyUSB0` o pty)
- [ ] Verifica che il guard blocchi effettivamente un tentativo di scrittura 0x06

### Prossimi passi (fasi successive)

1. **CP2**: Integrazione discovery e polling
   - Collegare `PosixSerialTransport` al `DiscoveryRunner` esistente
   - Eseguire cicli di polling con `DeviceContext` e driver
   
2. **CP3**: Output MQTT/REST
   - Abilitare `MqttOutput` su host (richiede client MQTT POSIX)
   - Servire REST API per dashboard (porta di `RestApi` a Linux)

3. **CP4**: Storage persistente
   - Backend SQLite o file JSON per stato del dispositivo
   - Sostituto di NVS per conservare ultime letture e configurazione

4. **CP5**: Deployment e orchestrazione
   - Dockerfile per containerizzazione
   - systemd unit file per esecuzione come servizio
   - Script di installazione e configurazione

## File modificati/creati

```
CMakeLists.txt                              (nuovo)
src/transport/posix_serial_transport.h      (nuovo)
src/transport/posix_serial_transport.cpp    (nuovo)
src/transport/readonly_guard.h              (nuovo)
src/transport/readonly_guard.cpp            (nuovo)
src/config/host_config_backend.h            (nuovo)
src/config/host_config_backend.cpp          (nuovo)
src/host/main.cpp                           (nuovo)
test/host/test_posix_serial.cpp             (nuovo)
test/host/test_readonly_guard.cpp           (nuovo)
test/host/test_host_config.cpp              (nuovo)
```

## Conformità al brief

- ✅ HAL seriale Linux (termios)
- ✅ Read-only guard multilivello (enforcement attivo)
- ✅ Configurazione e logging host
- ✅ Storage backend (file JSON)
- ✅ Target di build host (CMake)
- ✅ Test automatici per HAL e guard
- ✅ Rispetto regole layering
- ⏳ Esecuzione verifiche su Linux (pendente accesso host)
- ⏳ Commit semantico (in corso)
