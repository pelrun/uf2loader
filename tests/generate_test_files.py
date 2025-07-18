import os
import shutil
import random

def create_corrupted_uf2(input_file, output_dir):
    """Creates various corrupted versions of a valid UF2 file for testing."""
    if not os.path.exists(output_dir):
        os.makedirs(output_dir)

    base_name = os.path.basename(input_file).replace('.uf2', '')

    # --- Test Case 1: Bad Magic Number ---
    bad_magic_file = os.path.join(output_dir, f"{base_name}_bad_magic.uf2")
    shutil.copy(input_file, bad_magic_file)
    with open(bad_magic_file, 'r+b') as f:
        f.seek(0)
        f.write(b'\x00\x00\x00\x00') # Overwrite first magic number
        f.seek(508)
        f.write(b'\x00\x00\x00\x00') # Overwrite final magic number

    # --- Test Case 2: Incorrect Family ID ---
    wrong_family_file = os.path.join(output_dir, f"{base_name}_wrong_family.uf2")
    shutil.copy(input_file, wrong_family_file)
    with open(wrong_family_file, 'r+b') as f:
        # Find the family ID tag and corrupt it
        data = f.read()
        # A simple approach: just find and replace the known RP2040/RP2350 IDs
        # RP2040 family ID: 0xe48bff56
        # RP2350 family ID: 0x79d239a0
        # Replace with a bogus ID
        data = data.replace(b'\x56\xff\x8b\xe4', b'\xde\xad\xbe\xef')
        data = data.replace(b'\xa0\x39\xd2\x79', b'\xde\xad\xbe\xef')
        f.seek(0)
        f.write(data)

    # --- Test Case 3: Bad CRC32 Checksum ---
    bad_crc_file = os.path.join(output_dir, f"{base_name}_bad_crc.uf2")
    shutil.copy(input_file, bad_crc_file)
    with open(bad_crc_file, 'r+b') as f:
        # Corrupt a byte in the data payload of the first block
        f.seek(32) 
        original_byte = f.read(1)
        f.seek(32)
        f.write(bytes([original_byte[0] ^ 0xFF]))

    print(f"Generated test files in {output_dir}")

if __name__ == '__main__':
    # This script would be called from the main test runner
    # with paths to the compiled UF2 files.
    # Example usage:
    pico_uf2 = '../src/build/picocalc_sd_boot_pico.uf2'
    pico2_uf2 = '../src/build/picocalc_sd_boot_pico2_w.uf2'
    if os.path.exists(pico_uf2):
        create_corrupted_uf2(pico_uf2, '../src/build/test_uf2/corrupted_pico')
    if os.path.exists(pico2_uf2):
        create_corrupted_uf2(pico2_uf2, '../src/build/test_uf2/corrupted_pico2_w') 