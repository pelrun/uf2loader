#!/usr/bin/env python3
"""
Generate corrupted UF2 files to test bootloader's error handling
"""

import struct
import random
import os

# UF2 constants
UF2_MAGIC_START0 = 0x0A324655
UF2_MAGIC_START1 = 0x9E5D5157
UF2_MAGIC_END = 0x0AB16F30
UF2_FLAG_FAMILY_ID_PRESENT = 0x00002000
FAMILY_ID_RP2040 = 0xe48bff56
FAMILY_ID_RP2350 = 0xe48bff59

def create_uf2_block(data, target_addr, block_no, num_blocks, family_id):
    """Create a properly formatted UF2 block"""
    block = struct.pack('<IIIIIIII',
        UF2_MAGIC_START0,
        UF2_MAGIC_START1,
        UF2_FLAG_FAMILY_ID_PRESENT,
        target_addr,
        len(data),
        block_no,
        num_blocks,
        family_id
    )
    # Pad data to 476 bytes
    padded_data = data + b'\x00' * (476 - len(data))
    block += padded_data
    block += struct.pack('<I', UF2_MAGIC_END)
    return block

def corrupt_block(block, corruption_type):
    """Apply various corruption types to a UF2 block"""
    block_bytes = bytearray(block)
    
    if corruption_type == "bit_flips":
        # Random bit flips throughout the block
        for _ in range(10):
            pos = random.randint(0, len(block_bytes) - 1)
            block_bytes[pos] ^= random.randint(1, 255)
    
    elif corruption_type == "truncated":
        # Truncate the block
        return bytes(block_bytes[:400])
    
    elif corruption_type == "oversized":
        # Add extra data
        return bytes(block_bytes) + b'\xFF' * 100
    
    elif corruption_type == "bad_crc_simulation":
        # Corrupt data section only (simulates CRC failure)
        for i in range(32, 200):
            block_bytes[i] ^= 0x55
    
    elif corruption_type == "wrong_block_order":
        # Swap block numbers
        block_no = struct.unpack('<I', block_bytes[20:24])[0]
        struct.pack_into('<I', block_bytes, 20, (block_no + 5) % 10)
    
    elif corruption_type == "address_misalignment":
        # Misalign target address
        target_addr = struct.unpack('<I', block_bytes[12:16])[0]
        struct.pack_into('<I', block_bytes, 12, target_addr + 1)
    
    elif corruption_type == "zero_payload":
        # Zero out the payload
        for i in range(32, 508):
            block_bytes[i] = 0
    
    elif corruption_type == "flash_boundary_overflow":
        # Set target address near flash end
        struct.pack_into('<I', block_bytes, 12, 0x10000000 + 0x1FF000)
    
    return bytes(block_bytes)

def generate_corrupted_uf2_files():
    """Generate various corrupted UF2 files for testing"""
    
    corruption_scenarios = [
        ("test_corrupt_bitflips.uf2", "bit_flips", 
         "Random bit flips throughout the file"),
        
        ("test_corrupt_truncated.uf2", "truncated",
         "Truncated block (incomplete data)"),
        
        ("test_corrupt_oversized.uf2", "oversized",
         "Block with extra data appended"),
        
        ("test_corrupt_data_crc.uf2", "bad_crc_simulation",
         "Valid structure but corrupted data (CRC fail)"),
        
        ("test_corrupt_block_order.uf2", "wrong_block_order",
         "Blocks out of sequence"),
        
        ("test_corrupt_misaligned.uf2", "address_misalignment",
         "Target address not page-aligned"),
        
        ("test_corrupt_zero_payload.uf2", "zero_payload",
         "All payload data is zero"),
        
        ("test_corrupt_boundary.uf2", "flash_boundary_overflow",
         "Would write beyond flash boundary"),
    ]
    
    # Create a base valid firmware
    test_data = bytes([i & 0xFF for i in range(1024)])
    base_addr = 0x10040000
    
    print("Generating corrupted UF2 test files...")
    print("-" * 60)
    
    for filename, corruption_type, description in corruption_scenarios:
        # Create 4 blocks
        blocks = []
        for i in range(4):
            block_data = test_data[i*256:(i+1)*256]
            block = create_uf2_block(
                block_data,
                base_addr + i * 256,
                i,
                4,
                FAMILY_ID_RP2040
            )
            
            # Apply corruption to the second block
            if i == 1:
                block = corrupt_block(block, corruption_type)
            
            blocks.append(block)
        
        # Write file
        with open(filename, 'wb') as f:
            for block in blocks:
                f.write(block)
        
        print(f"{filename:<30} - {description}")
    
    # Create a special case: valid blocks but will fail during flash
    print("\nSpecial test cases:")
    print("-" * 60)
    
    # Multiple writes to same address
    with open("test_corrupt_duplicate_addr.uf2", 'wb') as f:
        for i in range(4):
            block = create_uf2_block(
                test_data[:256],
                base_addr,  # Same address for all blocks!
                i,
                4,
                FAMILY_ID_RP2040
            )
            f.write(block)
    print(f"test_corrupt_duplicate_addr.uf2 - Multiple blocks target same address")
    
    # Mix of RP2040 and RP2350 blocks
    with open("test_corrupt_mixed_family.uf2", 'wb') as f:
        for i in range(4):
            family = FAMILY_ID_RP2040 if i < 2 else FAMILY_ID_RP2350
            block = create_uf2_block(
                test_data[i*256:(i+1)*256],
                base_addr + i * 256,
                i,
                4,
                family
            )
            f.write(block)
    print(f"test_corrupt_mixed_family.uf2   - Mixed RP2040/RP2350 blocks")
    
    print("\nCorrupted test files generated successfully!")
    print("\nExpected bootloader behavior:")
    print("- Should reject all these files without flashing")
    print("- Should not leave device in unbootable state")
    print("- Should display appropriate error messages")

if __name__ == "__main__":
    generate_corrupted_uf2_files() 