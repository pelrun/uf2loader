#!/usr/bin/env python3
"""
UF2 CRC32 Validation Tool
Verifies UF2 file integrity by checking magic numbers, CRC32, and structure.
Based on Microsoft UF2 specification: https://github.com/microsoft/uf2
"""

import sys
import struct
import zlib
import argparse
import json
from pathlib import Path

# UF2 Constants
UF2_MAGIC_START0 = 0x0A324655  # "UF2\n"
UF2_MAGIC_START1 = 0x9E5D5157  # Randomly selected
UF2_MAGIC_END = 0x0AB16F30     # Ditto

# UF2 Block structure
UF2_BLOCK_SIZE = 512
UF2_DATA_SIZE = 476
UF2_HEADER_SIZE = 32
UF2_FOOTER_SIZE = 4

# Family IDs
FAMILY_RP2040 = 0xE48BFF56
FAMILY_RP2350 = 0xE48BFF59
FAMILY_RP2350_ARM_S = 0xE48BFF5A
FAMILY_RP2350_RISCV = 0xE48BFF5B

FAMILY_NAMES = {
    FAMILY_RP2040: "RP2040",
    FAMILY_RP2350: "RP2350",
    FAMILY_RP2350_ARM_S: "RP2350-ARM-S",
    FAMILY_RP2350_RISCV: "RP2350-RISCV"
}

class UF2Block:
    """Represents a single UF2 block"""
    def __init__(self, data):
        if len(data) != UF2_BLOCK_SIZE:
            raise ValueError(f"Invalid block size: {len(data)} != {UF2_BLOCK_SIZE}")
        
        # Parse header
        header = struct.unpack("<IIIIIIII", data[:32])
        self.magic_start0 = header[0]
        self.magic_start1 = header[1]
        self.flags = header[2]
        self.target_addr = header[3]
        self.payload_size = header[4]
        self.block_number = header[5]
        self.total_blocks = header[6]
        self.family_id = header[7]
        
        # Extract payload
        self.payload = data[32:32 + self.payload_size]
        
        # Parse footer
        self.magic_end = struct.unpack("<I", data[508:512])[0]
        
    def is_valid(self):
        """Check if block has valid magic numbers"""
        return (self.magic_start0 == UF2_MAGIC_START0 and
                self.magic_start1 == UF2_MAGIC_START1 and
                self.magic_end == UF2_MAGIC_END)
    
    def calculate_crc32(self):
        """Calculate CRC32 of the payload"""
        return zlib.crc32(self.payload) & 0xFFFFFFFF

class UF2Validator:
    """Validates UF2 files for correctness"""
    def __init__(self, filepath):
        self.filepath = Path(filepath)
        self.blocks = []
        self.errors = []
        self.warnings = []
        
    def load(self):
        """Load and parse UF2 file"""
        try:
            data = self.filepath.read_bytes()
            
            # Check file size
            if len(data) % UF2_BLOCK_SIZE != 0:
                self.errors.append(f"File size {len(data)} is not a multiple of {UF2_BLOCK_SIZE}")
                return False
                
            # Parse blocks
            num_blocks = len(data) // UF2_BLOCK_SIZE
            for i in range(num_blocks):
                block_data = data[i * UF2_BLOCK_SIZE:(i + 1) * UF2_BLOCK_SIZE]
                try:
                    block = UF2Block(block_data)
                    self.blocks.append(block)
                except Exception as e:
                    self.errors.append(f"Failed to parse block {i}: {e}")
                    
            return len(self.blocks) > 0
            
        except Exception as e:
            self.errors.append(f"Failed to read file: {e}")
            return False
    
    def validate_structure(self):
        """Validate UF2 structure and consistency"""
        if not self.blocks:
            self.errors.append("No blocks loaded")
            return False
            
        # Check magic numbers
        for i, block in enumerate(self.blocks):
            if not block.is_valid():
                self.errors.append(f"Block {i}: Invalid magic numbers")
                
        # Check block numbering
        expected_total = self.blocks[0].total_blocks if self.blocks else 0
        for i, block in enumerate(self.blocks):
            if block.block_number != i:
                self.errors.append(f"Block {i}: Incorrect block number {block.block_number}")
            if block.total_blocks != expected_total:
                self.errors.append(f"Block {i}: Inconsistent total blocks {block.total_blocks} != {expected_total}")
                
        # Check we have all blocks
        if len(self.blocks) != expected_total:
            self.errors.append(f"Missing blocks: have {len(self.blocks)}, expected {expected_total}")
            
        # Check family ID consistency
        family_ids = set(block.family_id for block in self.blocks)
        if len(family_ids) > 1:
            self.warnings.append(f"Multiple family IDs found: {[FAMILY_NAMES.get(fid, hex(fid)) for fid in family_ids]}")
            
        # Check payload sizes
        for i, block in enumerate(self.blocks):
            if block.payload_size > UF2_DATA_SIZE:
                self.errors.append(f"Block {i}: Payload size {block.payload_size} > {UF2_DATA_SIZE}")
            if block.payload_size == 0:
                self.warnings.append(f"Block {i}: Empty payload")
                
        # Check address alignment
        for i, block in enumerate(self.blocks):
            if block.target_addr % 256 != 0:
                self.warnings.append(f"Block {i}: Target address 0x{block.target_addr:08x} not 256-byte aligned")
                
        # Check for overlapping addresses
        addr_map = {}
        for i, block in enumerate(self.blocks):
            addr_range = range(block.target_addr, block.target_addr + block.payload_size)
            for addr in addr_range:
                if addr in addr_map:
                    self.errors.append(f"Block {i} overlaps with block {addr_map[addr]} at 0x{addr:08x}")
                addr_map[addr] = i
                
        return len(self.errors) == 0
    
    def calculate_total_crc32(self):
        """Calculate CRC32 of all payloads combined"""
        all_data = b''.join(block.payload for block in self.blocks)
        return zlib.crc32(all_data) & 0xFFFFFFFF
    
    def get_info(self):
        """Get UF2 file information"""
        if not self.blocks:
            return {}
            
        first_block = self.blocks[0]
        family_name = FAMILY_NAMES.get(first_block.family_id, f"Unknown (0x{first_block.family_id:08x})")
        
        # Calculate address range
        min_addr = min(block.target_addr for block in self.blocks)
        max_addr = max(block.target_addr + block.payload_size for block in self.blocks)
        
        # Calculate total payload size
        total_payload = sum(block.payload_size for block in self.blocks)
        
        return {
            "filename": str(self.filepath),
            "file_size": self.filepath.stat().st_size,
            "num_blocks": len(self.blocks),
            "family_id": family_name,
            "min_address": f"0x{min_addr:08x}",
            "max_address": f"0x{max_addr:08x}",
            "total_payload": total_payload,
            "crc32": f"0x{self.calculate_total_crc32():08x}",
            "valid": len(self.errors) == 0
        }

def main():
    parser = argparse.ArgumentParser(description="Validate UF2 file CRC32 and structure")
    parser.add_argument("uf2_file", help="Path to UF2 file to validate")
    parser.add_argument("--json", action="store_true", help="Output results as JSON")
    parser.add_argument("--verbose", "-v", action="store_true", help="Verbose output")
    args = parser.parse_args()
    
    validator = UF2Validator(args.uf2_file)
    
    # Load file
    if not validator.load():
        if args.json:
            print(json.dumps({"error": "Failed to load file", "errors": validator.errors}))
        else:
            print(f"ERROR: Failed to load {args.uf2_file}")
            for error in validator.errors:
                print(f"  - {error}")
        sys.exit(1)
    
    # Validate structure
    validator.validate_structure()
    
    # Get info
    info = validator.get_info()
    
    if args.json:
        output = {
            "info": info,
            "errors": validator.errors,
            "warnings": validator.warnings
        }
        print(json.dumps(output, indent=2))
    else:
        print(f"UF2 File: {info['filename']}")
        print(f"Size: {info['file_size']} bytes ({info['num_blocks']} blocks)")
        print(f"Family: {info['family_id']}")
        print(f"Address range: {info['min_address']} - {info['max_address']}")
        print(f"Total payload: {info['total_payload']} bytes")
        print(f"CRC32: {info['crc32']}")
        
        if validator.errors:
            print("\nERRORS:")
            for error in validator.errors:
                print(f"  ✗ {error}")
                
        if validator.warnings and args.verbose:
            print("\nWARNINGS:")
            for warning in validator.warnings:
                print(f"  ! {warning}")
                
        if info['valid']:
            print("\n✓ UF2 file is valid")
        else:
            print("\n✗ UF2 file is INVALID")
    
    sys.exit(0 if info['valid'] else 1)

if __name__ == "__main__":
    main() 