# ARM64 Raspberry Pi Portability & Validation Assessment

**Data:** 2026-11-11  
**Branch:** `g1-readiness-consolidated`  
**Stato:** ANALISI ARCHITETTURALE CONCLUSA — VERIFICA SU SILICIO TARGET REGISTRATA COME PENDENTE

---

## 1. Analisi di Portabilità Architetturale (x86_64 vs aarch64)

L'intero codice sorgente di `afore-autonomous-ems` è stato sviluppato nel rispetto rigoroso degli standard di portabilità multipiattaforma:

1. **Standard C++17 Puro:**
   - Nessun uso di estensioni del compilatore non conformi allo standard ISO (`__builtin_*` proprietari, assembly inline `asm()`).
   - Uso esclusivo di tipi a dimensione fissa standard `<cstdint>` (`uint8_t`, `uint16_t`, `uint32_t`, `int16_t`, `int32_t`). Nessuna assunzione sulla dimensione di `int` o `long`.
2. **Gestione Endianness e Allineamento:**
   - La decodifica dei registri Modbus (big-endian di protocollo) è implementata tramite manipolazione esplicita di byte shift (`(b[0] << 8) | b[1]`), indipendente dall'endianness nativa dell'architettura host.
   - Nessun type-punning illegale tramite cast di puntatori a struct (`reinterpret_cast<uint16_t*>`), evitando UB da violazione di strict aliasing o disallineamento su architetture ARM.
3. **Serial Transport POSIX:**
   - L'implementazione [`src/transport/posix_serial_transport.cpp`](../src/transport/posix_serial_transport.cpp) utilizza unicamente header POSIX standard (`<termios.h>`, `<fcntl.h>`, `<unistd.h>`), identici su Debian GNU/Linux x86_64 e Debian 13 trixie / Raspberry Pi OS aarch64.
4. **Toolchain & Build System:**
   - La configurazione `CMakeLists.txt` supporta nativamente generatori Ninja e Make, con flag di compilazione standard `-Wall -Wextra -Werror -std=c++17`.

---

## 2. Stato delle Evidenze di Compilazione ed Esecuzione

| Ambiente di Test | Architettura | Compilatore | Stato Esecuzione | Esito Verificato |
|---|---|---|---|---|
| **Ambiente Locale Dev (Docker)** | `x86_64` (amd64) | GNU GCC 13.3.0 | 100% Eseguito | **PASS:** Tutti i 17 scenari CTest ed M5 superati con zero errori |
| **Raspberry Pi Target (Produzione)** | `aarch64` (ARM64) | GNU GCC 14.2.0 | **Pendente prima dell'accesso fisico** | Nessun ambiente virtualizzato o cross-compiler ARM64 installato sul container locale di dev. L'esecuzione fisica è pianificata prima della connessione Modbus. |

---

## 3. Procedura Operativa di Convalida su Raspberry Pi

Non appena la macchina Raspberry Pi è accessibile via terminale o SSH, eseguire:
```bash
chmod +x scripts/verify_arm64.sh
./scripts/verify_arm64.sh
```

Lo script esegue:
1. Verifica dell'architettura (`uname -m` = `aarch64`).
2. Configurazione CMake con flag rigorosi `-Werror`.
3. Compilazione nativa del daemon Shadow e delle librerie core.
4. Esecuzione integrale della suite `ctest` e del test runner `test-target-aware-ems`.
5. Pulizia automatica dell'ambiente temporaneo di build.
