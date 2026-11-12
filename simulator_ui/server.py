#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""
Afore AF5K-SL Standalone Local GUI Simulator.

Serves an interactive Web UI on HTTP (default 8080) and a simulated Modbus TCP
server (default 5020) representing the Afore AF5K-SL Hybrid Inverter.
Pure Python standard library (no pip packages required, 100% offline).
"""

import argparse
import http.server
import json
import os
import socketserver
import struct
import sys
import threading
import time

# Default Inverter Simulation State
state_lock = threading.Lock()
state = {
    # Power / Grid block (startAddress 512, 19 regs)
    "grid_power_w": 1250,
    "grid_voltage_v": 230.2,
    "inverter_temp_c": 38.5,
    "total_yield_kwh": 1420.5,
    # PV block (startAddress 552, 10 regs)
    "pv1_power_w": 1800,
    "pv1_voltage_v": 320.0,
    "pv2_power_w": 1200,
    "pv2_voltage_v": 315.0,
    # Battery block (startAddress 2000, 10 regs)
    "battery_soc": 78,
    "battery_power_w": -650,  # negative = charging
    "battery_voltage_v": 51.8,
    # Fault Injection Flags
    "fault_timeout": False,
    "fault_crc": False,
    "fault_partial_frame": False,
    "fault_out_of_range": False,
}

def calculate_modbus_crc(data: bytes) -> int:
    crc = 0xFFFF
    for byte in data:
        crc ^= byte
        for _ in range(8):
            if crc & 0x0001:
                crc = (crc >> 1) ^ 0xA001
            else:
                crc >>= 1
    return crc

def get_registers_for_range(start_addr: int, count: int) -> list:
    regs = [0] * count
    with state_lock:
        if state["fault_out_of_range"]:
            return [65535] * count

        for i in range(count):
            addr = start_addr + i
            # Block 0x0200 (512): Grid & Status
            if addr == 512:
                regs[i] = 1 # Running status
            elif addr == 515: # 0x0203: Grid voltage (0.1V)
                regs[i] = int(state["grid_voltage_v"] * 10)
            elif addr == 516: # 0x0204: Inverter temp (0.1C)
                regs[i] = int(state["inverter_temp_c"] * 10)
            elif addr == 520: # 0x0208: Grid active power (W signed)
                p = int(state["grid_power_w"])
                regs[i] = p if p >= 0 else (65536 + p)
            elif addr == 524: # 0x020C: Total yield low word (0.1 kWh)
                regs[i] = int(state["total_yield_kwh"] * 10) & 0xFFFF
            elif addr == 525: # 0x020D: Total yield high word
                regs[i] = (int(state["total_yield_kwh"] * 10) >> 16) & 0xFFFF
            
            # Block 0x0228 (552): PV Strings
            elif addr == 552: # PV1 Voltage (0.1V)
                regs[i] = int(state["pv1_voltage_v"] * 10)
            elif addr == 553: # PV1 Current (0.1A)
                regs[i] = int((state["pv1_power_w"] / max(1.0, state["pv1_voltage_v"])) * 10)
            elif addr == 554: # PV2 Voltage (0.1V)
                regs[i] = int(state["pv2_voltage_v"] * 10)
            elif addr == 555: # PV2 Current (0.1A)
                regs[i] = int((state["pv2_power_w"] / max(1.0, state["pv2_voltage_v"])) * 10)

            # Block 0x07D0 (2000): Battery
            elif addr == 2000: # Battery SoC (%)
                regs[i] = int(state["battery_soc"])
            elif addr == 2001: # Battery Power (W signed)
                p = int(state["battery_power_w"])
                regs[i] = p if p >= 0 else (65536 + p)
            elif addr == 2002: # Battery Voltage (0.1V)
                regs[i] = int(state["battery_voltage_v"] * 10)

    return regs

def handle_modbus_rtu_frame(frame: bytes) -> bytes:
    if len(frame) < 8:
        return b""
    with state_lock:
        if state["fault_timeout"]:
            time.sleep(1.5)
            return b""

    slave_id = frame[0]
    func_code = frame[1]
    if func_code not in (0x03, 0x04):
        # Unsupported function code exception
        err = bytes([slave_id, func_code | 0x80, 0x01])
        crc = calculate_modbus_crc(err)
        return err + struct.pack("<H", crc)

    start_addr, count = struct.unpack(">HH", frame[2:6])
    regs = get_registers_for_range(start_addr, count)

    byte_count = count * 2
    body = bytearray([slave_id, func_code, byte_count])
    for reg in regs:
        body.extend(struct.pack(">H", reg))

    with state_lock:
        if state["fault_partial_frame"]:
            return bytes(body[:4]) # Truncate

        crc = calculate_modbus_crc(body)
        if state["fault_crc"]:
            crc ^= 0x55AA # Corrupt CRC

    return bytes(body) + struct.pack("<H", crc)

class ModbusTcpHandler(socketserver.BaseRequestHandler):
    def handle(self):
        while True:
            header = self.request.recv(6)
            if not header or len(header) < 6:
                break
            tx_id, proto_id, length = struct.unpack(">HHH", header)
            pdu = self.request.recv(length)
            if not pdu or len(pdu) < length:
                break
            
            unit_id = pdu[0]
            func_code = pdu[1]
            if func_code in (0x03, 0x04) and len(pdu) >= 5:
                start_addr, count = struct.unpack(">HH", pdu[2:6])
                regs = get_registers_for_range(start_addr, count)
                byte_count = count * 2
                resp_pdu = bytearray([unit_id, func_code, byte_count])
                for reg in regs:
                    resp_pdu.extend(struct.pack(">H", reg))
                resp_header = struct.pack(">HHH", tx_id, proto_id, len(resp_pdu))
                self.request.sendall(resp_header + resp_pdu)

class SimulatorHttpHandler(http.server.SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=os.path.dirname(os.path.abspath(__file__)), **kwargs)

    def do_GET(self):
        if self.path == "/api/state":
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.end_headers()
            with state_lock:
                self.wfile.write(json.dumps(state).encode("utf-8"))
            return
        elif self.path == "/api/preview":
            self.send_response(200)
            self.send_header("Content-Type", "text/plain")
            self.end_headers()
            with state_lock:
                txt = f"""
=====================================================
   AFORE AF5K-SL HYBRID INVERTER SIMULATOR STATUS
=====================================================
 PV1 Power:       {state['pv1_power_w']} W ({state['pv1_voltage_v']} V)
 PV2 Power:       {state['pv2_power_w']} W ({state['pv2_voltage_v']} V)
 Total Solar:     {state['pv1_power_w'] + state['pv2_power_w']} W
-----------------------------------------------------
 Grid Power:      {state['grid_power_w']} W ({state['grid_voltage_v']} V)
 Battery SoC:     {state['battery_soc']} % ({state['battery_voltage_v']} V)
 Battery Power:   {state['battery_power_w']} W ({'CHARGING' if state['battery_power_w'] < 0 else 'DISCHARGING'})
 Inverter Temp:   {state['inverter_temp_c']} °C
 Total Yield:     {state['total_yield_kwh']} kWh
-----------------------------------------------------
 FAULT INJECTION STATUS:
  Timeout:        {'[ACTIVE]' if state['fault_timeout'] else 'Inactive'}
  CRC Error:      {'[ACTIVE]' if state['fault_crc'] else 'Inactive'}
  Partial Frame:  {'[ACTIVE]' if state['fault_partial_frame'] else 'Inactive'}
  Out of Range:   {'[ACTIVE]' if state['fault_out_of_range'] else 'Inactive'}
=====================================================
"""
                self.wfile.write(txt.encode("utf-8"))
            return
        return super().do_GET()

    def do_POST(self):
        if self.path == "/api/state":
            length = int(self.headers.get("Content-Length", 0))
            body = self.rfile.read(length).decode("utf-8")
            data = json.loads(body)
            with state_lock:
                for k, v in data.items():
                    if k in state:
                        state[k] = v
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.end_headers()
            self.wfile.write(json.dumps({"status": "ok"}).encode("utf-8"))
            return
        elif self.path == "/api/provision":
            length = int(self.headers.get("Content-Length", 0))
            body = self.rfile.read(length).decode("utf-8")
            data = json.loads(body)
            # Validate input syntax
            ssid = data.get("wifi", {}).get("ssid", "")
            if not ssid or len(ssid) > 32:
                self.send_response(400)
                self.send_header("Content-Type", "application/json")
                self.end_headers()
                self.wfile.write(json.dumps({"error": "invalid_ssid", "message": "SSID length must be 1-32 chars"}).encode("utf-8"))
                return
            # Redact password in echo
            res = {
                "status": "success",
                "message": f"Provisioned WiFi SSID '{ssid}' successfully (dry-run)",
                "mqtt_enabled": data.get("mqtt", {}).get("enabled", False),
                "safe_summary": f"SSID: {ssid}, Password: [REDACTED]"
            }
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.end_headers()
            self.wfile.write(json.dumps(res).encode("utf-8"))
            return
        self.send_error(404)

def run_servers(http_port=8080, modbus_port=5020):
    httpd = socketserver.TCPServer(("", http_port), SimulatorHttpHandler)
    modbusd = socketserver.TCPServer(("", modbus_port), ModbusTcpHandler)

    t_http = threading.Thread(target=httpd.serve_forever, daemon=True)
    t_modbus = threading.Thread(target=modbusd.serve_forever, daemon=True)

    t_http.start()
    t_modbus.start()
    print(f"[SIMULATOR] HTTP UI running at http://127.0.0.1:{http_port}/")
    print(f"[SIMULATOR] Modbus TCP server running on port {modbus_port}")
    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        print("[SIMULATOR] Shutting down.")
        httpd.shutdown()
        modbusd.shutdown()

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Afore AF5K-SL Local Simulator")
    parser.add_argument("--http-port", type=int, default=8080, help="Web UI port (default: 8080)")
    parser.add_argument("--modbus-port", type=int, default=5020, help="Modbus TCP port (default: 5020)")
    args = parser.parse_args()
    run_servers(args.http_port, args.modbus_port)
