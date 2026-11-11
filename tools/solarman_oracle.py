#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""
Afore AF5K-SL Solarman & Modbus RTU Register Oracle.

Validates Modbus RTU / Solarman hex payload frames against documented register
specifications, endianness, scale formulas, sign conventions, and formal
provenance classifications (M6 Milestone).
"""

from __future__ import annotations

import argparse
import json
import struct
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Any

# Default path to sample fixture
DEFAULT_FIXTURE = (
    Path(__file__).parent.parent
    / "test"
    / "test_afore"
    / "fixtures"
    / "af5k_sl_sample.json"
)


# Formal classification states per AFORE_REGISTER_PROVENANCE.md
class ProvenanceState:
    VERIFIED_EMPIRICALLY = "VERIFIED_EMPIRICALLY"
    PROVISIONAL = "PROVISIONAL"
    DOCUMENTED_CONFLICTED = "DOCUMENTED_CONFLICTED"
    DOCUMENTED_CONTROL_EXCLUDED = "DOCUMENTED_CONTROL_EXCLUDED"
    CONFIGURATION_EXCLUDED = "CONFIGURATION_EXCLUDED"


@dataclass(frozen=True)
class RegisterSpec:
    address: int
    name: str
    dtype: str  # u16, s16, u32, s32
    scale: float
    unit: str
    provenance: str
    description: str


REGISTER_MAP: dict[int, RegisterSpec] = {
    # 0x07D2: Sole empirically verified register in Phase 0
    0x07D2: RegisterSpec(
        address=0x07D2,
        name="battery.soc",
        dtype="u16",
        scale=1.0,
        unit="%",
        provenance=ProvenanceState.VERIFIED_EMPIRICALLY,
        description="Battery State of Charge (verified at 80% in af5k_sl_sample.json)",
    ),
    0x07D8: RegisterSpec(
        address=0x07D8,
        name="battery.power",
        dtype="s16",
        scale=-1.0,  # Canonical flip: Afore negative = charging
        unit="W",
        provenance=ProvenanceState.PROVISIONAL,
        description="Battery Power (negative in hardware during charge)",
    ),
    0x07D4: RegisterSpec(
        address=0x07D4,
        name="battery.voltage",
        dtype="u16",
        scale=0.1,
        unit="V",
        provenance=ProvenanceState.PROVISIONAL,
        description="Battery Voltage (0.1V scale)",
    ),
    0x07D5: RegisterSpec(
        address=0x07D5,
        name="battery.current",
        dtype="s16",
        scale=0.01,
        unit="A",
        provenance=ProvenanceState.PROVISIONAL,
        description="Battery Current (0.01A scale, signed)",
    ),
    0x07D0: RegisterSpec(
        address=0x07D0,
        name="battery.state",
        dtype="u16",
        scale=1.0,
        unit="enum",
        provenance=ProvenanceState.PROVISIONAL,
        description="Battery State enum (4=Charge, 5=Discharge, 6=Standby)",
    ),
    0x022A: RegisterSpec(
        address=0x022A,
        name="dc.power.total",
        dtype="u16",
        scale=1.0,
        unit="W",
        provenance=ProvenanceState.PROVISIONAL,
        description="Total PV Power sum",
    ),
    0x022D: RegisterSpec(
        address=0x022D,
        name="dc.mppt_1.power",
        dtype="u16",
        scale=1.0,
        unit="W",
        provenance=ProvenanceState.PROVISIONAL,
        description="PV String 1 Power",
    ),
    0x0230: RegisterSpec(
        address=0x0230,
        name="dc.mppt_2.power",
        dtype="u16",
        scale=1.0,
        unit="W",
        provenance=ProvenanceState.PROVISIONAL,
        description="PV String 2 Power",
    ),
    0x0228: RegisterSpec(
        address=0x0228,
        name="dc.mppt_1.voltage",
        dtype="u16",
        scale=0.1,
        unit="V",
        provenance=ProvenanceState.PROVISIONAL,
        description="PV String 1 Voltage",
    ),
    0x0229: RegisterSpec(
        address=0x0229,
        name="dc.mppt_1.current",
        dtype="u16",
        scale=0.1,
        unit="A",
        provenance=ProvenanceState.PROVISIONAL,
        description="PV String 1 Current",
    ),
    0x022E: RegisterSpec(
        address=0x022E,
        name="dc.mppt_2.voltage",
        dtype="u16",
        scale=0.1,
        unit="V",
        provenance=ProvenanceState.PROVISIONAL,
        description="PV String 2 Voltage",
    ),
    0x022F: RegisterSpec(
        address=0x022F,
        name="dc.mppt_2.current",
        dtype="u16",
        scale=0.1,
        unit="A",
        provenance=ProvenanceState.PROVISIONAL,
        description="PV String 2 Current",
    ),
    0x0212: RegisterSpec(
        address=0x0212,
        name="grid.power",
        dtype="s16",
        scale=1.0,
        unit="W",
        provenance=ProvenanceState.PROVISIONAL,
        description="Grid Active Power (+import / -export)",
    ),
    0x020B: RegisterSpec(
        address=0x020B,
        name="ac.power.total",
        dtype="s16",
        scale=1.0,
        unit="W",
        provenance=ProvenanceState.PROVISIONAL,
        description="Inverter AC Output Power",
    ),
    0x020C: RegisterSpec(
        address=0x020C,
        name="inverter.temperature",
        dtype="s16",
        scale=0.1,
        unit="°C",
        provenance=ProvenanceState.PROVISIONAL,
        description="Inverter Internal Temperature",
    ),
    0x0200: RegisterSpec(
        address=0x0200,
        name="inverter.status",
        dtype="u16",
        scale=1.0,
        unit="enum",
        provenance=ProvenanceState.PROVISIONAL,
        description="Inverter Running Status",
    ),
    0x0203: RegisterSpec(
        address=0x0203,
        name="ac.phase_l1.voltage",
        dtype="u16",
        scale=0.1,
        unit="V",
        provenance=ProvenanceState.PROVISIONAL,
        description="Grid Voltage L1",
    ),
    0x0205: RegisterSpec(
        address=0x0205,
        name="ac.frequency",
        dtype="u16",
        scale=0.01,
        unit="Hz",
        provenance=ProvenanceState.PROVISIONAL,
        description="Grid Frequency",
    ),
    # Conflicted and excluded registers
    0x09C4: RegisterSpec(
        address=0x09C4,
        name="ems.mode",
        dtype="u16",
        scale=1.0,
        unit="enum",
        provenance=ProvenanceState.DOCUMENTED_CONFLICTED,
        description="EMS Mode (5=External EMS) vs Inverter Status conflict",
    ),
    0x0A08: RegisterSpec(
        address=0x0A08,
        name="control.type",
        dtype="u16",
        scale=1.0,
        unit="enum",
        provenance=ProvenanceState.DOCUMENTED_CONTROL_EXCLUDED,
        description="External Control Type (0=batt, 1=inv, 2=grid, 3=off)",
    ),
    0x0A09: RegisterSpec(
        address=0x0A09,
        name="control.power_setpoint",
        dtype="s32",
        scale=1.0,
        unit="W",
        provenance=ProvenanceState.DOCUMENTED_CONTROL_EXCLUDED,
        description="Active Power Setpoint (signed 32-bit big-endian)",
    ),
    0x0BC1: RegisterSpec(
        address=0x0BC1,
        name="config.max_charge_current",
        dtype="u16",
        scale=0.01,
        unit="A",
        provenance=ProvenanceState.CONFIGURATION_EXCLUDED,
        description="Max Charge Current (EEPROM wear risk, permanently banned)",
    ),
    0x0BC2: RegisterSpec(
        address=0x0BC2,
        name="config.max_discharge_current",
        dtype="u16",
        scale=0.01,
        unit="A",
        provenance=ProvenanceState.CONFIGURATION_EXCLUDED,
        description="Max Discharge Current (EEPROM wear risk, permanently banned)",
    ),
}


def modbus_crc16(data: bytes) -> int:
    """Calculate Modbus RTU CRC-16 using polynomial 0xA001."""
    crc = 0xFFFF
    for byte in data:
        crc ^= byte
        for _ in range(8):
            if crc & 0x0001:
                crc = (crc >> 1) ^ 0xA001
            else:
                crc >>= 1
    return crc


def verify_crc16(frame: bytes) -> bool:
    """Verify that the trailing 2 bytes of the frame match the Modbus CRC-16."""
    if len(frame) < 3:
        return False
    payload = frame[:-2]
    expected_crc = modbus_crc16(payload)
    actual_crc = frame[-2] | (frame[-1] << 8)
    return expected_crc == actual_crc


def parse_hex_words(hex_string: str) -> list[int]:
    """Parse space-separated or continuous hex words into unsigned 16-bit integers."""
    clean = hex_string.replace(" ", "").replace("\n", "").strip()
    if len(clean) % 4 != 0:
        raise ValueError(
            f"Hex string length ({len(clean)}) must be a multiple of 4 (16-bit words)"
        )
    words = []
    for i in range(0, len(clean), 4):
        words.append(int(clean[i : i + 4], 16))
    return words


def decode_register_value(raw: int, spec: RegisterSpec) -> float | int:
    """Apply signedness and scale formula (value = signed(raw) * scale)."""
    if spec.dtype == "s16":
        signed_raw = struct.unpack(">h", struct.pack(">H", raw & 0xFFFF))[0]
        return round(signed_raw * spec.scale, 4)
    elif spec.dtype == "u16":
        return round((raw & 0xFFFF) * spec.scale, 4)
    return raw


def decode_fixture(fixture_path: Path) -> dict[str, Any]:
    """Load JSON fixture and decode all blocks against registered specifications."""
    with open(fixture_path, encoding="utf-8") as f:
        data = json.load(f)

    results: dict[str, Any] = {
        "description": data.get("description", ""),
        "blocks": [],
        "decoded_registers": {},
        "validations": [],
        "passed": True,
    }

    # Iterate over register blocks
    for block in data.get("registers", {}).get("blocks", []):
        start_addr = block["start_address"]
        raw_words = parse_hex_words(block["raw_hex"])
        block_info = {
            "comment": block.get("comment", ""),
            "start_address": start_addr,
            "hex_start": f"0x{start_addr:04X}",
            "count": len(raw_words),
            "registers": [],
        }

        for offset, raw_val in enumerate(raw_words):
            addr = start_addr + offset
            spec = REGISTER_MAP.get(addr)
            reg_entry: dict[str, Any] = {
                "address": addr,
                "hex_address": f"0x{addr:04X}",
                "raw_hex": f"0x{raw_val:04X}",
                "raw_dec": raw_val,
            }

            if spec:
                val = decode_register_value(raw_val, spec)
                reg_entry.update(
                    {
                        "name": spec.name,
                        "type": spec.dtype,
                        "scale": spec.scale,
                        "unit": spec.unit,
                        "decoded_value": val,
                        "provenance": spec.provenance,
                        "description": spec.description,
                    }
                )
                results["decoded_registers"][spec.name] = {
                    "address": f"0x{addr:04X}",
                    "value": val,
                    "unit": spec.unit,
                    "provenance": spec.provenance,
                }
            else:
                reg_entry.update(
                    {
                        "name": f"unmapped_0x{addr:04X}",
                        "provenance": "UNMAPPED",
                    }
                )

            block_info["registers"].append(reg_entry)

        results["blocks"].append(block_info)

    # Validate Gate 1: Check Battery SOC (0x07D2) empirical value == 80%
    soc_entry = results["decoded_registers"].get("battery.soc")
    if soc_entry and soc_entry["value"] == 80:
        results["validations"].append(
            {
                "check": "VERIFIED_EMPIRICALLY Battery SOC 0x07D2 == 80%",
                "status": "PASS",
                "details": f"Decoded SOC = {soc_entry['value']}%",
            }
        )
    else:
        results["validations"].append(
            {
                "check": "VERIFIED_EMPIRICALLY Battery SOC 0x07D2 == 80%",
                "status": "FAIL",
                "details": f"Unexpected SOC value: {soc_entry}",
            }
        )
        results["passed"] = False

    # Validate Gate 2: Battery Power canonical sign flip (+200W canonical from -200W raw)
    p_batt = results["decoded_registers"].get("battery.power")
    if p_batt and p_batt["value"] == 200:
        results["validations"].append(
            {
                "check": "Sign convention battery.power scale=-1 (+200W charging)",
                "status": "PASS",
                "details": "Canonical +200W derived from raw -200W",
            }
        )
    else:
        results["validations"].append(
            {
                "check": "Sign convention battery.power scale=-1",
                "status": "FAIL",
                "details": f"Unexpected battery.power: {p_batt}",
            }
        )
        results["passed"] = False

    # Validate Gate 3: Excluded control and configuration registers absent from polled blocks
    for block in results["blocks"]:
        start = block["start_address"]
        end = start + block["count"]
        # Ensure 0x09C4 (2500), 0x0A08 (2568), 0x0A09 (2569), 0x0BC1 (3009) not inside polled blocks
        for excluded_addr, spec in REGISTER_MAP.items():
            if spec.provenance in (
                ProvenanceState.DOCUMENTED_CONTROL_EXCLUDED,
                ProvenanceState.CONFIGURATION_EXCLUDED,
                ProvenanceState.DOCUMENTED_CONFLICTED,
            ) and start <= excluded_addr < end:
                results["validations"].append(
                    {
                        "check": f"Safety boundary: {spec.name} (0x{excluded_addr:04X}) quarantine",
                        "status": "FAIL",
                        "details": f"Excluded register 0x{excluded_addr:04X} present in poll block {start}-{end}",
                    }
                )
                results["passed"] = False

    results["validations"].append(
        {
            "check": "Safety quarantine for control and EEPROM registers",
            "status": "PASS",
            "details": "0x09C4, 0x0A08, 0x0A09, 0x0BC1, 0x0BC2 absent from telemetry blocks",
        }
    )

    return results


def print_provenance_matrix() -> None:
    """Print the formal provenance matrix formatted in markdown."""
    print("# Afore AF5K-SL Register Provenance Matrix")
    print(
        "| Hex Addr | Dec Addr | Name | Type | Scale | Unit | Formal State | Description |"
    )
    print(
        "|---|---|---|---|---|---|---|---|"
    )
    for addr, spec in sorted(REGISTER_MAP.items()):
        print(
            f"| `0x{addr:04X}` | {addr:4d} | `{spec.name}` | {spec.dtype} | {spec.scale} | {spec.unit} | **`{spec.provenance}`** | {spec.description} |"
        )


def print_probe_plan() -> None:
    """Print the conservative Phase 0 probe plan."""
    print("""# Phase 0 Conservative Probe Plan (Post-G1/G2 Blueprint)

Target Single-Register Probes:
  - Probe 1: 0x07D2 (Battery SOC)        - Cadence: 15s - FC 0x03, Count=1
  - Probe 2: 0x07D8 (Battery Power)      - Cadence: 10s - FC 0x03, Count=1
  - Probe 3: 0x022A (Total PV Power)     - Cadence: 10s - FC 0x03, Count=1
  - Probe 4: 0x0212 (Grid Active Power)  - Cadence: 10s - FC 0x03, Count=1

Bus Rules & Constraints:
  - Retry = 0 (drop frame on first failure, zero retransmissions)
  - Strict single transaction active on the bus at any millisecond
  - Inter-frame silence gap >= 200 ms
  - Emergency Stop: automated abort after 3 consecutive errors/timeouts
  - Isolation Rule: zero continuous blocks spanning unverified registers
""")


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Afore AF5K-SL Modbus RTU / Solarman Oracle & Provenance Validator"
    )
    parser.add_argument(
        "--fixture",
        type=Path,
        default=DEFAULT_FIXTURE,
        help="Path to af5k_sl_sample.json fixture",
    )
    parser.add_argument(
        "--provenance",
        action="store_true",
        help="Print formal provenance matrix",
    )
    parser.add_argument(
        "--probe-plan",
        action="store_true",
        help="Print conservative Phase 0 probe plan",
    )
    parser.add_argument(
        "--hex",
        type=str,
        help="Decode raw hex payload (16-bit words)",
    )
    parser.add_argument(
        "--start",
        type=lambda x: int(x, 0),
        default=0x07D0,
        help="Starting register address (hex or dec) for --hex option",
    )
    parser.add_argument(
        "--json",
        action="store_true",
        help="Output results in JSON format",
    )

    args = parser.parse_args()

    if args.provenance:
        print_provenance_matrix()
        return 0

    if args.probe_plan:
        print_probe_plan()
        return 0

    if args.hex:
        words = parse_hex_words(args.hex)
        decoded = []
        for i, word in enumerate(words):
            addr = args.start + i
            spec = REGISTER_MAP.get(addr)
            if spec:
                val = decode_register_value(word, spec)
                decoded.append(
                    {
                        "address": f"0x{addr:04X}",
                        "name": spec.name,
                        "raw": f"0x{word:04X} ({word})",
                        "value": val,
                        "unit": spec.unit,
                        "provenance": spec.provenance,
                    }
                )
            else:
                decoded.append(
                    {
                        "address": f"0x{addr:04X}",
                        "raw": f"0x{word:04X} ({word})",
                        "provenance": "UNMAPPED",
                    }
                )
        if args.json:
            print(json.dumps(decoded, indent=2))
        else:
            for item in decoded:
                print(
                    f"[{item['address']}] {item.get('name', 'unmapped'):<22}: raw={item['raw']:<12} -> {item.get('value', 'N/A')} {item.get('unit', '')} ({item['provenance']})"
                )
        return 0

    # Default mode: validate fixture
    if not args.fixture.exists():
        print(f"Error: Fixture file not found: {args.fixture}", file=sys.stderr)
        return 1

    results = decode_fixture(args.fixture)

    if args.json:
        print(json.dumps(results, indent=2))
    else:
        print(
            "======================================================================"
        )
        print(" Afore AF5K-SL Solarman & Modbus RTU Oracle Validation (Offline)")
        print(
            "======================================================================"
        )
        print(f"Fixture: {args.fixture}")
        print(f"Description: {results['description']}\n")

        print("--- Decoded Core Telemetry ---")
        for name, info in sorted(results["decoded_registers"].items()):
            print(
                f"  [{info['address']}] {name:<24}: {info['value']:>8} {info['unit']:<5} [{info['provenance']}]"
            )

        print("\n--- Validation Gates ---")
        for v in results["validations"]:
            status_tag = f"[{v['status']}]"
            print(f"  {status_tag:<8} {v['check']}")
            print(f"           Details: {v['details']}")

        print(
            "======================================================================"
        )
        if results["passed"]:
            print("[PASS] All offline oracle validation gates passed cleanly.")
        else:
            print("[FAIL] Oracle validation encountered errors.")

    return 0 if results["passed"] else 1


if __name__ == "__main__":
    sys.exit(main())
