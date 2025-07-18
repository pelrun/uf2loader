import os
import sys
import subprocess
import time
from renode_test import *
from generate_test_files import create_corrupted_uf2

# Define paths
ROOT_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
RENODE_DIR = os.path.join(ROOT_DIR, 'renode')
BUILD_DIR = os.path.join(ROOT_DIR, 'build')

# Test configurations
RENODE_PLATFORMS = {
    'pico': 'platforms/boards/raspberry_pi_pico.repl',
    'pico2': 'platforms/boards/raspberry_pi_pico2.repl'
}

ELF_FILES = {
    'pico': os.path.join(BUILD_DIR, 'picocalc_sd_boot.elf'),
    'pico2': os.path.join(BUILD_DIR, 'picocalc_sd_boot.elf')
}

SD_CARD_IMG = os.path.join(BUILD_DIR, 'sd_card.img')
FIRMWARE_DIR = '/tmp/sd_mount/firmware'

def setup_logger():
    """Set up basic logging if robot framework not available"""
    import logging
    logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')
    return logging.getLogger(__name__)

# Try to use robot logger, fall back to standard logging
try:
    from robot.api import logger
except ImportError:
    logger = setup_logger()

def send_key(renode_test, key):
    """Sends a simulated keypress to the UART."""
    key_map = {'up': '\x1b[A', 'down': '\x1b[B', 'enter': '\r'}
    char = key_map.get(key.lower())
    if char:
        renode_test.uart_key('sysbus.uart0', char)
        time.sleep(0.5)

def run_renode_test(board):
    """Runs a single Renode emulation test for the specified board."""
    if not os.path.exists(ELF_FILES[board]):
        logger.error(f"ELF file for {board} not found at {ELF_FILES[board]}")
        sys.exit(1)

    with RenodeTest(RENODE_DIR, platform=RENODE_PLATFORMS[board]) as test:
        test.run_command(f'sysbus LoadELF @{ELF_FILES[board]}')
        test.run_command(f'sdc SetImage @{SD_CARD_IMG}')
        test.run_command('s')

        test.wait_for_log_entry("PicoCalc Firmware Loader", timeout=30)
        logger.info(f"[{board}] Bootloader UI successfully loaded.")

        # --- Test Case: Flash a valid UF2 ---
        logger.info(f"[{board}] Testing flash of a valid UF2...")
        send_key(test, 'down')
        send_key(test, 'enter')
        test.wait_for_log_entry("Flash successful", timeout=60)
        logger.info(f"[{board}] Valid UF2 flashed successfully.")

        # --- Test Case: Flash corrupted UF2 (Bad CRC) ---
        logger.info(f"[{board}] Testing flash of a corrupted UF2 (bad CRC)...")
        test.run_command('r')
        test.wait_for_log_entry("PicoCalc Firmware Loader", timeout=30)
        send_key(test, 'down')
        send_key(test, 'down')
        send_key(test, 'enter')
        test.wait_for_log_entry("CRC32 mismatch", timeout=60)
        logger.info(f"[{board}] Correctly rejected UF2 with bad CRC.")

        # --- Test Case: Flash corrupted UF2 (Wrong Family ID) ---
        logger.info(f"[{board}] Testing flash of a corrupted UF2 (wrong family ID)...")
        test.run_command('r')
        test.wait_for_log_entry("PicoCalc Firmware Loader", timeout=30)
        send_key(test, 'down')
        send_key(test, 'down')
        send_key(test, 'down')
        send_key(test, 'enter')
        test.wait_for_log_entry("Family ID mismatch", timeout=60)
        logger.info(f"[{board}] Correctly rejected UF2 with wrong family ID.")

        logger.info(f"[{board}] All failure mode tests passed.")

def main():
    if not os.path.exists(RENODE_DIR):
        logger.info("Renode not found. Cloning...")
        subprocess.run(['git', 'clone', 'https://github.com/renode/renode.git', RENODE_DIR], check=True)

    pico_uf2 = ELF_FILES['pico'].replace('.elf', '.uf2')
    pico2_uf2 = ELF_FILES['pico2'].replace('.elf', '.uf2')
    corrupted_pico_dir = os.path.join(BUILD_DIR, 'corrupted_pico')
    corrupted_pico2_dir = os.path.join(BUILD_DIR, 'corrupted_pico2_w')
    
    if os.path.exists(pico_uf2):
        create_corrupted_uf2(pico_uf2, corrupted_pico_dir)
    if os.path.exists(pico2_uf2):
        create_corrupted_uf2(pico2_uf2, corrupted_pico2_dir)

    logger.info("Creating virtual SD card image...")
    if os.path.exists(SD_CARD_IMG):
        os.remove(SD_CARD_IMG)
    subprocess.run(['dd', 'if=/dev/zero', f'of={SD_CARD_IMG}', 'bs=1M', 'count=256'], check=True)
    subprocess.run(['mkfs.vfat', '-F', '32', SD_CARD_IMG], check=True)
    
    os.makedirs('/tmp/sd_mount', exist_ok=True)
    subprocess.run(['sudo', 'mount', SD_CARD_IMG, '/tmp/sd_mount'], check=True)
    os.makedirs(FIRMWARE_DIR, exist_ok=True)
    
    if os.path.exists(pico_uf2):
        subprocess.run(['sudo', 'cp', pico_uf2, f"{FIRMWARE_DIR}/valid_pico.uf2"], check=True)
        subprocess.run(['sudo', 'cp', '-r', f"{corrupted_pico_dir}/.", FIRMWARE_DIR], check=True)
        
    if os.path.exists(pico2_uf2):
         subprocess.run(['sudo', 'cp', pico2_uf2, f"{FIRMWARE_DIR}/valid_pico2.uf2"], check=True)
         subprocess.run(['sudo', 'cp', '-r', f"{corrupted_pico2_dir}/.", FIRMWARE_DIR], check=True)

    subprocess.run(['sudo', 'umount', '/tmp/sd_mount'], check=True)

    for board in RENODE_PLATFORMS:
        logger.info(f"--- Running Renode test for {board} ---")
        run_renode_test(board)

if __name__ == '__main__':
    main() 