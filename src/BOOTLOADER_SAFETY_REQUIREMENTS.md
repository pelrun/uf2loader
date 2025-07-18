# Bootloader Safety Requirements

This document defines the **mandatory safety requirements** for the Picocalc SD Bootloader. Tests should verify these requirements, not be modified to match implementation behavior.

## Critical Safety Requirements

### 1. Firmware Integrity Protection

**Requirement**: The bootloader MUST verify firmware integrity before execution.

- **R1.1**: All flash writes MUST be verified with CRC32 immediately after writing
- **R1.2**: If CRC verification fails, the flash operation MUST abort and return failure
- **R1.3**: Corrupted firmware MUST NOT be marked as bootable in prog_info
- **R1.4**: The bootloader MUST validate vector table sanity before jumping to application

**Rationale**: Executing corrupted code can damage hardware or cause unpredictable behavior.

### 2. Atomicity of Firmware Updates

**Requirement**: Firmware updates MUST be atomic - either fully complete or fully failed.

- **R2.1**: prog_info MUST only be updated after ALL blocks are successfully written and verified
- **R2.2**: If any block fails to write or verify, prog_info MUST NOT be updated
- **R2.3**: Partial firmware writes MUST be detectable on next boot
- **R2.4**: The device MUST remain bootable after any failure during update

**Rationale**: Partial updates could leave the device in an unbootable state.

### 3. Bootloader Self-Protection

**Requirement**: The bootloader MUST protect itself from being overwritten.

- **R3.1**: Boot2 (first 256 bytes) MUST be preserved when flashing sector 0
- **R3.2**: The bootloader code area MUST never be erased or written to
- **R3.3**: Applications MUST NOT be allowed to write beyond their allocated space
- **R3.4**: Flash operations MUST enforce boundary checks

**Rationale**: A corrupted bootloader renders the device permanently unusable.

### 4. Platform-Specific Safety

**Requirement**: Platform-specific features MUST be handled safely.

- **R4.1**: [RP2040] Family ID MUST match RP2040_FAMILY_ID (0xe48bff56)
- **R4.2**: [RP2350] Family ID MUST match RP2350 variants
- **R4.3**: [RP2350] ATU remapping MUST use 4KB-aligned addresses
- **R4.4**: Cross-platform firmware MUST be rejected

**Rationale**: Wrong firmware for platform can cause undefined behavior.

### 5. Error Recovery

**Requirement**: The bootloader MUST handle all error conditions gracefully.

- **R5.1**: SD card removal during operation MUST NOT corrupt flash
- **R5.2**: Power loss during flash MUST leave device recoverable
- **R5.3**: Invalid UF2 files MUST be rejected without side effects
- **R5.4**: All errors MUST display user-friendly messages

**Rationale**: Users should always be able to recover from errors.

### 6. Flash Operation Safety

**Requirement**: Flash operations MUST be performed safely.

- **R6.1**: Flash operations MUST run from RAM (no XIP during flash write)
- **R6.2**: Memory barriers MUST ensure flash writes complete
- **R6.3**: Address alignment MUST be verified (256-byte pages, 4KB sectors)
- **R6.4**: Out-of-bounds addresses MUST be rejected

**Rationale**: Incorrect flash operations can corrupt memory or crash.

## Test Verification

Each requirement above MUST have corresponding tests that verify the behavior. If a test fails, it indicates the implementation is unsafe, not that the test needs adjustment.

### Test Principles

1. **Tests define correct behavior** - Never modify a test to match buggy implementation
2. **Tests should fail on unsafe behavior** - A failing test indicates a safety issue
3. **Tests must be comprehensive** - All safety requirements must be tested
4. **Tests must be realistic** - Test scenarios that can actually occur

## Consequences of Non-Compliance

Failure to meet these requirements can result in:

- Bricked devices (unrecoverable)
- Corrupted user applications
- Security vulnerabilities
- Hardware damage (in extreme cases)
- Loss of user trust

**Remember**: The bootloader is the last line of defense. If it fails, the device is lost. 