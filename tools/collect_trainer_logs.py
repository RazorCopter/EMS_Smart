# SPDX-License-Identifier: MIT
"""Collect the ESP diagnostic ring to a bounded, external log directory."""

import argparse
import json
from pathlib import Path
import time
from urllib.request import Request, urlopen

from deploy_and_verify_m9b import HOST, get_auth_header


def collect(output: Path, host: str, seconds: int, segment_bytes: int, segments: int) -> None:
    output.mkdir()
    request = Request(f"http://{host}/api/v1/logs", headers={"Authorization": get_auth_header()})
    health = Request(f"http://{host}/api/v1/health", headers=request.headers)
    deadline = time.monotonic() + seconds
    last_seq = 0
    last_uptime = None
    segment = 0
    used = 0
    stream = (output / f"trainer-{segment:02}.log").open("xb")

    def write(line: str) -> None:
        nonlocal stream, segment, used
        data = (line + "\n").encode()
        if used + len(data) > segment_bytes:
            stream.close()
            segment += 1
            expired = segment - segments
            if expired >= 0:
                (output / f"trainer-{expired:02}.log").unlink()
            stream = (output / f"trainer-{segment:02}.log").open("xb")
            used = 0
        stream.write(data)
        used += len(data)

    try:
        while True:
            try:
                with urlopen(health, timeout=5) as response:
                    uptime = json.load(response)["uptime_ms"]
                with urlopen(request, timeout=5) as response:
                    lines = response.read().decode().splitlines()
                if last_uptime is not None and uptime < last_uptime:
                    write("COLLECTOR ESP_RESTART")
                    last_seq = 0
                last_uptime = uptime
                for line in lines:
                    seq, _, message = line.split(" ", 2)
                    seq = int(seq)
                    if seq <= last_seq:
                        continue
                    if last_seq and seq > last_seq + 1:
                        write(f"COLLECTOR GAP first={last_seq + 1} last={seq - 1}")
                    last_seq = seq
                    if message.startswith(("TRAINER", "LIVE_", "EnergyState")):
                        write(line)
            except (OSError, ValueError, KeyError) as exc:
                write(f"COLLECTOR POLL_ERROR {type(exc).__name__}: {str(exc)[:240]}")
            stream.flush()
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                break
            time.sleep(min(2, remaining))
    finally:
        stream.close()


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path, help="New directory for this session")
    parser.add_argument("--host", default=HOST)
    parser.add_argument("--seconds", type=int, default=86400)
    parser.add_argument("--segment-mib", type=int, default=32)
    parser.add_argument("--segments", type=int, default=4)
    args = parser.parse_args()
    if not (1 <= args.seconds <= 172800 and 1 <= args.segment_mib <= 64 and 1 <= args.segments <= 8):
        parser.error("Seconds 1–172800, segment MiB 1–64, segments 1–8")
    collect(args.output, args.host, args.seconds, args.segment_mib * 1024**2, args.segments)


if __name__ == "__main__":
    main()
