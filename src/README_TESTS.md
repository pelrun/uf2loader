# UF2 Implementation Test Suite

This directory contains comprehensive tests for the UF2 bootloader implementation in Picocalc SD Boot.

## Overview

The test suite consists of three main components:

1. **Unit Tests** (`test_uf2.c`) - Tests individual functions and components
2. **Critical Scenario Tests** (`test_critical_scenarios.c`) - Tests dangerous edge cases and failure modes
3. **Test File Generator** (`generate_test_uf2.py`) - Creates various UF2 files for testing
4. **Corrupted File Generator** (`test_corrupted_uf2_flow.py`) - Creates corrupted UF2 files to test error handling
5. **Integration Tests** (`test_integration.sh`) - Runs the complete test suite

## Running Tests

### Quick Start

Run all tests with:
```bash
./test_integration.sh
```

### Individual Components

#### Unit Tests Only
```bash
make -f Makefile.test test-unit
```

#### Critical Tests Only
```bash
make -f Makefile.test test-critical
```

#### Generate Test UF2 Files
```bash
python3 generate_test_uf2.py
python3 test_corrupted_uf2_flow.py
```

## Test Coverage

### Unit Tests
- **UF2 Block Validation**
  - Magic number verification
  - Family ID checking (RP2040/RP2350)
  - Address range validation
  - Block sequence validation

- **CRC32 Implementation**
  - Test vectors validation
  - Flash write verification
  - Error detection

- **Flash Operations**
  - Erase functionality
  - Program verification
  - CRC validation after write

- **Edge Cases**
  - RP2350-E10 workaround blocks
  - Multi-block files
  - Boundary conditions

### Critical Scenario Tests
- **Boot2 Preservation**
  - Ensures boot2 is never corrupted when flashing to sector 0
  - Tests the backup and restore mechanism

- **Flash Corruption Detection**
  - Single and multiple bit flips
  - Byte-level corruption
  - CRC verification failure paths

- **Partial Write Recovery**
  - Power failure simulation during multi-page writes
  - Verifies bootloader handles incomplete writes gracefully

- **Boundary Protection**
  - Tests flashing near bootloader boundaries
  - Ensures bootloader code is never overwritten

- **Atomic Operation Failures**
  - Multi-block write interruption
  - Ensures partial updates don't leave device unbootable

- **SD Card Removal**
  - Simulates SD card removal during flash operations
  - Tests cleanup and error handling

- **RP2350 Specific**
  - ATU alignment validation (4KB boundaries)
  - Address translation requirements

### Generated Test Files

| File | Description | Expected Result |
|------|-------------|-----------------|
| `test_rp2040_pattern.uf2` | Basic RP2040 test pattern | Valid |
| `test_rp2350_pattern.uf2` | Basic RP2350 test pattern | Valid |
| `test_multiblock.uf2` | 4KB multi-block file | Valid |
| `test_single_byte.uf2` | Single byte payload | Valid |
| `test_exact_block.uf2` | Exactly one 256-byte block | Valid |
| `test_invalid_magic.uf2` | Corrupted magic numbers | Invalid |
| `test_out_of_bounds.uf2` | Invalid target address | Invalid |
| `test_rp2350_e10_workaround.uf2` | RP2350-E10 workaround | Valid |
| `test_corrupted_data.uf2` | Valid structure, bad data | CRC fail |

### Corrupted Test Files

| File | Description | Expected Behavior |
|------|-------------|-------------------|
| `test_corrupt_bitflips.uf2` | Random bit flips throughout | Should fail validation |
| `test_corrupt_truncated.uf2` | Incomplete block data | Should fail to parse |
| `test_corrupt_oversized.uf2` | Extra data appended | Should fail validation |
| `test_corrupt_data_crc.uf2` | Corrupted payload data | Should fail CRC check during flash |
| `test_corrupt_block_order.uf2` | Out-of-sequence blocks | Should detect sequence error |
| `test_corrupt_misaligned.uf2` | Non-page-aligned addresses | Should reject on alignment check |
| `test_corrupt_zero_payload.uf2` | All zeros in payload | May pass structure check but fail app validation |
| `test_corrupt_boundary.uf2` | Would overflow flash | Should reject on boundary check |
| `test_corrupt_duplicate_addr.uf2` | Multiple blocks same address | Should detect duplicate writes |
| `test_corrupt_mixed_family.uf2` | Mixed RP2040/RP2350 blocks | Should reject on family ID mismatch |

## Test Architecture

### Mock Flash Memory
The unit tests use a 1MB mock flash array to simulate flash operations without requiring hardware.

### CRC32 Verification
All flash writes are verified using CRC32 to ensure data integrity. The implementation uses a 4-bit lookup table for efficiency.

### Test Macros
- `TEST(name)` - Define a test function
- `RUN_TEST(name)` - Execute a test
- `ASSERT_EQ(expected, actual)` - Assert equality
- `ASSERT_TRUE(condition)` - Assert condition is true
- `ASSERT_MEM_EQ(expected, actual, size)` - Assert memory equality

## Adding New Tests

### Unit Test Example
```c
TEST(my_new_test) {
    // Setup
    struct uf2_block block;
    create_uf2_block(&block, 0, 1, 0x10040000, NULL, 0xe48bff56);
    
    // Test
    ASSERT_EQ(block.payload_size, FLASH_PAGE_SIZE);
    
    // Verify
    ASSERT_TRUE(block.magic_end == 0x0AB16F30);
}

// Add to main():
RUN_TEST(my_new_test);
```

### Test UF2 File Example
```python
# In generate_test_uf2.py
def generate_custom_test():
    data = b"Custom test data"
    create_test_uf2("test_custom.uf2", data, 0x10040000, FAMILY_ID_RP2040)
```

## Continuous Integration

The test suite is designed to be CI-friendly:
- Returns 0 on success, 1 on failure
- Colored output for easy reading
- Summary statistics
- Automatic cleanup

## Troubleshooting

### Common Issues

1. **"gcc not found"**
   - Install GCC: `sudo apt-get install gcc` (Linux) or `brew install gcc` (macOS)

2. **"python3 not found"**
   - Install Python 3: `sudo apt-get install python3` (Linux) or `brew install python3` (macOS)

3. **Build failures**
   - Ensure Pico SDK path is correct
   - Check that all submodules are initialized

### Debug Mode

To see detailed output:
```bash
make -f Makefile.test CFLAGS="-DDEBUG"
```

## Future Improvements

- [ ] Add performance benchmarks
- [ ] Test flash wear leveling
- [ ] Add fuzzing tests
- [ ] Hardware-in-the-loop testing
- [ ] Code coverage reporting 