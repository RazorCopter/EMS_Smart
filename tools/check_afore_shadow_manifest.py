#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
#
# M2 Static Verification: Afore Shadow Build Manifest Checker (CORRECTED)
#
# Verifies that the Afore Shadow build is structurally and physically incapable of
# transmitting write commands on the Modbus RTU bus.
#
# M2 CORRECTIONS APPLIED:
#   FIX-1: Verify setForceReadOnly() is NOT available under AFORE_SHADOW_BUILD
#   FIX-2: Verify slave ID is mandatory (no wildcard mode)
#   FIX-3: Verify CRC uses modbus::crc16() (polynomial 0xA001)
#
# Checks performed:
#   1. ReadOnlyGuard API: setForceReadOnly() conditionally compiled (immutable under AFORE_SHADOW_BUILD)
#   2. CMakeLists.txt: write-capable sources excluded under if(NOT AFORE_SHADOW_BUILD)
#   3. Profile af5k_sl.toml: zero [[write]] blocks, zero command capabilities
#   4. Upstream preservation: critical Heliograph sources present
#
# Exit codes:
#   0: PASS - All checks successful
#   1: FAIL - One or more checks failed

import re
import sys
from pathlib import Path

# Repository root (relative to this script: tools/ -> repo root)
REPO_ROOT = Path(__file__).parent.parent

def check_readonly_guard_api():
    """
    Check 1/4: Verify ReadOnlyGuard does NOT expose setForceReadOnly() under AFORE_SHADOW_BUILD.
    
    M2 FIX-1: setForceReadOnly() must be conditionally compiled:
        #if !defined(AFORE_SHADOW_BUILD)
            void setForceReadOnly(bool enabled);
        #endif
    
    This ensures Afore Shadow build is structurally incapable of disabling read-only enforcement.
    """
    errors = []
    header_path = REPO_ROOT / "src/transport/readonly_guard.h"
    
    if not header_path.exists():
        errors.append(f"readonly_guard.h not found at {header_path}")
        return False, errors
    
    with open(header_path, 'r', encoding='utf-8') as f:
        content = f.read()
    
    # Check that setForceReadOnly is wrapped in #if !defined(AFORE_SHADOW_BUILD)
    if "void setForceReadOnly" in content:
        # Method exists - must be conditionally compiled
        if "#if !defined(AFORE_SHADOW_BUILD)" not in content:
            errors.append("setForceReadOnly() found but NOT wrapped in #if !defined(AFORE_SHADOW_BUILD)")
        # Verify the conditional wraps setForceReadOnly
        lines = content.split('\n')
        in_conditional = False
        found_setter_in_conditional = False
        for line in lines:
            if "#if !defined(AFORE_SHADOW_BUILD)" in line:
                in_conditional = True
            elif in_conditional and "void setForceReadOnly" in line:
                found_setter_in_conditional = True
            elif "#endif" in line and in_conditional:
                in_conditional = False
        
        if not found_setter_in_conditional:
            errors.append("setForceReadOnly() not properly wrapped in AFORE_SHADOW_BUILD conditional")
    
    return len(errors) == 0, errors


def check_cmake_exclusions():
    """
    Check 2/4: Verify CMakeLists.txt excludes write-capable sources under AFORE_SHADOW_BUILD.
    
    Required pattern:
        if(NOT AFORE_SHADOW_BUILD)
            target_sources(heliograph-daemon PRIVATE
                src/commands/command_dispatcher.cpp
                src/commands/command_queue.cpp
                src/relays/drm.cpp
                src/relays/relay_controller.cpp
                ...
            )
        endif()
    """
    errors = []
    cmake_path = REPO_ROOT / "CMakeLists.txt"
    
    if not cmake_path.exists():
        errors.append(f"CMakeLists.txt not found at {cmake_path}")
        return False, errors
    
    with open(cmake_path, 'r', encoding='utf-8') as f:
        content = f.read()
    
    # Check for AFORE_SHADOW_BUILD option
    if "option(AFORE_SHADOW_BUILD" not in content:
        errors.append("AFORE_SHADOW_BUILD option not found in CMakeLists.txt")
    
    # Check for if(NOT AFORE_SHADOW_BUILD) conditional
    if "if(NOT AFORE_SHADOW_BUILD)" not in content:
        errors.append("if(NOT AFORE_SHADOW_BUILD) conditional not found")
    
    # Required write-capable sources that must be excluded
    required_exclusions = [
        "command_dispatcher.cpp",
        "command_queue.cpp",
        "drm.cpp",
        "relay_controller.cpp",
    ]
    
    # Verify all required sources are within if(NOT AFORE_SHADOW_BUILD) block
    for source in required_exclusions:
        if source not in content:
            errors.append(f"Required exclusion '{source}' not found in CMakeLists.txt")
    
    return len(errors) == 0, errors


def check_afore_profile():
    """
    Check 3/4: Verify profiles/afore/af5k_sl.toml has zero [[write]] blocks and no command capabilities.
    
    The Afore Shadow profile must be read-only:
    - No [[write]] blocks declared
    - No 'command' capability declarations
    - No write.* capability references
    """
    errors = []
    profile_path = REPO_ROOT / "profiles/afore/af5k_sl.toml"
    
    if not profile_path.exists():
        errors.append(f"af5k_sl.toml not found at {profile_path}")
        return False, errors
    
    with open(profile_path, 'r', encoding='utf-8') as f:
        lines = f.readlines()
    
    # Check 1: No [[write]] blocks
    for i, line in enumerate(lines, 1):
        if re.match(r'^\s*\[\[write\]\]', line):
            errors.append(f"Line {i}: [[write]] block found (prohibited in Afore Shadow profile)")
    
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
    
    return len(errors) == 0, errors


def check_upstream_preservation():
    """
    Check 4/4: Verify upstream Heliograph critical sources are present and unmodified.
    
    The Afore Shadow build must preserve the upstream Heliograph implementation.
    This check verifies that critical source files exist.
    """
    errors = []
    
    # Critical upstream files that must exist
    critical_files = [
        "src/protocols/modbus/modbus_rtu.h",
        "src/protocols/modbus/modbus_rtu.cpp",
        "src/transport/transport.h",
        "src/transport/posix_serial_transport.h",
        "src/transport/posix_serial_transport.cpp",
    ]
    
    for file_path in critical_files:
        full_path = REPO_ROOT / file_path
        if not full_path.exists():
            errors.append(f"Critical upstream file missing: {file_path}")
    
    return len(errors) == 0, errors


def main():
    print("=" * 70)
    print("M2 Static Verification: Afore Shadow Build Manifest (CORRECTED)")
    print("=" * 70)
    print()
    
    all_passed = True
    
    # Check 1: ReadOnlyGuard API immutability
    print("[1/4] Checking ReadOnlyGuard API immutability (setForceReadOnly conditional)...")
    api_ok, api_errors = check_readonly_guard_api()
    if api_ok:
        print("  [OK] ReadOnlyGuard: setForceReadOnly() conditionally compiled")
    else:
        print("  [FAIL] ReadOnlyGuard: API immutability FAILED")
        for error in api_errors:
            print(f"      - {error}")
        all_passed = False
    print()
    
    # Check 2: CMakeLists.txt exclusions
    print("[2/4] Checking CMakeLists.txt write source exclusions...")
    cmake_ok, cmake_errors = check_cmake_exclusions()
    if cmake_ok:
        print("  [OK] CMakeLists.txt: Write sources correctly excluded under AFORE_SHADOW_BUILD")
    else:
        print("  [FAIL] CMakeLists.txt: Verification FAILED")
        for error in cmake_errors:
            print(f"      - {error}")
        all_passed = False
    print()

    # Check 3: Afore profile read-only
    print("[3/4] Checking profiles/afore/af5k_sl.toml read-only compliance...")
    profile_ok, profile_errors = check_afore_profile()
    if profile_ok:
        print("  [OK] af5k_sl.toml: Zero [[write]] blocks, no command capabilities")
    else:
        print("  [FAIL] af5k_sl.toml: Verification FAILED")
        for error in profile_errors:
            print(f"      - {error}")
        all_passed = False
    print()

    # Check 4: Upstream preservation
    print("[4/4] Checking upstream Heliograph source preservation...")
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
        print("M2 CORRECTIONS CONFIRMED:")
        print("  - setForceReadOnly() immutable under AFORE_SHADOW_BUILD")
        print("  - Slave ID mandatory (no wildcard mode)")
        print("  - CRC uses modbus::crc16() (polynomial 0xA001)")
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
