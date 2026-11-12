#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# CI Pipeline Offline - Verification Gates
# Validates:
#   Gate 1: tools/check_layering.sh (11 architectural layering checks)
#   Gate 2: Static analysis via cppcheck
#   Gate 3: Negative compile test (setForceReadOnly() blocked in AFORE_SHADOW_BUILD)
#   Gate 4: CTest unit test regression suite
#   Gate 5: Simulator UI integrity & test suite

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WORKSPACE_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

cd "${WORKSPACE_ROOT}"

echo "================================================================"
echo "           STARTING OFFLINE CI VERIFICATION GATES               "
echo "================================================================"

echo ""
echo "[GATE 1/5] Architectural Layering Checks (check_layering.sh)..."
bash tools/check_layering.sh
echo "--> Gate 1 PASSED"

echo ""
echo "[GATE 2/5] Static Analysis (cppcheck)..."
cppcheck --error-exitcode=1 --enable=warning,portability --inline-suppr \
    -I src \
    src/drivers/afore/ \
    src/transport/readonly_guard.cpp \
    src/device/telemetry_quality.cpp
echo "--> Gate 2 PASSED"

echo ""
echo "[GATE 3/5] Write-Path Negative Compile Test..."
set +e
COMPILE_OUT=$(g++ -DAFORE_SHADOW_BUILD=1 -Isrc -Im3a -std=c++17 m3a/negative_compile/disable_guard.cpp -c -o /dev/null 2>&1)
COMPILE_RC=$?
set -e

if [ "${COMPILE_RC}" -ne 0 ] && echo "${COMPILE_OUT}" | grep -q "setForceReadOnly"; then
    echo "Negative compile test correctly failed as expected: setForceReadOnly() is unavailable."
    echo "--> Gate 3 PASSED"
else
    echo "ERROR: Negative compile test did not fail with expected error! Code: ${COMPILE_RC}"
    echo "${COMPILE_OUT}"
    exit 1
fi

echo ""
echo "[GATE 4/5] Native CTest Regression Suite..."
mkdir -p build_cmake && cd build_cmake
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)
ctest --output-on-failure
cd "${WORKSPACE_ROOT}"
echo "--> Gate 4 PASSED"

echo ""
echo "[GATE 5/5] Simulator UI Integrity Check..."
python3 -m py_compile simulator_ui/server.py simulator_ui/test_simulator.py
python3 -m unittest discover -s simulator_ui -p "test_*.py"
echo "--> Gate 5 PASSED"

echo ""
echo "================================================================"
echo "       ALL OFFLINE CI VERIFICATION GATES PASSED (5/5)           "
echo "================================================================"
