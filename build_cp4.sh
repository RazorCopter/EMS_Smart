#!/bin/bash
# Quick build script for Shadow EMS CP4 verification

set -e

BUILD_DIR="build-cp4"

echo "=== Shadow EMS CP4 Build Script ==="
echo ""

# Create build directory
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

echo "Configuring with CMake..."
cmake .. -DCMAKE_BUILD_TYPE=Debug

echo ""
echo "Building test-shadow-ems target..."
make test-shadow-ems -j$(nproc)

echo ""
echo "Running Shadow EMS tests..."
./test-shadow-ems

echo ""
echo "Running all CTest tests..."
ctest --output-on-failure

echo ""
echo "✅ Build and test completed successfully!"
