#!/usr/bin/env python3
"""Temporary Linux read-only acquisition adapter for the existing web UI.

The register map remains in the TOML profile. No control operations are exposed.
"""
import argparse
import copy
import json
import pathlib
import struct
import threading
import time
import tomllib

import serial
import web_ui


def crc16(data):
    value = 0xFFFF
    for byte in data:
        value ^= byte
        for _ in range(8):
            value = (value >> 1) ^ 0xA001 if value & 1 else value >> 1
    return value


class Acquisition:
    def __init__(self, profile, port, baud, slave):
        self.profile = profile
        self.port, self.baud, self.slave = port, baud, slave
        self.lock = threading.Lock()
        self.started = time.monotonic()
        self.values = {}
        self.successes = self.failures = self.consecutive = 0
        self.last_success = None
        self.last_error = "Waiting for first physical read"

    def exchange(self, link, block):
        # Only read function codes can be constructed here.
        fc = {"input": 4, "holding": 3}[block["space"]]
        count = block["count"]
        if not 1 <= self.slave <= 247 or not 1 <= count <= 125:
            raise ValueError("Invalid read request")
        payload = struct.pack(">BBHH", self.slave, fc, block["start"], count)
        link.reset_input_buffer()
        link.write(payload + struct.pack("<H", crc16(payload)))
        link.flush()
        header = link.read(3)
        if len(header) != 3:
            raise TimeoutError("No complete response header")
        expected = 2 if header[1] & 0x80 else header[2] + 2
        response = header + link.read(expected)
        if len(response) != 3 + expected:
            raise TimeoutError("Incomplete response")
        if crc16(response[:-2]) != int.from_bytes(response[-2:], "little"):
            raise ValueError("Response CRC mismatch")
        if header[0] != self.slave:
            raise ValueError("Unexpected slave")
        if header[1] == fc | 0x80:
            raise ValueError(f"Modbus exception {header[2]}")
        if header[1] != fc or header[2] != count * 2:
            raise ValueError("Unexpected function or byte count")
        return list(struct.unpack(">" + "H" * count, response[3:-2]))

    def run(self):
        while True:
            try:
                with serial.Serial(self.port, self.baud, timeout=2, exclusive=True) as link:
                    while True:
                        for block in self.profile["block"]:
                            data = self.exchange(link, block)
                            now = time.monotonic()
                            updates = {}
                            for row in self.profile["register"]:
                                offset = row["address"] - block["start"]
                                if row["space"] != block["space"] or not 0 <= offset < len(data):
                                    continue
                                if row["type"] not in ("s16", "u16", "s32", "u32"):
                                    raise ValueError("Unsupported register encoding")
                                raw = data[offset]
                                bits = 32 if row["type"].endswith("32") else 16
                                if bits == 32:
                                    if offset + 1 >= len(data):
                                        raise ValueError("Truncated 32-bit measurement")
                                    raw = (raw << 16) | data[offset + 1]
                                if row["type"].startswith("s") and raw >= 1 << (bits - 1):
                                    raw -= 1 << bits
                                updates[row["measurement"]] = {
                                    "value": raw * row.get("scale", 1),
                                    "unit": row.get("unit", ""), "raw": raw,
                                    "timestamp": now, "verified": False,
                                }
                            with self.lock:
                                self.values.update(updates)
                                self.successes += 1
                                self.consecutive = 0
                                self.last_success = now
                                self.last_error = ""
                            time.sleep(0.3)
                        time.sleep(5)
            except Exception as error:
                with self.lock:
                    self.failures += 1
                    self.consecutive += 1
                    self.last_error = str(error)
                print(f"Physical acquisition error: {error}", flush=True)
                time.sleep(30)

    def snapshot(self):
        with self.lock:
            now = time.monotonic()
            measurements = {}
            for key, sample in self.values.items():
                sample = copy.deepcopy(sample)
                age = now - sample.pop("timestamp")
                sample.update(valid=age <= 20, stale=age > 20, age_seconds=round(age, 1))
                if age > 20:
                    sample["value"] = None
                measurements[key] = sample
            online = self.last_success is not None and now - self.last_success <= 20
            device = {
                "id": "inverter_1", "label": self.profile["profile"]["display_name"],
                "driver_id": self.profile["profile"]["driver"],
                "manufacturer": self.profile["profile"]["manufacturer"],
                "model": self.profile["profile"]["id"], "serial_number": "",
                "support_level": "experimental", "online": online,
                "data_valid": online, "data_stale": not online,
                "consecutive_poll_failures": self.consecutive,
                "last_successful_poll_seconds_ago": None if self.last_success is None else round(now-self.last_success, 1),
            }
            def reading(key):
                value = measurements.get(key, {})
                return value.get("value") if value.get("valid") else None
            device.update(ac_power_w=reading("ac.power.total"), energy_today_kwh=reading("energy.today"))
            status = {
                "bridge": {"id": "physical-bridge", "name": "Physical read-only bridge",
                    "board_id": "raspberry-pi", "firmware_version": "temporary-live-adapter",
                    "mqtt_connected": False, "modbus_listening": False,
                    "devices_configured": 1, "devices_started": 1, "max_devices": 1,
                    "device_problems": [] if online else [self.last_error or "Stale telemetry"]},
                "device": device, "devices": [device], "measurements": measurements,
                "totals": {"ac_power_w": reading("ac.power.total"), "energy_today_kwh": reading("energy.today"),
                    "devices_polled": 1, "devices_answering": int(online)},
                "source": "physical_modbus_rtu", "read_only": True,
                "status_text": "Physical Modbus reads; experimental mapping; control disabled",
            }
            diagnostics = {"uptime_seconds": int(now-self.started), "poll_success_total": self.successes,
                "poll_failure_total": self.failures, "consecutive_poll_failures": self.consecutive,
                "last_error": self.last_error, "board": "raspberry-pi", "source": "physical_modbus_rtu"}
            return status, diagnostics


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--profile", required=True)
    parser.add_argument("--serial", required=True)
    parser.add_argument("--baud", type=int, default=9600)
    parser.add_argument("--slave", type=int, default=1)
    parser.add_argument("--port", type=int, default=8080)
    args = parser.parse_args()
    profile = tomllib.loads(pathlib.Path(args.profile).read_text(encoding="utf-8"))
    acquisition = Acquisition(profile, args.serial, args.baud, args.slave)
    class Handler(web_ui.EmsWebHandler):
        def serve_status(self):
            self.send_json(acquisition.snapshot()[0])

        def get_standard_measurements(self):
            return acquisition.snapshot()[0]["measurements"]

        def serve_devices(self):
            self.send_json({"devices": ["inverter_1"]})

        def serve_device_detail(self, url):
            if "/inverter_1" not in url:
                self.send_error(404)
            elif url.endswith("/measurements"):
                self.send_json({"measurements": self.get_standard_measurements()})
            elif url.endswith("/capabilities"):
                self.send_json({"readable": True, "writable": False, "controls": []})
            else:
                self.send_json(acquisition.snapshot()[0]["device"])

        def serve_diagnostics(self):
            self.send_json(acquisition.snapshot()[1])

        def serve_logs(self):
            diag = acquisition.snapshot()[1]
            self.send_json({"level": "info", "dropped": 0, "lines": [json.dumps(diag)]})

        def serve_drivers(self):
            self.send_json({"drivers": [{"id": profile["profile"]["driver"],
                "name": profile["profile"]["display_name"], "support_level": "experimental"}]})

        def serve_wifi_scan(self):
            self.send_json({"networks": []})

        def serve_config(self):
            self.send_json({"version": 2, "bridge_name": "Physical read-only bridge",
                "driver": {"id": profile["profile"]["driver"], "label": profile["profile"]["display_name"],
                    "options": {"profile": profile["profile"]["id"], "unit_id": str(args.slave)}},
                "serial": {"port": args.serial, "baud_rate": args.baud, "parity": "none", "data_bits": 8, "stop_bits": 1},
                "mqtt": {"enabled": False}, "modbus": {"enabled": False},
                "security": {"read_only_mode": True}, "relays": {"enabled": False},
                "polling": {"interval_seconds": 5}, "logging": {"level": "info"}, "additional_devices": []})

        def serve_sse(self):
            self.send_response(200)
            self.send_header("Content-Type", "text/event-stream")
            self.send_header("Cache-Control", "no-cache")
            self.end_headers()
            try:
                while True:
                    payload = json.dumps(acquisition.snapshot()[0])
                    self.wfile.write(f"event: state\ndata: {payload}\n\n".encode())
                    self.wfile.flush()
                    time.sleep(3)
            except (BrokenPipeError, ConnectionResetError):
                pass

        def do_POST(self):
            self.send_json({"error": "Configuration changes are disabled during the physical read-only trial"}, 409)

        do_PATCH = do_POST

    threading.Thread(target=acquisition.run, daemon=True).start()
    with web_ui.ThreadedHTTPServer(("0.0.0.0", args.port), Handler) as server:
        server.serve_forever()


if __name__ == "__main__":
    main()
