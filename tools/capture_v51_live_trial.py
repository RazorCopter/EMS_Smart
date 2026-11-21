# SPDX-License-Identifier: MIT
"""Record an operator-started field trial without sending control requests."""

import argparse
import base64
import json
import time
from datetime import datetime, timezone
from pathlib import Path
from urllib.request import Request, urlopen


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path)
    parser.add_argument("--host", default="10.0.0.110")
    parser.add_argument("--password-file", type=Path, required=True)
    parser.add_argument("--seconds", type=int, default=540)
    args = parser.parse_args()

    password = args.password_file.read_text(encoding="utf-8").strip()
    auth = "Basic " + base64.b64encode(f"admin:{password}".encode()).decode()
    routes = ("manual", "telemetry", "ems/decision")
    deadline = time.monotonic() + args.seconds
    interval = 2
    with args.output.open("x", encoding="utf-8") as output:
        while time.monotonic() < deadline:
            sample = {"captured_at": datetime.now(timezone.utc).isoformat()}
            for route in routes:
                request = Request(
                    f"http://{args.host}/api/v1/{route}",
                    headers={"Authorization": auth},
                )
                try:
                    with urlopen(request, timeout=2) as response:
                        sample[route] = json.load(response)
                except (OSError, ValueError) as error:
                    sample[route] = {"error": str(error)}
            output.write(json.dumps(sample) + "\n")
            output.flush()
            configured = sample["manual"].get("live_trainer_sample_s")
            if isinstance(configured, int) and 1 <= configured <= 30:
                interval = configured
            time.sleep(interval)


if __name__ == "__main__":
    main()
