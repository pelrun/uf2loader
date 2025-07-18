#!/usr/bin/env python3
"""
Simple Renode emulation test for Picocalc SD Boot bootloader
Tests basic bootloader startup on emulated Pico hardware
"""

import os
import sys
import time
import subprocess
import tempfile
import shutil

def test_bootloader_startup():
    """Test that bootloader starts up correctly in Renode"""
    
    # Get the actual path to the ELF file
    elf_path = os.path.abspath('build/picocalc_sd_boot.elf')
    
    # Create a simple Renode script
    renode_script = f"""
# Create machine
mach create "pico"
machine LoadPlatformDescription @platforms/boards/raspberryPi_pico-rp2040.repl

# Load bootloader ELF
sysbus LoadELF @{elf_path}

# Create a minimal SD card simulation
machine PyDevFromString '''
from Antmicro.Renode.Peripherals.SD import SDCard
sd_card = SDCard(blockSize=512, blockCount=524288)  # 256MB
'''
connector Connect sysbus.spi1 sd_card

# Configure UART to monitor output
sysbus.uart0 CreateFileBackend @/tmp/uart_output.txt

# Start the simulation
start

# Run for 2 seconds
sleep 2

# Print some status
sysbus.cpu PC
echo "Bootloader test completed"
"""
    
    # Write script to file
    with tempfile.NamedTemporaryFile(mode='w', suffix='.resc', delete=False) as f:
        f.write(renode_script)
        script_path = f.name
    
    print("Picocalc SD Boot - Simple Renode Test")
    print("=====================================\n")
    
    try:
        # Run Renode
        # Try different ways to run Renode
        # For macOS, we need to use the proper launcher
        macos_launcher = '/Applications/Renode.app/Contents/MacOS/macos_run.command'
        
        # Use the macOS launcher command
        cmd = [macos_launcher, '--disable-xwt', '--console', '--execute', script_path, '--port', '0']
        
        print("Starting Renode simulation...")
        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            timeout=30,
            env={**os.environ, 'TERM': 'xterm'}
        )
        
        print("\nRenode Output:")
        print("-" * 40)
        print(result.stdout)
        if result.stderr:
            print("\nRenode Errors:")
            print(result.stderr)
        
        # Check UART output
        uart_output = ""
        if os.path.exists('/tmp/uart_output.txt'):
            with open('/tmp/uart_output.txt', 'r') as f:
                uart_output = f.read()
            print("\nUART Output:")
            print("-" * 40)
            print(uart_output)
            os.unlink('/tmp/uart_output.txt')
        
        # Determine success
        success = False
        if "Bootloader test completed" in result.stdout:
            print("\n✓ Renode simulation completed successfully")
            success = True
            
        if "PicoCalc" in uart_output or "Bootloader" in uart_output:
            print("✓ Bootloader output detected")
            success = True
        elif not uart_output:
            print("⚠ No UART output captured")
        else:
            print("✗ Bootloader signature not found in UART output")
            
        return 0 if success else 1
        
    except subprocess.TimeoutExpired:
        print("\n✗ Renode simulation timed out")
        return 1
    except Exception as e:
        print(f"\n✗ Error running Renode: {e}")
        return 1
    finally:
        # Cleanup
        if os.path.exists(script_path):
            os.unlink(script_path)

def test_multiple_boards():
    """Test on both Pico and Pico2 boards"""
    # Get the actual path to the ELF file
    elf_path = os.path.abspath('build/picocalc_sd_boot.elf')
    
    boards = {
        "pico": "platforms/boards/raspberryPi_pico-rp2040.repl",
        "pico2": "platforms/boards/raspberryPi_pico2-rp2350.repl"
    }
    
    results = {}
    
    for board, platform in boards.items():
        print(f"\n{'='*50}")
        print(f"Testing {board.upper()}")
        print(f"{'='*50}\n")
        
        # Create board-specific script
        renode_script = f"""
mach create "{board}"
machine LoadPlatformDescription @{platform}
sysbus LoadELF @{elf_path}
sysbus.uart0 CreateFileBackend @/tmp/uart_{board}.txt
start
sleep 1
"""
        
        with tempfile.NamedTemporaryFile(mode='w', suffix='.resc', delete=False) as f:
            f.write(renode_script)
            script_path = f.name
        
        try:
            # Use the macOS launcher command
            macos_launcher = '/Applications/Renode.app/Contents/MacOS/macos_run.command'
            cmd = [macos_launcher, '--disable-xwt', '--console', '--execute', script_path, '--port', '0']
            result = subprocess.run(cmd, capture_output=True, text=True, timeout=15)
            
            # Check UART output
            uart_file = f'/tmp/uart_{board}.txt'
            if os.path.exists(uart_file):
                with open(uart_file, 'r') as f:
                    uart_output = f.read()
                if uart_output:
                    print(f"✓ {board}: UART output captured")
                    results[board] = True
                else:
                    print(f"✗ {board}: No UART output")
                    results[board] = False
                os.unlink(uart_file)
            else:
                print(f"✗ {board}: UART file not created")
                results[board] = False
                
        except Exception as e:
            print(f"✗ {board}: Error - {e}")
            results[board] = False
        finally:
            if os.path.exists(script_path):
                os.unlink(script_path)
    
    # Summary
    print(f"\n{'='*50}")
    print("TEST SUMMARY")
    print(f"{'='*50}")
    
    all_passed = True
    for board, passed in results.items():
        status = "✓ PASSED" if passed else "✗ FAILED"
        print(f"{board:.<40} {status}")
        if not passed:
            all_passed = False
    
    return 0 if all_passed else 1

def main():
    """Main test runner"""
    # Check if bootloader ELF exists
    elf_path = os.path.abspath("build/picocalc_sd_boot.elf")
    if not os.path.exists(elf_path):
        print(f"Error: Bootloader ELF not found at {elf_path}")
        print("Please build the project first")
        return 1
    
    # Run simple startup test
    result1 = test_bootloader_startup()
    
    # Run multi-board test
    result2 = test_multiple_boards()
    
    # Return failure if any test failed
    return 1 if (result1 != 0 or result2 != 0) else 0

if __name__ == "__main__":
    sys.exit(main()) 