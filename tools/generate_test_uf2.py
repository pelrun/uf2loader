#!/usr/bin/env python3
"""
Generate test UF2 files for testing the bootloader
"""

import struct
import sys
import os

# UF2 constants
UF2_MAGIC_START0 = 0x0A324655
UF2_MAGIC_START1 = 0x9E5D5157
UF2_MAGIC_END = 0x0AB16F30
UF2_FLAG_FAMILY_ID_PRESENT = 0x00002000

# Family IDs
FAMILY_ID_RP2040 = 0xe48bff56
FAMILY_ID_RP2350 = 0xe48bff59

class UF2Block:
    """Represents a single UF2 block"""
    def __init__(self, data, target_addr, block_no, num_blocks, family_id):
        self.magic_start0 = UF2_MAGIC_START0
        self.magic_start1 = UF2_MAGIC_START1
        self.flags = UF2_FLAG_FAMILY_ID_PRESENT
        self.target_addr = target_addr
        self.payload_size = len(data)
        self.block_no = block_no
        self.num_blocks = num_blocks
        self.file_size = family_id  # Family ID when flag is set
        self.data = data + b'\x00' * (476 - len(data))  # Pad to 476 bytes
        self.magic_end = UF2_MAGIC_END
    
    def to_bytes(self):
        """Convert block to bytes"""
        return struct.pack('<IIIIIIII476sI',
            self.magic_start0,
            self.magic_start1,
            self.flags,
            self.target_addr,
            self.payload_size,
            self.block_no,
            self.num_blocks,
            self.file_size,
            self.data,
            self.magic_end
        )

def create_test_uf2(filename, data, base_addr, family_id, block_size=256):
    """Create a UF2 file from data"""
    blocks = []
    num_blocks = (len(data) + block_size - 1) // block_size
    
    for i in range(num_blocks):
        offset = i * block_size
        chunk = data[offset:offset + block_size]
        block = UF2Block(chunk, base_addr + offset, i, num_blocks, family_id)
        blocks.append(block)
    
    with open(filename, 'wb') as f:
        for block in blocks:
            f.write(block.to_bytes())
    
    print(f"Created {filename}: {num_blocks} blocks, {len(data)} bytes")

def create_rp2350_e10_workaround_block():
    """Create RP2350-E10 workaround block (all 0xFF)"""
    return b'\xFF' * 512

def generate_test_files():
    """Generate various test UF2 files"""
    
    # Test 1: Simple RP2040 blink program
    print("Generating test UF2 files...")
    
    # Simple test pattern
    test_data = bytes([i & 0xFF for i in range(1024)])
    create_test_uf2("test_rp2040_pattern.uf2", test_data, 0x10040000, FAMILY_ID_RP2040)
    
    # RP2350 test with same pattern
    create_test_uf2("test_rp2350_pattern.uf2", test_data, 0x10040000, FAMILY_ID_RP2350)
    
    # Multi-block test (4KB)
    large_data = bytes([i & 0xFF for i in range(4096)])
    create_test_uf2("test_multiblock.uf2", large_data, 0x10040000, FAMILY_ID_RP2040)
    
    # Edge case: Single byte
    create_test_uf2("test_single_byte.uf2", b'\x42', 0x10040000, FAMILY_ID_RP2040)
    
    # Edge case: Exactly one block
    exact_block = bytes([0xAA] * 256)
    create_test_uf2("test_exact_block.uf2", exact_block, 0x10040000, FAMILY_ID_RP2040)
    
    # Invalid magic test - create manually
    print("Creating invalid magic test file...")
    block = UF2Block(b'TEST', 0x10040000, 0, 1, FAMILY_ID_RP2040)
    block.magic_start0 = 0xDEADBEEF  # Invalid magic
    with open("test_invalid_magic.uf2", 'wb') as f:
        f.write(block.to_bytes())
    
    # Out of bounds address test
    print("Creating out-of-bounds test file...")
    create_test_uf2("test_out_of_bounds.uf2", b'TEST', 0x00001000, FAMILY_ID_RP2040)
    
    # RP2350-E10 workaround test
    print("Creating RP2350-E10 workaround test file...")
    with open("test_rp2350_e10_workaround.uf2", 'wb') as f:
        # First block is workaround
        f.write(create_rp2350_e10_workaround_block())
        # Second block is actual data
        block = UF2Block(test_data[:256], 0x10040000, 1, 2, FAMILY_ID_RP2350)
        f.write(block.to_bytes())
    
    # Create a corrupted CRC test by modifying a valid file
    print("Creating corrupted data test file...")
    block = UF2Block(exact_block, 0x10040000, 0, 1, FAMILY_ID_RP2040)
    block_bytes = bytearray(block.to_bytes())
    # Corrupt one byte in the data section
    block_bytes[32 + 100] ^= 0x01  # Flip one bit
    with open("test_corrupted_data.uf2", 'wb') as f:
        f.write(block_bytes)
    
    print("\nTest files generated successfully!")
    print("\nTest scenarios:")
    print("1. test_rp2040_pattern.uf2 - Basic RP2040 test pattern")
    print("2. test_rp2350_pattern.uf2 - Basic RP2350 test pattern")
    print("3. test_multiblock.uf2 - Multi-block file (4KB)")
    print("4. test_single_byte.uf2 - Edge case: single byte")
    print("5. test_exact_block.uf2 - Edge case: exactly one block")
    print("6. test_invalid_magic.uf2 - Invalid magic number (should be rejected)")
    print("7. test_out_of_bounds.uf2 - Address outside valid range (should be rejected)")
    print("8. test_rp2350_e10_workaround.uf2 - RP2350-E10 workaround block")
    print("9. test_corrupted_data.uf2 - Valid structure but corrupted data")

if __name__ == "__main__":
    generate_test_files() 