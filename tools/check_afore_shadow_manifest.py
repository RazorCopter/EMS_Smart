#!/usr/bin/env python3
"""
M2 Static Verification: Afore Shadow Build Manifest Checker

Verifies compile-time read-only enforcement for AFORE_SHADOW_BUILD=1:
1. CMakeLists.txt excludes write-capable sources when AFORE_SHADOW_BUILD is set
2. profiles/afore/af5k_sl.toml contains zero [[write]] blocks
3. profiles/afore/af5k_sl.toml has no command capability declarations
4. Upstream Heliograph target preserves all original sources

Exit codes:
    0 = All checks passed (read-only manifest verified)
    1 = Verification failed (write capability detected or manifest violated)
"""

import re
import sys
from pathlib import Path
from typing import List, Tuple

# Project root (relative to tools/)
PROJECT_ROOT = Path(__file__).parent.parent

# M2 Write-capable sources that MUST be excluded under AFORE_SHADOW_BUILD
WRITE_SOURCES = [
    "src/commands/command_dispatcher.cpp",
    "src/commands/command_queue.cpp",
    "src/relays/drm.cpp",
    "src/relays/relay_controller.cpp",
    "src/status/boot_button.cpp",
    "src/status/status_led.cpp",
]

# Critical registers that MUST NOT appear in [[write]] blocks (EEPROM protection)
FORBIDDEN_WRITE_REGISTERS = [
    0x09C4,  # EMS Mode
    0x0A08,  # External Control Type
    0x0A09,  # Active Power Setpoint (low word)
    0x0A0A,  # Active Power Setpoint (high word)
    0x0BC1,  # CurrMaxChg (EEPROM wear hazard)
    0x0BC2,  # Related charge control
]


def check_cmake_exclusions() -> Tuple[bool, List[str]]:
    """
    Verify CMakeLists.txt conditionally excludes write sources under AFORE_SHADOW_BUILD.
    Returns: (success, error_messages)
    """
    cmake_path = PROJECT_ROOT / "CMakeLists.txt"
    errors = []

    if not cmake_path.exists():
        return False, ["CMakeLists.txt not found"]

    content = cmake_path.read_text(encoding="utf-8")

    # Check 1: AFORE_SHADOW_BUILD option exists
    if "AFORE_SHADOW_BUILD" not in content:
        errors.append("CMakeLists.txt missing AFORE_SHADOW_BUILD option")

    # Check 2: Write sources are conditionally excluded (if(NOT AFORE_SHADOW_BUILD))
    conditional_pattern = r"if\s*\(\s*NOT\s+AFORE_SHADOW_BUILD\s*\)"
    if not re.search(conditional_pattern, content, re.IGNORECASE):
        errors.append("CMakeLists.txt missing 'if(NOT AFORE_SHADOW_BUILD)' conditional")

    # Check 3: All write sources appear in the excluded block
    for source in WRITE_SOURCES:
        if source not in content:
            errors.append(f"Write source '{source}' not referenced in CMakeLists.txt")
            continue

        # Verify it appears after "if(NOT AFORE_SHADOW_BUILD)"
        # Simple heuristic: check that the conditional exists before the source
        source_pos = content.find(source)
        conditional_pos = content.rfind("if(NOT AFORE_SHADOW_BUILD", 0, source_pos)
        if conditional_pos == -1:
            errors.append(
                f"Write source '{source}' not protected by AFORE_SHADOW_BUILD conditional"
            )

    return len(errors) == 0, errors


def check_afore_profile() -> Tuple[bool, List[str]]:
    """
    Verify profiles/afore/af5k_sl.toml is strictly read-only:
    - Zero [[write]] blocks
    - Zero command capability references
    - No forbidden registers
    Returns: (success, error_messages)
    """
    profile_path = PROJECT_ROOT / "profiles" / "afore" / "af5k_sl.toml"
    errors = []

    if not profile_path.exists():
        return False, ["profiles/afore/af5k_sl.toml not found"]

    content = profile_path.read_text(encoding="utf-8")
    lines = content.split("\n")

    # Check 1: No [[write]] blocks
    write_blocks = [i for i, line in enumerate(lines, 1) if re.match(r"^\s*\[\[write\]\]", line)]
    if write_blocks:
        errors.append(
            f"Found {len(write_blocks)} [[write]] block(s) at line(s): {write_blocks}"
        )

    # Check 2: No command capability declarations (skip comments)
    command_patterns = [
        r"\bcommand\s*=",
        r"capability.*write",
        r"write\s*\.\s*\w+",
    ]
    for i, line in enumerate(lines, 1):
        # Skip comment lines
        stripped = line.strip()
        if stripped.startswith("#"):
            continue
        for pattern in command_patterns:
            if re.search(pattern, line, re.IGNORECASE):
                errors.append(f"Line {i}: Potential command capability: {line.strip()}")

    # Check 3: No forbidden write registers appear in any context
    for reg in FORBIDDEN_WRITE_REGISTERS:
        hex_pattern = rf"0x{reg:04X}"
        dec_pattern = rf"\b{reg}\b"
        for i, line in enumerate(lines, 1):
            if re.search(hex_pattern, line, re.IGNORECASE) or re.search(dec_pattern, line):
                # Allow in comments (documenting why it's forbidden)
                if not line.strip().startswith("#"):
                    errors.append(
                        f"Line {i}: Forbidden register 0x{reg:04X} ({reg}) found: {line.strip()}"
                    )

    return len(errors) == 0, errors


def check_upstream_preservation() -> Tuple[bool, List[str]]:
    """
    Verify upstream Heliograph sources are preserved (not deleted or moved).
    Returns: (success, error_messages)
    """
    errors = []
    
    # Sampling of critical upstream sources that must exist
    upstream_sources = [
        "src/device/capability.cpp",
        "src/device/measurement.cpp",
        "src/transport/transport.cpp",
        "src/protocols/modbus/modbus_rtu.cpp",
        "src/protocols/modbus/modbus_client.cpp",
    ]
    
    for source in upstream_sources:
        source_path = PROJECT_ROOT / source
        if not source_path.exists():
            errors.append(f"Upstream source missing: {source}")
    
    return len(errors) == 0, errors


def main() -> int:
    """Run all M2 static verification checks."""
    print("=" * 70)
    print("M2 Static Verification: Afore Shadow Build Manifest")
    print("=" * 70)
    print()

    all_passed = True
    
    # Check 1: CMakeLists.txt exclusions
    print("[1/3] Checking CMakeLists.txt write source exclusions...")
    cmake_ok, cmake_errors = check_cmake_exclusions()
    if cmake_ok:
        print("  [OK] CMakeLists.txt: Write sources correctly excluded under AFORE_SHADOW_BUILD")
    else:
        print("  [FAIL] CMakeLists.txt: Verification FAILED")
        for error in cmake_errors:
            print(f"      - {error}")
        all_passed = False
    print()

    # Check 2: Afore profile read-only
    print("[2/3] Checking profiles/afore/af5k_sl.toml read-only compliance...")
    profile_ok, profile_errors = check_afore_profile()
    if profile_ok:
        print("  [OK] af5k_sl.toml: Zero [[write]] blocks, no command capabilities")
    else:
        print("  [FAIL] af5k_sl.toml: Verification FAILED")
        for error in profile_errors:
            print(f"      - {error}")
        all_passed = False
    print()

    # Check 3: Upstream preservation
    print("[3/3] Checking upstream Heliograph source preservation...")
    upstream_ok, upstream_errors = check_upstream_preservation()
    if upstream_ok:
        print("  [OK] Upstream sources: All critical files present")
    else:
        print("  [FAIL] Upstream sources: Verification FAILED")
        for error in upstream_errors:
            print(f"      - {error}")
        all_passed = False
    print()

    # Summary
    print("=" * 70)
    if all_passed:
        print("[PASS] M2 VERIFICATION PASSED: Afore Shadow read-only manifest verified")
        print()
        print("Build command for Afore Shadow read-only daemon:")
        print("  cmake -DAFORE_SHADOW_BUILD=ON ..")
        print("  make heliograph-daemon")
        print()
        return 0
    else:
        print("[FAIL] M2 VERIFICATION FAILED: Read-only manifest violations detected")
        print()
        print("The Afore Shadow build is NOT safe for deployment until all checks pass.")
        print()
        return 1


if __name__ == "__main__":
    sys.exit(main())
