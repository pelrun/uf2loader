# UF2 Implementation Test Suite

This directory contains comprehensive tests for the UF2 bootloader implementation in Picocalc SD Boot.

## Overview

The test suite consists of three main components:

1. **Unit Tests** (`tests/test_uf2.c`) - Tests individual functions and components
2. **Critical Scenario Tests** (`tests/test_critical_scenarios.c`) - Tests dangerous edge cases and failure modes
3. **Safety Gap Tests** (`tests/test_safety_gaps.c`) - Tests safety mechanisms and boundary conditions
4. **Test File Generator** (`tools/generate_test_uf2.py`) - Creates various UF2 files for testing
5. **Corrupted File Generator** (`tools/test_corrupted_uf2_flow.py`) - Creates corrupted UF2 files to test error handling

## Running Tests

### Quick Start

Run all tests with the CMake test runner:
```bash
./tools/run_tests_cmake.sh
```

This will:
- Build all unit tests as host executables
- Run UF2 implementation tests
- Run critical scenario tests
- Run safety gap tests
- Generate test UF2 files

### Building Tests Manually

Tests are built separately from the main bootloader to run on the host system:

```bash
# Create build directory for tests
mkdir -p build_tests
cd build_tests

# Configure for host build
cmake ../tests -DCMAKE_C_COMPILER=gcc -DCMAKE_BUILD_TYPE=Debug

# Build all tests
make -j$(nproc)

# Run individual tests
./test_uf2
./test_critical_scenarios
./test_safety_gaps
```

### Generating Test Files

Generate test UF2 files:
```bash
cd tools
python3 generate_test_uf2.py
```

Generate corrupted UF2 files for error handling tests:
```bash
cd tools
python3 test_corrupted_uf2_flow.py
```

## Test Coverage

The test suite covers:

1. **Unit Tests** - Core functionality testing
2. **Integration Tests** - System-level behavior
3. **Safety Tests** - Critical safety requirements
4. **Corruption Tests** - Invalid input handling
5. **Emulation Tests** - Hardware simulation testing

## Emulation Testing with Renode

The project includes hardware emulation tests using Renode, which simulates the Raspberry Pi Pico hardware to test the bootloader in a virtualized environment.

### Running Renode Tests

Renode tests can be run using Docker (recommended) or with a local Renode installation:

```bash
# Using Docker (recommended)
./tools/run_renode_tests_docker.sh

# Using local Renode installation
cd tests
python3 renode_flash_test.py
```

### What Renode Tests Cover

The Renode emulation tests verify:

1. **Hardware Compatibility**
   - Tests on emulated Pico (RP2040) hardware
   - Tests on emulated Pico 2 (RP2350) hardware

2. **Bootloader Initialization**
   - Verifies bootloader starts correctly on emulated hardware
   - Checks UART output and console messages

3. **Flash Operations**
   - Monitors flash read/write operations
   - Verifies flash erase procedures
   - Tests flash programming sequences

4. **SD Card Interface**
   - Simulates SD card with mock FAT32 filesystem
   - Tests SD card communication over SPI

### Renode Test Output

A successful Renode test run shows:

```
Picocalc SD Boot - Renode Emulation Tests
==========================================

============================================================
Testing bootloader on emulated PICO hardware
============================================================

Starting Renode emulation...
  [Renode output messages...]
  
✓ Bootloader detected in output

========================================
Test Results:
========================================
Bootloader started: ✓ Yes
Flash operations: 15

Flash activity detected:
  - FLASH READ: offset=0x00000000, size=512
  - FLASH READ: offset=0x00001000, size=256
  ... and 13 more

============================================================
Testing Flash Programming Simulation
============================================================

Flash programming test output:
[Flash operation logs...]

Flash erase detected: ✓
Flash write detected: ✓

============================================================
TEST SUMMARY
============================================================
pico...................................... ✓ PASSED
pico2..................................... ✓ PASSED
flash_programming.......................... ✓ PASSED

✓ All Renode emulation tests passed!
```

### Docker Environment

The Docker environment includes:
- Ubuntu 22.04 base
- ARM GCC toolchain
- Renode 1.14.0
- Python 3 with required packages
- Pico SDK

Build and run tests in Docker:
```bash
# Build Docker image
docker build -t picocalc-test-env docker/

# Run all tests including Renode
docker run --rm -v $(pwd):/workspace -w /workspace picocalc-test-env \
    -c "./tools/run_all_tests.sh && ./tools/run_renode_tests_docker.sh"
```

## Test Organization

### Unit Tests (`test_uf2.c`)
- CRC32 computation
- Block validation
- Address boundary checking
- Magic number validation
- Payload size validation
- Family ID handling

### Critical Scenario Tests (`test_critical_scenarios.c`)
- Power loss during flash write
- Corrupted blocks mid-transfer
- Out-of-order block reception
- Overlapping memory regions
- Flash boundary violations

### Safety Gap Tests (`test_safety_gaps.c`)
- Vector table validation
- Bootloader boundary protection
- Boot2 preservation
- Atomic prog_info updates
- Flash wear leveling

## Test File Types

The test generators create various UF2 files:

### Valid Test Files
- `test_rp2040_pattern.uf2` - Basic RP2040 test pattern
- `test_rp2350_pattern.uf2` - Basic RP2350 test pattern
- `test_multiblock.uf2` - Multi-block file (4KB)
- `test_single_byte.uf2` - Edge case: single byte
- `test_exact_block.uf2` - Edge case: exactly one block
- `test_rp2350_e10_workaround.uf2` - RP2350-E10 silicon workaround

### Invalid Test Files
- `test_invalid_magic.uf2` - Invalid magic number
- `test_out_of_bounds.uf2` - Address outside valid range
- `test_corrupted_data.uf2` - Valid structure but corrupted data

### Corrupted Test Files
- `test_corrupt_bitflips.uf2` - Random bit flips
- `test_corrupt_truncated.uf2` - Truncated block
- `test_corrupt_oversized.uf2` - Block with extra data
- `test_corrupt_data_crc.uf2` - CRC failure simulation
- `test_corrupt_block_order.uf2` - Out-of-sequence blocks
- `test_corrupt_misaligned.uf2` - Misaligned target address
- `test_corrupt_zero_payload.uf2` - Zero payload data
- `test_corrupt_boundary.uf2` - Flash boundary overflow
- `test_corrupt_duplicate_addr.uf2` - Multiple blocks to same address
- `test_corrupt_mixed_family.uf2` - Mixed RP2040/RP2350 blocks

## Expected Behavior

The bootloader should:
1. Accept all valid test files and flash them correctly
2. Reject all invalid/corrupted files without flashing
3. Never leave the device in an unbootable state
4. Display appropriate error messages for failures
5. Preserve critical regions (Boot2, bootloader itself)

## Safety Verifications

The test suite verifies these safety mechanisms:
- ✓ Vector table validation
- ✓ Bootloader boundary protection  
- ✓ Power failure recovery mechanism
- ✓ CRC32 verification after flash
- ✓ Boot2 preservation
- ✓ Atomic prog_info updates

## Integration with CI/CD

The test suite can be integrated into CI/CD pipelines:

```yaml
# Example GitHub Actions workflow
- name: Run bootloader tests
  run: |
    ./tools/run_tests_cmake.sh
```

## Debugging Test Failures

If tests fail:

1. Check the test output for specific failure messages
2. Run individual tests with debugging:
   ```bash
   gdb ./build_tests/test_uf2
   ```
3. Enable debug output in the bootloader by defining `DEBUG`
4. Check generated UF2 files with a hex editor

## Adding New Tests

To add new test cases:

1. Add test functions to the appropriate test file
2. Register them in the test runner
3. Update this documentation
4. Ensure the test passes on both RP2040 and RP2350 targets 