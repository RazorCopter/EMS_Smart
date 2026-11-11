#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# Native ARM64 Raspberry Pi Build & Validation Script
set -euo pipefail

echo "=== Heliograph Afore EMS: Native ARM64 Raspberry Pi Verification ==="
echo "Target Architecture: $(uname -m)"
echo "Kernel: $(uname -r)"
echo "Compiler: $(c++ --version | head -n 1)"

if [[ "$(uname -m)" != "aarch64" && "$(uname -m)" != "arm64" ]]; then
  echo "WARNING: Running on non-arm64 architecture ($(uname -m)). Native ARM64 verification requires an ARM64 host."
fi

BUILD_DIR="$(mktemp -d -t afore-arm64-build-XXXXXX)"
cleanup() {
  echo "Cleaning up temporary build directory: ${BUILD_DIR}"
  rm -rf "${BUILD_DIR}"
}
trap cleanup EXIT

echo "Configuring with CMake in ${BUILD_DIR}..."
cmake -S . -B "${BUILD_DIR}" \
  -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTING=ON \
  -DAFORE_SHADOW_BUILD=ON \
  -DCMAKE_CXX_FLAGS="-Wall -Wextra -Werror"

JOBS=${VERIFY_JOBS:-1}
echo "Building all targets on native ARM64 with concurrency ${JOBS}..."
cmake --build "${BUILD_DIR}" -j "${JOBS}"

echo "Executing CTest test suite on native ARM64..."
(cd "${BUILD_DIR}" && ctest --output-on-failure)

echo "Executing Target-Aware EMS standalone test binary..."
"${BUILD_DIR}/test-target-aware-ems"

echo "=== ALL ARM64 NATIVE CHECKS PASSED SUCCESSFULLY ==="
