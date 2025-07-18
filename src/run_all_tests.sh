#!/bin/bash

# Comprehensive test script for Picocalc SD Bootloader

set -e  # Exit on error

echo "=== Picocalc SD Bootloader Test Suite ==="
echo

# Clean previous builds
echo "Cleaning previous test builds..."
make -f Makefile.test clean >/dev/null 2>&1 || true

# Build all tests
echo "Building test suite..."
if ! make -f Makefile.test; then
    echo "ERROR: Failed to build tests"
    exit 1
fi

echo
echo "Running UF2 implementation tests..."
echo "=================================="
if ! ./test_uf2; then
    echo "ERROR: UF2 tests failed!"
    exit 1
fi

echo
echo "Running critical scenario tests..."
echo "=================================="
if ! ./test_critical; then
    echo "ERROR: Critical scenario tests failed!"
    exit 1
fi

echo
echo "Running safety gap tests..."
echo "=========================="
if ! ./test_safety_gaps; then
    echo "ERROR: Safety gap tests failed!"
    exit 1
fi

echo
echo "Generating corrupted UF2 test files..."
echo "====================================="
if command -v python3 &> /dev/null; then
    python3 test_corrupted_uf2_flow.py
else
    echo "WARNING: Python3 not found, skipping UF2 generation tests"
fi

echo
echo "=== BOOTLOADER TEST SUMMARY ==="
echo "All tests passed successfully!"
echo
echo "Safety implementations verified:"
echo "✓ Vector table validation"
echo "✓ Bootloader boundary protection"  
echo "✓ Power failure recovery mechanism"
echo "✓ CRC32 verification after flash"
echo "✓ Boot2 preservation"
echo "✓ Atomic prog_info updates"
echo
echo "The bootloader is ready for hardware testing." 