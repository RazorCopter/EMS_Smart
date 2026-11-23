"""Capture ESP diagnostics and Zigbee power in one CSV without changing inverter mode."""
import argparse
import base64
from bisect import bisect_right
import csv
from datetime import datetime, timezone
import json
import math
from os import environ
from pathlib import Path
import time
from urllib.request import Request, urlopen

import paho.mqtt.client as mqtt


TOPICS = {"zigbee2mqtt/POW-Generale": "generale", "zigbee2mqtt/POW-Prese": "prese"}
EVENTS = {"TRAINER_DEBUG", "TRAINER_POWER", "EnergyState", "LIVE_READBACK"}
MAX_AGE = 15


def now():
    return datetime.now(timezone.utc)


def read_logs(host, auth):
    request = Request(f"http://{host}/api/v1/logs", headers={"Authorization": auth})
    with urlopen(request, timeout=5) as response:
        for line in response.read().decode().splitlines():
            prefix, sep, clock = line.partition(" {")
            if not sep:
                continue
            seq, uptime, event, *parts = prefix.split()
            if event not in EVENTS and not event.startswith(("LIVE_", "TRAINER")):
                continue
            timing = json.loads("{" + clock)
            stamp = datetime.fromisoformat(timing["timestamp"]) if timing.get("time_valid") else None
            values = dict(part.split("=", 1) for part in parts if "=" in part)
            yield int(seq), stamp, {"esp_uptime_ms": uptime, "event": event,
                                    "message": " ".join(parts), **values}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--seconds", type=int, default=420)
    parser.add_argument("--host", default="10.0.0.110")
    parser.add_argument("--env", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    password = (Path(environ["LOCALAPPDATA"]) / "AforeEMS/v51-prototype/management-password.txt")
    auth = "Basic " + base64.b64encode(("admin:" + password.read_text().strip()).encode()).decode()
    settings = dict(line.strip().split("=", 1) for line in args.env.read_text().splitlines()
                    if "=" in line and not line.lstrip().startswith("#"))
    samples = {name: [] for name in TOPICS.values()}
    rows = {}
    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
    if settings.get("MQTT_USER"):
        client.username_pw_set(settings["MQTT_USER"], settings.get("MQTT_PASSWORD"))
    if settings.get("MQTT_TLS", "").lower() in {"1", "true", "yes"}:
        client.tls_set()

    def connected(conn, userdata, flags, reason, properties):
        if reason.is_failure:
            print(f"MQTT: {reason}", flush=True)
        else:
            conn.subscribe([(topic, 0) for topic in TOPICS])

    def received(conn, userdata, msg):
        if msg.retain:
            return
        try:
            watts = json.loads(msg.payload).get("power")
        except (ValueError, AttributeError):
            return
        if type(watts) in (float, int) and math.isfinite(watts) and watts >= 0:
            samples[TOPICS[msg.topic]].append((now(), watts))

    client.on_connect = connected
    client.on_message = received
    client.connect(settings["MQTT_HOST"], int(settings.get("MQTT_PORT", "1883")))
    client.loop_start()
    try:
        start = now()
        deadline = time.monotonic() + args.seconds
        while True:
            try:
                for seq, stamp, row in read_logs(args.host, auth):
                    if stamp and stamp >= start.replace(microsecond=0):
                        rows[(stamp, seq)] = row
            except (OSError, ValueError) as exc:
                print(f"ESP: {exc}", flush=True)
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                break
            time.sleep(min(2, remaining))
    finally:
        client.disconnect()
        client.loop_stop()

    stamps = {name: [stamp for stamp, _ in values] for name, values in samples.items()}
    export = []
    for (stamp, seq), row in sorted(rows.items()):
        row = {"sample_utc": stamp.isoformat(), "esp_seq": seq, **row}
        for name, values in samples.items():
            idx = bisect_right(stamps[name], stamp) - 1
            if idx >= 0:
                updated, watts = values[idx]
                age = (stamp - updated).total_seconds()
                if age <= MAX_AGE:
                    row.update({f"{name}_w": watts, f"{name}_received_utc": updated.isoformat(),
                                f"{name}_age_s": round(age, 3)})
        if "generale_w" in row and "prese_w" in row:
            row["generale_minus_prese_w"] = row["generale_w"] - row["prese_w"]
        export.append(row)
    columns = dict.fromkeys(["sample_utc", "esp_seq", "event", "generale_w", "generale_received_utc",
                             "generale_age_s", "prese_w", "prese_received_utc", "prese_age_s",
                             "generale_minus_prese_w"])
    for row in export:
        columns.update(dict.fromkeys(row))
    with args.output.open("x", encoding="utf-8-sig", newline="") as output:
        writer = csv.DictWriter(output, fieldnames=list(columns))
        writer.writeheader()
        writer.writerows(export)
    print(f"ESP: {len(export)} | Generale: {len(samples['generale'])} | Prese: {len(samples['prese'])}",
          flush=True)
    return 0 if any("generale_minus_prese_w" in row for row in export) else 1


if __name__ == "__main__":
    raise SystemExit(main())
