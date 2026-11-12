#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Shoots desktop and mobile screenshots of the Setup / Provisioning portal and Dashboard tabs."""

import http.server
import json
import pathlib
import socketserver
import subprocess
import sys
import threading

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import build_web

ROOT = pathlib.Path(__file__).resolve().parent.parent
OUT = ROOT / "docs" / "images"

class SetupHandler(http.server.BaseHTTPRequestHandler):
    def do_GET(self):
        self.send_response(200)
        self.send_header("Content-Type", "text/html; charset=utf-8")
        self.end_headers()
        self.wfile.write(build_web.served_page("setup_html.h").encode("utf-8"))

    def do_POST(self):
        # mock /api/setup/apply
        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.end_headers()
        self.wfile.write(b'{"status":"ok","rebooting":true}')

    def log_message(self, format, *args):
        pass

def shoot(chrome, url, target, width, height):
    subprocess.run(
        [
            chrome,
            "--headless",
            "--disable-gpu",
            "--no-sandbox",
            "--hide-scrollbars",
            f"--window-size={width},{height}",
            "--virtual-time-budget=4000",
            f"--screenshot={target}",
            url,
        ],
        capture_output=True,
        timeout=120,
        check=False,
    )
    return target.stat().st_size if target.exists() else 0

def main():
    chrome = build_web.find_chrome()
    if not chrome:
        print("FAIL: No Chrome/Edge found")
        return 1

    OUT.mkdir(parents=True, exist_ok=True)
    with socketserver.TCPServer(("127.0.0.1", 0), SetupHandler) as server:
        port = server.server_address[1]
        th = threading.Thread(target=server.serve_forever)
        th.daemon = True
        th.start()
        try:
            url = f"http://127.0.0.1:{port}/"
            # 1. Setup Desktop
            s_desk = shoot(chrome, url, OUT / "setup-desktop.png", 1280, 800)
            print(f"setup-desktop.png: {s_desk // 1024} KB")
            # 2. Setup Mobile
            s_mob = shoot(chrome, url, OUT / "setup-mobile.png", 390, 844)
            print(f"setup-mobile.png:  {s_mob // 1024} KB")
        finally:
            server.shutdown()
            server.server_close()

    print("Screenshots generation successful.")
    return 0

if __name__ == "__main__":
    sys.exit(main())
