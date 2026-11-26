# Logical Map — Firmware ESP32-S3 (`afore-ems-v51-prototype`, `v5.1-live-trial-18`)

> Nota Git/Deploy: il firmware ESP32 vive sul branch `v51-prototype` (`origin`: `RazorCopter/EMS_Smart`). Il Companion Docker live (Portainer GitOps) legge invece il branch `main` del repository `https://github.com/RazorCopter/dashboard_ems.git` (remote `dashboard_ems`, con `companion/` alla root).

Questa cartella (`logical_map/`) fornisce una mappa file-per-file completa del firmware ESP32-S3 v5.1, pensata per dare contesto immediato a un'altra IA o sviluppatore.

## Indice dei documenti
1. **`00_INDEX_AND_BUILD.md`** (questo file): build system, partizioni, hardware board, profilo inverter, generatori pre-build.
2. **`01_MODBUS_AND_DRIVERS.md`**: strato fisico RS485, framing Modbus RTU, polling `Priority.HIGH/LOW`, attuatore `ExternalEmsActuator`.
3. **`02_EMS_CORE_AND_LIVE_CONTROL.md`**: boot/loop (`main.cpp`), `StrategyManager` (`shadow_core.h`), `ems_runtime`, `LiveExecutor`, `TrainerResponse`, `live_trial`, e test unitari host.
4. **`03_NETWORK_UI_MQTT_AND_TIME.md`**: Wi-Fi/HTTP/OTA (`remote`), MQTT/HA (`m4`, `m5`), clock/RTC/NTP (`clock_service`, `rtc_time`, `iana_zones`), e dashboard web (`dashboard.html`).

---

## File di Build, Configurazione e Hardware

### `platformio.ini` (495 righe)
- **Scopo**: Definisce gli ambienti PlatformIO per ESP32-S3 (`esp32-s3-devkitc-1`, Arduino framework, `16 MB` Flash QIO, `8 MB` PSRAM OPI, C++17).
- **Ambienti v5.1 chiave**:
  - `waveshare-1ch-v51`: build **SHADOW-only** (`-DV51_BUILD_ID="v5.1-console-8-m9b"`). Esclude `v51/live_trial.cpp` e ogni simbolo di scrittura Modbus.
  - `waveshare-1ch-v51-live-trial`: estende `waveshare-1ch-v51` aggiungendo `-DV51_LIVE_TRIAL=1` e `+<v51/live_trial.cpp>` per l'esecuzione controllata `ExtEms` / `Trainer` / `NativeDelegated`.
  - `waveshare-1ch-v51-ota-failure`: test di rollback OTA su banco.
- **Pre-build scripts**: `tools/gen_v51_profile.py` e `tools/gen_v51_dashboard.py`.

### `partitions_v51_16mb.csv` (5 righe)
- **Scopo**: Tabella partizioni ESP32-S3 da 16 MB con doppio slot OTA (`app0` e `app1` da 6 MB ciascuno), `nvs`, `otadata`, per rollback automatico in caso di mancata probation (60 s).

### `profiles/afore/af5k_sl.toml` (263 righe)
- **Scopo**: Profilo dichiarativo dell'inverter **Afore AF5K-SL** (monofase 5 kW ibrido con batteria):
  - Seriale: `9600 baud, 8N1`, trasporto `rtu`.
  - 4 blocchi FC04 (`start/count`): `507/24` (rete/inverter), `552/9` (PV1/PV2), `2000/9` (batteria/SoC), `2515/1` (temperatura radiatore).
  - Mappatura registri (`address`, `type` u16/s16/u32/s32, `scale`, `unit`) e note di provenienza.

### `tools/gen_v51_profile.py` (49 righe)
- **Scopo**: Script pre-build Python che legge `profiles/afore/af5k_sl.toml` e genera `v51_profile_generated.h` (costanti `kBaud`, `kBlocks[]`, `kFields[]`) incluso da `src/drivers/afore/v51_reader.cpp`.

### `tools/gen_v51_dashboard.py` (16 righe)
- **Scopo**: Script pre-build Python che comprime `src/v51/dashboard.html` in gzip e genera `dashboard_gzip.h` incluso da `src/v51/dashboard.cpp`.

### `src/boards/waveshare_esp32_s3_relay_1ch.h` (112 righe)
- **Scopo**: Definizione pinout della scheda Waveshare ESP32-S3-Relay-1CH (`kRelayPins = {47}`, `kRelayActiveHigh`, pin RS485 TX/RX/DE, pin I2C RTC `kRtcSda`/`kRtcScl`).

### `src/json_limits.h` (34 righe)
- **Scopo**: Limiti statici di dimensione payload JSON/HTTP usati per validazione difensiva lato server.

### `src/v51/ota_public_key.h` (9 righe)
- **Scopo**: Chiave pubblica **ECDSA P-256** (formato PEM/DER) usata da `remote.cpp` per verificare la firma `X-Firmware-Signature` degli upload OTA su `/api/v1/ota`.
