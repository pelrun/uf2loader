# Project Structure

This document outlines the directory structure of the Picocalc SD Boot project after reorganization.

## Directory Layout

```
Picocalc_SD_Boot/
├── bootloader/              # Main bootloader implementation
│   ├── board/              # Board-specific configurations
│   ├── boot2/              # Second-stage bootloader (boot2_custom.S)
│   ├── include/            # Public header files
│   │   ├── atu.h           # ATU (Address Translation Unit) definitions
│   │   ├── bldetect.h      # Bootloader detection
│   │   ├── boot_defs.h     # Boot definitions and constants
│   │   ├── config.h        # Configuration parameters
│   │   ├── debug.h         # Debug macros and utilities
│   │   ├── key_event.h     # Keyboard event handling
│   │   ├── proginfo.h      # Program information structures
│   │   ├── text_directory_ui.h  # Text-based directory UI
│   │   └── uf2.h           # UF2 format definitions
│   ├── libs/               # Custom libraries
│   │   ├── i2ckbd/         # I2C keyboard driver
│   │   └── lcdspi/         # SPI LCD driver with fonts
│   ├── src/                # Source files
│   │   ├── bldetect.c      # Bootloader detection implementation
│   │   ├── ff_minimal_unicode.c  # Minimal unicode for FatFS
│   │   ├── key_event.c     # Keyboard event implementation
│   │   ├── main.c          # Main bootloader entry point
│   │   ├── proginfo.c      # Program info management
│   │   ├── text_directory_ui.c  # Directory browser UI
│   │   └── uf2.c           # UF2 file format handling
│   ├── memmap_2040.ld      # RP2040 linker script
│   └── memmap_2350.ld      # RP2350 linker script
├── tests/                   # Test suite
│   ├── critical/           # Critical scenario test data
│   ├── data/              # Test data files
│   ├── safety/            # Safety test data
│   ├── unit/              # Unit test data
│   ├── test_common.c      # Common test utilities
│   ├── test_critical_scenarios.c  # Critical edge case tests
│   ├── test_safety_gaps.c # Safety mechanism tests
│   ├── test_uf2.c         # UF2 implementation tests
│   ├── testuf2.c          # Additional UF2 tests
│   ├── uf2_test_utils.c   # UF2 test utilities
│   └── CMakeLists_standalone.txt  # Standalone CMake for host tests
├── tools/                   # Development and test tools
│   ├── generate_test_uf2.py      # Generate test UF2 files
│   ├── test_corrupted_uf2_flow.py # Generate corrupted test files
│   └── run_tests_cmake.sh        # CMake-based test runner
├── docs/                    # Documentation
│   ├── BOOTLOADER_SAFETY_REQUIREMENTS.md  # Safety design requirements
│   ├── IMPLEMENTATION_SUMMARY.md          # Implementation details
│   ├── README_TESTS.md                    # Test suite documentation
│   └── README_project_structure.md        # This file
├── pico-sdk/               # Raspberry Pi Pico SDK (submodule)
├── pico-vfs/               # Virtual filesystem library (submodule)
├── build/                  # CMake build directory (generated)
├── CMakeLists.txt          # Main CMake configuration
├── README.md               # Project overview
└── LICENSE                 # Project license

```

## Build System

The project uses CMake as its build system:

- **Main Build**: Configured by `CMakeLists.txt`, builds the bootloader for RP2040/RP2350
- **Test Build**: Tests are built separately as host executables using `tests/CMakeLists_standalone.txt`

## Key Components

### Bootloader (`bootloader/`)
The main bootloader implementation that:
- Handles UF2 file format for firmware updates
- Provides SD card boot functionality
- Includes safety mechanisms to prevent bricking
- Supports both RP2040 and RP2350 chips

### Tests (`tests/`)
Comprehensive test suite including:
- Unit tests for individual components
- Critical scenario tests for edge cases
- Safety gap tests for security/safety mechanisms

### Tools (`tools/`)
Host-side utilities for:
- Generating test UF2 files
- Running the complete test suite
- Creating corrupted files for error handling tests

### External Dependencies
- **pico-sdk**: Official Raspberry Pi Pico SDK
- **pico-vfs**: Virtual filesystem implementation with FatFS and LittleFS support

## Building

```bash
# Build bootloader
mkdir build && cd build
cmake .. -DPICO_BOARD=pico
make

# Run tests
./tools/run_tests_cmake.sh
``` 