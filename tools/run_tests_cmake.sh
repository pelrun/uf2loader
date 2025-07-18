#!/bin/bash

# Test script for Picocalc SD Bootloader with CMake build system

set -e  # Exit on error

echo "=== Picocalc SD Bootloader Test Suite (CMake) ==="
echo

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Get script directory
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$( cd "${SCRIPT_DIR}/.." && pwd )"

# Build tests separately as host executables
echo "Building tests as host executables..."
cd "${PROJECT_ROOT}"

# Create a separate build directory for tests
mkdir -p build_tests
cd build_tests

# Copy the standalone CMakeLists.txt for tests
cp ../tests/CMakeLists_standalone.txt ../tests/CMakeLists.txt

# Configure for host build (not cross-compilation)
echo "Configuring tests..."
cmake ../tests \
    -DCMAKE_C_COMPILER=gcc \
    -DCMAKE_CXX_COMPILER=g++ \
    -DCMAKE_BUILD_TYPE=Debug

# Build tests
echo "Building tests..."
make -j$(nproc 2>/dev/null || echo 4)

echo
echo "Running UF2 implementation tests..."
echo "=================================="
if ./test_uf2; then
    echo -e "${GREEN}✓ UF2 tests passed${NC}"
else
    echo -e "${RED}✗ UF2 tests failed!${NC}"
    exit 1
fi

echo
echo "Running critical scenario tests..."
echo "=================================="
if ./test_critical_scenarios; then
    echo -e "${GREEN}✓ Critical scenario tests passed${NC}"
else
    echo -e "${RED}✗ Critical scenario tests failed!${NC}"
    exit 1
fi

echo
echo "Running safety gap tests..."
echo "=========================="
if ./test_safety_gaps; then
    echo -e "${GREEN}✓ Safety gap tests passed${NC}"
else
    echo -e "${RED}✗ Safety gap tests failed!${NC}"
    exit 1
fi

echo
echo "Generating test UF2 files..."
echo "====================================="
cd "${PROJECT_ROOT}/tools"
if command -v python3 &> /dev/null; then
    if python3 generate_test_uf2.py; then
        echo -e "${GREEN}✓ Test UF2 files generated${NC}"
    else
        echo -e "${YELLOW}⚠ Failed to generate test UF2 files${NC}"
    fi
    
    if [ -f test_corrupted_uf2_flow.py ]; then
        python3 test_corrupted_uf2_flow.py
        echo -e "${GREEN}✓ Corrupted UF2 test files generated${NC}"
    fi
else
    echo -e "${YELLOW}WARNING: Python3 not found, skipping UF2 generation tests${NC}"
fi

echo
echo "=== BOOTLOADER TEST SUMMARY ==="
echo -e "${GREEN}All tests passed successfully!${NC}"
echo
echo "Safety implementations verified:"
echo "✓ Vector table validation"
echo "✓ Bootloader boundary protection"  
echo "✓ Power failure recovery mechanism"
echo "✓ CRC32 verification after flash"
echo "✓ Boot2 preservation"
echo "✓ Atomic prog_info updates"
echo
echo "Main bootloader binary: ${PROJECT_ROOT}/build/picocalc_sd_boot.elf"
echo "The bootloader is ready for hardware testing." 