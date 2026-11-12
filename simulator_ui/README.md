# Afore AF5K-SL Local Standalone Simulator & Web GUI

An offline, dependency-free inverter emulator providing real-time Modbus register emulation and an interactive Web GUI for testing and development.

## Highlights
- **100% Offline**: Requires only Python 3 standard library (`http.server`, `socketserver`, `struct`).
- **Interactive UI**: Sliders for PV strings, Grid active power, Battery SoC, Voltage, and Inverter Temperature.
- **Modbus Protocol**: Supports Modbus RTU / TCP (Function Codes 0x03 and 0x04) matching Afore register mapping.
- **Fault Injection**:
  - Bus Timeout (Inverter silence)
  - CRC16 Checksum Bitflip Corruption
  - Partial Frame Truncation
  - Out of Range / Saturated Registers (0xFFFF)

## Usage

### 1. Start the simulator:
```bash
python server.py --http-port 8080 --modbus-port 5020
```

### 2. Access the Interactive GUI:
Open your browser at `http://127.0.0.1:8080/`.

### 3. Run Automated Tests:
```bash
python test_simulator.py
```
