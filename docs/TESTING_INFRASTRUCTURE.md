# Pico Bootloader Testing Infrastructure

This document describes the comprehensive automated testing infrastructure implemented for the Pico bootloader project, following industry best practices for embedded systems testing.

## Overview

The testing infrastructure implements a multi-layered approach:

1. **Static Analysis** - Compile-time checks with clang-tidy
2. **Simulation Testing** - Deterministic tests using Renode
3. **Unit Testing** - Component-level tests
4. **Hardware Testing** - Real device validation with pytest-embedded
5. **Continuous Integration** - Automated pipeline with GitHub Actions

## Testing Stack

| Layer | Tool | Purpose |
|-------|------|---------|
| Simulation/CI | Renode + Robot Framework | Fast, deterministic, CI-ready RP2040 simulation |
| Test Runner (HW) | pytest-embedded | Python-based hardware test orchestration |
| Flashing/Verify | picotool | UF2/BIN conversion, USB-boot automation |
| Debug/Reset | PicoProbe/Debug Probe + OpenOCD | SWD debugging and flash verification |
| Build System | CMake + PlatformIO | Unified build and test execution |

## Key Components

### 1. Robot Framework Tests (`tests/bootloader.robot`)

Comprehensive test suite for Renode simulation covering:
- Power-on boot sequences
- Watchdog recovery
- UF2 update flows
- Brown-out conditions
- Flash protection
- GPIO/Clock states
- Memory exhaustion

### 2. Hardware Tests (`tests/test_hardware.py`)

pytest-embedded test suite for physical hardware validation:
- Serial communication tests
- Flash programming verification
- Command interface testing
- Stress testing
- Power control tests (with appropriate hardware)

### 3. UF2 Validation (`tools/check_uf2_crc32.py`)

Python tool for validating UF2 file integrity:
- Magic number verification
- CRC32 calculation
- Block structure validation
- Address overlap detection
- Family ID checking

### 4. CI/CD Pipeline (`.github/workflows/build.yml`)

Multi-stage GitHub Actions workflow:
- **Build Stage**: Compile for multiple boards with static analysis
- **Simulation Stage**: Run Renode tests in container
- **Unit Test Stage**: Execute component tests
- **Hardware Stage**: Optional physical device testing
- **Security Stage**: CodeQL and Semgrep analysis
- **Release Stage**: Automated release bundle creation

### 5. CI Helper Scripts

#### `ci-scripts/run_pytest_embedded.sh`
Bash script for hardware test execution:
- Environment setup
- Hardware connectivity checks
- Automatic retries
- Artifact collection
- Detailed logging

## Configuration Files

### `.clang-tidy`
Comprehensive static analysis rules focusing on:
- Safety-critical checks
- Embedded best practices
- Security vulnerabilities
- Code quality metrics

### `platformio.ini`
PlatformIO configuration supporting:
- Multiple board targets (pico, pico_w, pico2)
- Renode simulation environment
- Hardware test environment
- Debug configurations
- Static analysis profiles

### `tests/rp2040.resc`
Renode board configuration defining:
- CPU and memory layout
- Peripheral mappings
- UART configuration
- USB and flash setup

## Testing Workflow

### Local Development

1. **Run static analysis**:
   ```bash
   clang-tidy bootloader/src/*.c
   ```

2. **Run simulation tests**:
   ```bash
   renode-test tests/bootloader.robot
   ```

3. **Run hardware tests** (requires connected Pico):
   ```bash
   ./ci-scripts/run_pytest_embedded.sh --port /dev/ttyACM0
   ```

### CI Pipeline

Every pull request triggers:
1. Static analysis with clang-tidy
2. Build for all target boards
3. UF2 validation
4. Renode simulation tests
5. Security scanning

Nightly builds additionally run:
- PlatformIO tests
- Extended simulation scenarios

Manual triggers enable:
- Hardware-in-the-loop testing
- Release bundle generation

## Test Categories

### Safety-Critical Tests
- Watchdog timeout handling
- Brown-out recovery
- Stack overflow detection
- Flash protection boundaries

### Functional Tests
- Boot sequence verification
- UF2 update process
- Command interface
- Memory management

### Stress Tests
- Rapid reset cycles
- UART flooding
- Memory exhaustion
- Concurrent operations

### Security Tests
- Invalid UF2 rejection
- Buffer overflow protection
- Access control verification

## Hardware Requirements

### Minimum Setup
- Raspberry Pi Pico (target device)
- USB cable
- Host computer with USB

### Recommended Setup
- Multiple Pico boards for parallel testing
- PicoProbe or Debug Probe for SWD access
- USB hub with individual port control
- Serial-to-USB adapter for UART monitoring

### Advanced Setup
- Power control hardware for brown-out testing
- Logic analyzer for protocol debugging
- Current measurement equipment
- Temperature chamber for environmental testing

## Metrics and Reporting

The infrastructure generates:
- JUnit XML test results
- HTML test reports with embedded logs
- Code coverage metrics (Renode)
- Execution time analysis
- Binary size tracking

## Best Practices

1. **Write tests first** - TDD approach for new features
2. **Simulate before hardware** - Catch 80% of bugs in Renode
3. **Automate everything** - No manual testing steps
4. **Fail fast** - Quick feedback on every commit
5. **Track metrics** - Monitor test coverage and performance
6. **Document failures** - Detailed logs for debugging

## Troubleshooting

### Common Issues

1. **Renode tests failing**:
   - Check Robot Framework syntax
   - Verify binary paths
   - Review Renode logs

2. **Hardware tests failing**:
   - Verify USB permissions (`sudo usermod -a -G dialout $USER`)
   - Check serial port availability
   - Ensure bootloader is flashed

3. **CI pipeline issues**:
   - Check artifact paths
   - Verify Docker image availability
   - Review GitHub Actions logs

## Future Enhancements

- Integration with hardware test farms
- Fuzzing support for security testing
- Performance profiling integration
- Automated regression detection
- Multi-board parallel testing

## References

- [Renode Documentation](https://renode.readthedocs.io/)
- [Robot Framework User Guide](https://robotframework.org/robotframework/latest/RobotFrameworkUserGuide.html)
- [pytest-embedded Documentation](https://docs.espressif.com/projects/pytest-embedded/en/latest/)
- [PlatformIO Testing](https://docs.platformio.org/en/latest/advanced/unit-testing/index.html)
- [GitHub Actions Documentation](https://docs.github.com/en/actions) 