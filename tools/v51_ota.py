# SPDX-License-Identifier: MIT
"""Send a signed v5.1 application image on the local network."""
import argparse
import base64
import getpass
import hashlib
import json
import os
from pathlib import Path
import time
import urllib.error
import urllib.request

from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import ec, utils


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("host", help="ESP32 IP or afore-ems.local")
    parser.add_argument("image", type=Path, help="Application firmware.bin from PlatformIO")
    parser.add_argument("--key", type=Path, default=Path(os.environ["LOCALAPPDATA"])
                        / "AforeEMS" / "v51-prototype" / "ota-signing-key.pem")
    parser.add_argument("--expect-rollback", action="store_true", help="Verify a deliberate failed boot")
    args = parser.parse_args()
    image = args.image.read_bytes()
    if not image or image[0] != 0xE9 or len(image) > 0x600000:
        parser.error("expected an ESP32 application image that fits a 6 MiB slot")
    key = serialization.load_pem_private_key(args.key.read_bytes(), password=None)
    digest = hashlib.sha256(image).digest()
    signature = key.sign(digest, ec.ECDSA(utils.Prehashed(hashes.SHA256())))
    password = os.environ.get("AFORE_MANAGEMENT_PASSWORD") or getpass.getpass("Management password: ")
    authorization = "Basic " + base64.b64encode(f"admin:{password}".encode()).decode()
    headers = {"Authorization": authorization}
    base = f"http://{args.host}/api/v1/"
    opener = urllib.request.build_opener(urllib.request.ProxyHandler({}))

    def read_health():
        with opener.open(urllib.request.Request(base + "health", headers=headers), timeout=5) as reply:
            return json.load(reply)

    before = read_health()
    upload_headers = {
        **headers,
        "Content-Type": "application/octet-stream",
        "X-Firmware-SHA256": digest.hex(),
        "X-Firmware-Signature": signature.hex(),
    }
    with opener.open(urllib.request.Request(base + "ota", image, upload_headers), timeout=130) as reply:
        print(json.load(reply), "SHA256=" + digest.hex(), flush=True)

    deadline = time.monotonic() + 105
    saw_pending = False
    while time.monotonic() < deadline:
        time.sleep(1)
        try:
            status = read_health()
        except (OSError, urllib.error.URLError):
            continue
        if status["ota_slot"] != before["ota_slot"]:
            saw_pending |= status["ota_state"] == "pending_verify"
            if not args.expect_rollback and status["ota_state"] == "valid":
                print(json.dumps(status, indent=2))
                return
        if (args.expect_rollback and saw_pending and status["ota_slot"] == before["ota_slot"]
                and status["firmware"] == before["firmware"]):
            print("Rollback verified", json.dumps(status))
            return
    raise SystemExit("Timed out before the requested OTA outcome was observed")


if __name__ == "__main__":
    main()
