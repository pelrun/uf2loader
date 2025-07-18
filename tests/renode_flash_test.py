#!/usr/bin/env python3
"""
Renode emulation test for Picocalc SD Boot bootloader
Tests bootloader functionality on emulated Pico hardware
"""

import os
import sys
import time
import subprocess
import tempfile
import shutil

class RenodeBootloaderTest:
    def __init__(self):
        self.renode_path = "/opt/renode_1.14.0/renode"
        self.workspace = "/workspace"
        self.bootloader_elf = f"{self.workspace}/build/picocalc_sd_boot.elf"
        self.test_timeout = 60  # seconds
        
    def create_test_script(self, board="pico"):
        """Create Renode test script for specified board"""
        
        # Platform descriptions for different boards
        platforms = {
            "pico": "platforms/boards/raspberryPi_pico-rp2040.repl",
            "pico2": "platforms/boards/raspberryPi_pico2-rp2350.repl"
        }
        
        script = f"""
# Create machine
mach create "{board}"
machine LoadPlatformDescription @{platforms.get(board, platforms["pico"])}

# Configure peripherals
# Set up UART for console output
sysbus.uart0 AddLineHook "console_output" "print"

# Load bootloader ELF
sysbus LoadELF @{self.bootloader_elf}

# Configure flash memory (16MB)
sysbus.flash Size 0x1000000

# Add SD card peripheral
machine PyDevFromString '''
from Antmicro.Renode.Peripherals.SD import SDCard
import struct

class TestSDCard(SDCard):
    def __init__(self):
        super().__init__(blockSize=512, blockCount=524288)  # 256MB
        self.data = bytearray(512 * 524288)
        # Create mock filesystem structure
        self._create_mock_filesystem()
        
    def _create_mock_filesystem(self):
        # Simple FAT32 boot sector
        boot_sector = bytearray(512)
        boot_sector[0:3] = b'\\xEB\\x58\\x90'  # Jump instruction
        boot_sector[3:11] = b'MSDOS5.0'       # OEM name
        boot_sector[11:13] = struct.pack('<H', 512)  # Bytes per sector
        boot_sector[13] = 8  # Sectors per cluster
        boot_sector[510:512] = b'\\x55\\xAA'  # Boot signature
        self.data[0:512] = boot_sector
        
    def ReadData(self, offset, count):
        return bytes(self.data[offset:offset+count])
        
    def WriteData(self, offset, data):
        self.data[offset:offset+len(data)] = data

sd_card = TestSDCard()
'''
connector Connect sysbus.spi1 sd_card

# Monitor flash operations
python '''
def flash_monitor(self, isRead, offset, size):
    operation = "READ" if isRead else "WRITE"
    print(f"FLASH {operation}: offset=0x{offset:08X}, size={size}")
'''
sysbus.flash AddBlockAccessHook flash_monitor

# Set up logging function
def console_output(line):
    print(f"UART: {line}")
    
# Start the simulation
start

# Run for a short time to let bootloader initialize
sleep 2

# Print machine state
sysbus.cpu PC

# Continue running
runFor 5
"""
        return script
        
    def run_test(self, board="pico"):
        """Run bootloader test on specified board"""
        print(f"\n{'='*60}")
        print(f"Testing bootloader on emulated {board.upper()} hardware")
        print(f"{'='*60}\n")
        
        # Create temporary script file
        with tempfile.NamedTemporaryFile(mode='w', suffix='.resc', delete=False) as f:
            f.write(self.create_test_script(board))
            script_path = f.name
            
        try:
            # Run Renode
            cmd = [
                self.renode_path,
                '--disable-xwt',
                '--console',
                '--execute', script_path,
                '--port', '0'  # Don't open network ports
            ]
            
            print(f"Starting Renode emulation...")
            process = subprocess.Popen(
                cmd, 
                stdout=subprocess.PIPE, 
                stderr=subprocess.STDOUT, 
                text=True,
                env={**os.environ, 'TERM': 'dumb'}  # Disable color output
            )
            
            # Monitor output
            start_time = time.time()
            bootloader_started = False
            flash_operations = []
            
            while time.time() - start_time < self.test_timeout:
                line = process.stdout.readline()
                if not line and process.poll() is not None:
                    break
                    
                if line:
                    line = line.strip()
                    print(f"  {line}")
                    
                    # Check for bootloader initialization
                    if "PicoCalc" in line or "Bootloader" in line:
                        bootloader_started = True
                        print("\n✓ Bootloader detected in output")
                        
                    # Track flash operations
                    if "FLASH" in line:
                        flash_operations.append(line)
                        
                    # Check for errors
                    if "error" in line.lower() or "exception" in line.lower():
                        print(f"\n✗ Error detected: {line}")
                        
            # Terminate Renode
            process.terminate()
            process.wait(timeout=5)
            
            # Analyze results
            print(f"\n{'='*40}")
            print("Test Results:")
            print(f"{'='*40}")
            print(f"Bootloader started: {'✓ Yes' if bootloader_started else '✗ No'}")
            print(f"Flash operations: {len(flash_operations)}")
            
            if flash_operations:
                print("\nFlash activity detected:")
                for op in flash_operations[:5]:  # Show first 5 operations
                    print(f"  - {op}")
                if len(flash_operations) > 5:
                    print(f"  ... and {len(flash_operations) - 5} more")
                    
            return bootloader_started
            
        finally:
            # Clean up
            if os.path.exists(script_path):
                os.unlink(script_path)
                
    def test_flash_programming(self):
        """Test flash programming simulation"""
        print("\n" + "="*60)
        print("Testing Flash Programming Simulation")
        print("="*60 + "\n")
        
        # Create a test script that simulates flashing
        script = f"""
# Load machine and bootloader
mach create "pico"
machine LoadPlatformDescription @platforms/boards/raspberryPi_pico-rp2040.repl
sysbus LoadELF @{self.bootloader_elf}

# Pre-program some test data in flash to simulate existing firmware
sysbus WriteDoubleWord 0x10040000 0xDEADBEEF
sysbus WriteDoubleWord 0x10040004 0xCAFEBABE

# Monitor flash erase and program operations
python '''
def monitor(self, isRead, offset, size):
    addr = offset + 0x10000000  # Flash base address
    if not isRead:
        if size == 4096:  # Sector erase
            print(f"FLASH_ERASE: sector at 0x{addr:08X}")
        else:
            print(f"FLASH_WRITE: 0x{addr:08X} size={size}")
'''
sysbus.flash AddBlockAccessHook monitor

# Start simulation
start

# Simulate bootloader attempting to program flash
# This would normally be triggered by SD card UF2 file processing
runFor 0.1

# Read back flash content
sysbus ReadDoubleWord 0x10040000
sysbus ReadDoubleWord 0x10040004
"""
        
        with tempfile.NamedTemporaryFile(mode='w', suffix='.resc', delete=False) as f:
            f.write(script)
            script_path = f.name
            
        try:
            cmd = [self.renode_path, '--disable-xwt', '--console', script_path]
            output = subprocess.run(cmd, capture_output=True, text=True, timeout=30)
            
            print("Flash programming test output:")
            print(output.stdout)
            
            # Check for expected operations
            has_erase = "FLASH_ERASE" in output.stdout
            has_write = "FLASH_WRITE" in output.stdout
            
            print(f"\nFlash erase detected: {'✓' if has_erase else '✗'}")
            print(f"Flash write detected: {'✓' if has_write else '✗'}")
            
            return has_erase or has_write
            
        finally:
            if os.path.exists(script_path):
                os.unlink(script_path)

def main():
    """Main test runner"""
    print("Picocalc SD Boot - Renode Emulation Tests")
    print("==========================================\n")
    
    # Check if bootloader ELF exists
    elf_path = "/workspace/build/picocalc_sd_boot.elf"
    if not os.path.exists(elf_path):
        print(f"Error: Bootloader ELF not found at {elf_path}")
        print("Please build the project first")
        return 1
        
    tester = RenodeBootloaderTest()
    
    # Test on different boards
    boards = ["pico", "pico2"]
    results = {}
    
    for board in boards:
        try:
            results[board] = tester.run_test(board)
        except Exception as e:
            print(f"\nError testing {board}: {e}")
            results[board] = False
            
    # Test flash programming
    try:
        flash_test = tester.test_flash_programming()
        results["flash_programming"] = flash_test
    except Exception as e:
        print(f"\nError in flash programming test: {e}")
        results["flash_programming"] = False
        
    # Summary
    print("\n" + "="*60)
    print("TEST SUMMARY")
    print("="*60)
    
    for test, passed in results.items():
        status = "✓ PASSED" if passed else "✗ FAILED"
        print(f"{test:.<40} {status}")
        
    # Return exit code
    all_passed = all(results.values())
    return 0 if all_passed else 1

if __name__ == "__main__":
    sys.exit(main()) 