# SPDX-License-Identifier: MIT
"""
Mission 9B: OTA upload, bootstrap monitoring, probation verification and disposition live check.
"""
import base64
import hashlib
import json
import os
import pathlib
import sys
import time
import urllib.error
import urllib.request
from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import ec, utils

BASE_DIR = pathlib.Path(__file__).resolve().parent.parent
HOST = "10.0.0.110"
FIRMWARE_BIN = BASE_DIR / ".pio" / "build" / "waveshare-1ch-v51" / "firmware.bin"
KEY_PATH = pathlib.Path(os.environ["LOCALAPPDATA"]) / "AforeEMS" / "v51-prototype" / "ota-signing-key.pem"
PASS_PATH = pathlib.Path(os.environ["LOCALAPPDATA"]) / "AforeEMS" / "v51-prototype" / "management-password.txt"

def get_auth_header():
    pw = PASS_PATH.read_text(encoding="utf-8").strip()
    return "Basic " + base64.b64encode(f"admin:{pw}".encode()).decode()

def http_get(endpoint):
    req = urllib.request.Request(f"http://{HOST}/api/v1/{endpoint}", headers={"Authorization": get_auth_header()})
    with urllib.request.urlopen(req, timeout=5) as resp:
        return json.load(resp)

def main():
    print(f"--- Starting Mission 9B OTA Deployment to {HOST} ---")
    image = FIRMWARE_BIN.read_bytes()
    print(f"Firmware image size: {len(image)} bytes")
    digest = hashlib.sha256(image).digest()
    digest_hex = digest.hex()
    print(f"SHA-256: {digest_hex}")

    key = serialization.load_pem_private_key(KEY_PATH.read_bytes(), password=None)
    sig = key.sign(digest, ec.ECDSA(utils.Prehashed(hashes.SHA256())))
    print(f"Signature generated ({len(sig)} bytes DER)")

    before_health = http_get("health")
    before_sys = http_get("system")
    print(f"Pre-OTA health: slot={before_health.get('ota_slot')}, state={before_health.get('ota_state')}, fw={before_health.get('firmware')}")

    upload_headers = {
        "Authorization": get_auth_header(),
        "Content-Type": "application/octet-stream",
        "X-Firmware-SHA256": digest_hex,
        "X-Firmware-Signature": sig.hex(),
    }

    print("Uploading firmware via /api/v1/ota ...")
    req = urllib.request.Request(f"http://{HOST}/api/v1/ota", data=image, headers=upload_headers)
    with urllib.request.urlopen(req, timeout=140) as resp:
        res = json.load(resp)
        print("Upload response:", res)

    print("Firmware uploaded successfully. Rebooting to new slot...")
    t_start = time.time()
    saw_pending = False
    new_slot_booted = False
    post_boot_samples = []

    # Monitor bootstrap
    while time.time() - t_start < 180:
        time.sleep(0.5)
        try:
            h = http_get("health")
            elapsed = time.time() - t_start
            slot = h.get("ota_slot")
            state = h.get("ota_state")
            min_app_stack = h.get("min_application_stack_free_b", "N/A")
            min_stack = h.get("min_task_stack_free_b")
            min_task = h.get("min_task_stack_name")
            print(f"[{elapsed:5.1f}s] slot={slot} state={state} min_app_stack={min_app_stack} min_stack={min_stack}({min_task}) uptime={h.get('uptime_ms')}ms")

            if slot != before_health.get("ota_slot"):
                new_slot_booted = True
                if state == "pending_verify":
                    saw_pending = True
                if state == "valid":
                    print("--> Firmware reached VALID state!")
                    post_boot_samples.append(h)
                    break
        except (OSError, urllib.error.URLError) as e:
            elapsed = time.time() - t_start
            # Device offline/rebooting

    if not saw_pending:
        print("WARNING: Did not observe pending_verify state during polling (might have confirmed or transitioned quickly)")
    else:
        print("SUCCESS: Observed pending_verify state during probation window")

    if not new_slot_booted or not post_boot_samples:
        sys.exit("FAILED: Firmware did not boot in the new OTA slot and reach VALID state within 180s!")

    print("\n--- Verifying Detailed Diagnostics & Shadow Disposition ---")
    sys_info = http_get("system")
    health_info = http_get("health")

    shadow_dec = None
    dec_deadline = time.time() + 20.0
    while time.time() < dec_deadline:
        candidate = http_get("ems/decision")
        if isinstance(candidate, dict) and candidate.get("source") == "MEASURED":
            shadow_dec = candidate
            break
        time.sleep(1.0)

    print("\nPost-OTA Health (includes tasks array, min_application_stack, and mqtt_reconnect_count):")
    print(json.dumps(health_info, indent=2))

    print("\nShadow Decision:")
    print(json.dumps(shadow_dec, indent=2))

    errors = []
    if health_info.get("firmware") != "v5.1-console-8-m9b":
        errors.append(f"Unexpected firmware build: {health_info.get('firmware')}")
    if health_info.get("ota_slot") == before_health.get("ota_slot"):
        errors.append(f"OTA slot did not switch from {before_health.get('ota_slot')}")
    if health_info.get("ota_state") != "valid":
        errors.append(f"OTA state not valid: {health_info.get('ota_state')}")
    if health_info.get("reset_reason") != 3:
        errors.append(f"Unexpected reset_reason: {health_info.get('reset_reason')}")
    if not isinstance(health_info.get("tasks"), list) or len(health_info["tasks"]) == 0:
        errors.append("Missing or empty tasks diagnostics list")
    if (health_info.get("min_application_stack_free_b") or 0) < 1024:
        errors.append(f"Application stack < 1024 B: {health_info.get('min_application_stack_free_b')} ({health_info.get('min_application_stack_name')})")
    if "mqtt_reconnect_count" not in health_info:
        errors.append("Missing mqtt_reconnect_count")
    if health_info.get("aux1") != "OFF" or health_info.get("executed") is not False or health_info.get("modbus_write_count") != 0:
        errors.append("SHADOW safety invariant violation in health")
    if not isinstance(shadow_dec, dict):
        errors.append("No MEASURED shadow decision received within 20s after VALID")
    else:
        disp = shadow_dec.get("disposition")
        if not disp or disp in ("NONE", "NOT_EVALUATED"):
            errors.append(f"Invalid shadow decision disposition: {disp}")
        if shadow_dec.get("executed") is not False or shadow_dec.get("modbus_write_count") != 0 or shadow_dec.get("aux1") != "OFF":
            errors.append("SHADOW safety invariant violation in shadow decision")

    # Save evidence files
    out_dir = BASE_DIR / "docs" / "v5.1" / "evidence" / "M9B"
    out_dir.mkdir(parents=True, exist_ok=True)
    with open(out_dir / "post-ota-health.json", "w", encoding="utf-8") as f:
        json.dump(health_info, f, indent=2)
    with open(out_dir / "post-ota-system.json", "w", encoding="utf-8") as f:
        json.dump(sys_info, f, indent=2)
    with open(out_dir / "post-ota-shadow-decision.json", "w", encoding="utf-8") as f:
        json.dump(shadow_dec, f, indent=2)
    with open(out_dir / "firmware-sha256.txt", "w", encoding="utf-8") as f:
        f.write(digest_hex + "\n")

    if errors:
        for err in errors:
            print("FAIL:", err)
        sys.exit(1)

    print("\nVerification completed successfully.")

if __name__ == "__main__":
    main()
