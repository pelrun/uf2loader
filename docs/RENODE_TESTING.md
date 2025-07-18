# Renode Emulation Testing for Picocalc SD Boot

## Overview

The project includes infrastructure for testing the bootloader on emulated Raspberry Pi Pico hardware using Renode. This allows verification of bootloader functionality without physical hardware.

## Test Infrastructure

### Files Created

1. **`tests/renode_test.py`** - Base class for managing Renode simulation instances
2. **`tests/run_renode_tests.py`** - Original comprehensive test runner 
3. **`tests/renode_flash_test.py`** - Detailed flash operation testing
4. **`tests/simple_renode_test.py`** - Simplified test for basic bootloader verification
5. **`tools/run_renode_tests_docker.sh`** - Docker-based test runner script

### Docker Environment

The Docker image (`docker/Dockerfile`) includes:
- Ubuntu 22.04 base
- ARM GCC toolchain for cross-compilation
- Renode 1.14.0 (x86-64 Linux binary)
- Python 3 with Robot Framework
- Pico SDK

## Current Status

### What Works
✅ Bootloader builds successfully in Docker
✅ UF2 files are generated correctly
✅ Test infrastructure is in place
✅ Docker environment is properly configured

### Known Limitations

#### ARM Mac Compatibility Issue
When running on Apple Silicon (ARM) Macs, the x86-64 Renode binary fails with:
```
rosetta error: failed to open elf at /lib64/ld-linux-x86-64.so.2
```

This is because:
1. The Renode binary is compiled for x86-64 Linux
2. Docker on ARM Macs uses Rosetta for x86 emulation
3. The emulation layer has issues with the specific binary format

### Potential Solutions

1. **Use ARM-native Renode**: Wait for or build an ARM Linux version of Renode
2. **Run on x86 hardware**: The tests should work on Intel Macs or Linux x86 machines
3. **Use QEMU instead**: Alternative emulation that may have better ARM support
4. **GitHub Actions**: Run the tests in CI/CD on x86 Linux runners

## Running the Tests

### On Compatible Systems (x86-64)
```bash
# Build and run all tests including Renode
./tools/run_renode_tests_docker.sh
```

### What the Tests Verify

1. **Hardware Compatibility**
   - Bootloader initialization on RP2040 (Pico)
   - Bootloader initialization on RP2350 (Pico 2)

2. **Flash Operations**
   - Monitor flash read/write/erase operations
   - Verify correct flash programming sequences
   - Track memory access patterns

3. **Peripheral Simulation**
   - SD card interface over SPI
   - UART console output
   - Basic FAT32 filesystem simulation

## Test Output Example

On a working system, you would see:
```
Testing bootloader on emulated PICO hardware
============================================
Starting Renode simulation...
✓ Bootloader detected in output
✓ Flash operations: 15

Testing bootloader on emulated PICO2 hardware
=============================================
✓ UART output captured
✓ Flash operations detected

TEST SUMMARY
============
pico..................................... ✓ PASSED
pico2.................................... ✓ PASSED
```

## Future Improvements

1. Add ARM64 Linux Renode support
2. Implement alternative emulation options (QEMU)
3. Add more comprehensive flash programming tests
4. Test actual UF2 file processing in emulation
5. Simulate various failure scenarios

## Integration with CI/CD

The Renode tests can be integrated into GitHub Actions or other CI/CD pipelines running on x86-64 Linux to ensure continuous validation of bootloader functionality across different Pico hardware variants. 

### Alternative: Native Testing on macOS

For macOS users, especially on Apple Silicon (M1/M2/M3), we recommend running Renode tests natively instead of using Docker:

```bash
# Install Renode on macOS
# Option 1: Using Homebrew (if available)
brew install renode

# Option 2: Download from GitHub
# 1. Download the .dmg from https://github.com/renode/renode/releases
# 2. Install it
# 3. Add alias to your shell config:
echo "alias renode='mono /Applications/Renode.app/Contents/MacOS/bin/Renode.exe'" >> ~/.zshrc

# Run tests natively
./tools/run_renode_tests_native.sh
```

### Platform-Specific Limitations

#### Apple Silicon (ARM64) Macs

Currently, there are limitations when running Renode in Docker on Apple Silicon Macs:

1. **No Native ARM64 Docker Images**: Renode's official Docker images are only available for x86-64 (linux/amd64)
2. **Rosetta Emulation Issues**: Running x86-64 binaries through Rosetta in Docker can cause compatibility issues
3. **Building from Source**: While possible, building Renode from source in Docker is time-consuming

**Recommended Solution**: Use native macOS installation of Renode instead of Docker for testing on Apple Silicon.

#### Linux on ARM64

For ARM64 Linux systems:
- The portable Linux package should work with Mono runtime
- Building from source is recommended for best performance
- Docker usage requires building Renode from source within the container

### Future Improvements

- Native ARM64 builds of Renode for better performance
- Official ARM64 Docker images from the Renode project
- Improved cross-platform testing infrastructure 