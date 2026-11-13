#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""
Web UI Bridge and Configurator for Raspberry Pi (Linux Host)

Serves the full Heliograph embedded web application (Setup, Live, Inverters,
Integrations, Health, Bridge) on port 8080.
Allows complete setup via Graphical Web Interface on the Raspberry Pi:
- Network / Host settings
- Serial Port & Inverter setup
- MQTT & Home Assistant integration
- Live telemetry monitoring & zero-write shadow EMS status
"""

import argparse
import json
import os
import pathlib
import socketserver
import sys
import threading
from http.server import SimpleHTTPRequestHandler

REPO_ROOT = pathlib.Path(__file__).resolve().parent.parent
CONFIG_PATH = pathlib.Path("/etc/afore-ems/config.json")
LOCAL_CONFIG = REPO_ROOT / "deploy" / "config.json"


def get_active_config_path() -> pathlib.Path:
    if CONFIG_PATH.parent.exists() and os.access(CONFIG_PATH.parent, os.W_OK):
        return CONFIG_PATH
    return LOCAL_CONFIG


class EmsWebHandler(SimpleHTTPRequestHandler):
    def end_headers(self):
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS")
        self.send_header("Access-Control-Allow-Headers", "Content-Type")
        super().end_headers()

    def do_OPTIONS(self):
        self.send_response(204)
        self.end_headers()

    def do_GET(self):
        url = self.path.split("?")[0]
        if url in ("/", "/index.html"):
            self.serve_asset("index_html.h", "text/html; charset=utf-8")
        elif url in ("/setup", "/setup.html"):
            self.serve_asset("setup_html.h", "text/html; charset=utf-8")
        elif url == "/api/v1/config":
            self.serve_config()
        elif url == "/api/v1/status":
            self.serve_status()
        elif url == "/api/v1/events":
            self.serve_sse()
        else:
            self.send_error(404, "Not Found")

    def do_POST(self):
        url = self.path.split("?")[0]
        content_len = int(self.headers.get("Content-Length", 0))
        body = self.rfile.read(content_len).decode("utf-8") if content_len > 0 else "{}"

        if url in ("/api/v1/config", "/api/v1/setup"):
            try:
                data = json.loads(body)
                cfg_path = get_active_config_path()
                cfg_path.parent.mkdir(parents=True, exist_ok=True)
                with open(cfg_path, "w", encoding="utf-8") as f:
                    json.dump(data, f, indent=2)
                self.send_response(200)
                self.send_header("Content-Type", "application/json")
                self.end_headers()
                self.wfile.write(b'{"status":"saved","message":"Configuration updated successfully"}')
            except Exception as e:
                self.send_response(400)
                self.send_header("Content-Type", "application/json")
                self.end_headers()
                self.wfile.write(json.dumps({"status":"error", "error": str(e)}).encode())
        else:
            self.send_error(404, "Not Found")

    def serve_asset(self, header_filename: str, content_type: str):
        path = REPO_ROOT / "src" / "web" / "assets" / header_filename
        if not path.exists():
            self.send_error(500, f"Asset header not found: {header_filename}")
            return
        content = path.read_text(encoding="utf-8")
        # Extract the raw HTML literal between R"rawliteral( and )rawliteral";
        start = content.find('R"rawliteral(')
        end = content.rfind(')rawliteral";')
        if start != -1 and end != -1:
            raw_html = content[start + len('R"rawliteral('):end]
        else:
            raw_html = content

        payload = raw_html.encode("utf-8")
        self.send_response(200)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(payload)))
        self.end_headers()
        self.wfile.write(payload)

    def serve_config(self):
        cfg_path = get_active_config_path()
        if cfg_path.exists():
            try:
                with open(cfg_path, "r", encoding="utf-8") as f:
                    data = json.load(f)
            except Exception:
                data = {}
        else:
            data = {
                "serial_port": "/dev/ttyUSB0",
                "baud_rate": 9600,
                "inverter_address": 1,
                "mqtt": {"enabled": False, "host": "127.0.0.1", "port": 1883}
            }
        payload = json.dumps(data).encode("utf-8")
        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(payload)))
        self.end_headers()
        self.wfile.write(payload)

    def serve_status(self):
        status = {
            "version": "1.0.0",
            "mode": "shadow_readonly",
            "daemon_running": True,
            "system": "Linux / Raspberry Pi"
        }
        payload = json.dumps(status).encode("utf-8")
        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(payload)))
        self.end_headers()
        self.wfile.write(payload)

    def serve_sse(self):
        self.send_response(200)
        self.send_header("Content-Type", "text/event-stream")
        self.send_header("Cache-Control", "no-cache")
        self.send_header("Connection", "keep-alive")
        self.end_headers()
        self.wfile.write(b"data: {\"status\":\"connected\"}\n\n")


def run(host="0.0.0.0", port=8080):
    server = socketserver.TCPServer((host, port), EmsWebHandler)
    server.allow_reuse_address = True
    print(f"===========================================================")
    print(f"  Afore Autonomous EMS - Web UI Ready                      ")
    print(f"  Access the configuration dashboard from any browser:    ")
    print(f"  http://<RASPBERRY-IP>:{port}/                           ")
    print(f"  Setup wizard: http://<RASPBERRY-IP>:{port}/setup        ")
    print(f"===========================================================")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        server.shutdown()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Raspberry Pi EMS Web Interface")
    parser.add_argument("--port", type=int, default=8080, help="HTTP port (default: 8080)")
    parser.add_argument("--host", default="0.0.0.0", help="HTTP bind host (default: 0.0.0.0)")
    args = parser.parse_args()
    run(args.host, args.port)
