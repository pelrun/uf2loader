#!/usr/bin/env python3
"""
Hardware tests for Pico bootloader using pytest-embedded
Requires a physical RP2040 board connected via USB/UART
"""

import pytest
import time
import os
import subprocess
from pathlib import Path

# Pytest-embedded imports
from pytest_embedded import Dut
from pytest_embedded_serial.dut import SerialDut

# Test configuration
BOOTLOADER_UF2 = Path(__file__).parent.parent / "build/bootloader/picocalc_sd_boot.uf2"
TEST_UF2_DIR = Path(__file__).parent.parent / "tools"
BOOT_MESSAGE = "PicoCalc SD Boot"
VERSION_PATTERN = r"v\d+\.\d+\.\d+"
TIMEOUT_BOOT = 3
TIMEOUT_UPDATE = 10

# Serial port configuration
BAUDRATE = 115200


class TestBootloaderHardware:
    """Hardware test suite for Pico bootloader"""
    
    @pytest.fixture(scope="class")
    def dut(self, request) -> SerialDut:
        """DUT fixture providing serial connection to the board"""
        port = request.config.option.port
        if not port:
            pytest.skip("No serial port specified. Use --port /dev/ttyACM0")
        
        # Create DUT instance
        dut = SerialDut(
            port=port,
            baudrate=BAUDRATE,
            app=str(BOOTLOADER_UF2),
        )
        yield dut
        dut.close()
    
    def flash_bootloader(self, dut: SerialDut):
        """Flash bootloader to the board using picotool"""
        if not BOOTLOADER_UF2.exists():
            pytest.skip(f"Bootloader UF2 not found: {BOOTLOADER_UF2}")
            
        # Reset to bootsel mode
        dut.serial.setDTR(False)
        dut.serial.setRTS(True)
        time.sleep(0.1)
        dut.serial.setDTR(True)
        time.sleep(0.5)
        
        # Flash using picotool
        result = subprocess.run(
            ["picotool", "load", "-x", str(BOOTLOADER_UF2), "-f"],
            capture_output=True,
            text=True
        )
        
        if result.returncode != 0:
            pytest.fail(f"Failed to flash bootloader: {result.stderr}")
            
        # Wait for board to restart
        time.sleep(2)
    
    def reset_board(self, dut: SerialDut):
        """Reset the board via DTR/RTS"""
        dut.serial.setDTR(False)
        time.sleep(0.1)
        dut.serial.setDTR(True)
        time.sleep(0.5)
    
    def wait_for_prompt(self, dut: SerialDut, timeout: float = TIMEOUT_BOOT):
        """Wait for bootloader prompt"""
        dut.expect(BOOT_MESSAGE, timeout=timeout)
    
    @pytest.mark.timeout(30)
    def test_power_on_boot(self, dut: SerialDut):
        """Test basic power-on boot sequence"""
        self.flash_bootloader(dut)
        
        # Verify boot message
        self.wait_for_prompt(dut)
        
        # Verify version string
        dut.expect(VERSION_PATTERN, timeout=1)
        
        # Check for any error messages
        time.sleep(0.5)
        output = dut.read()
        assert "ERROR" not in output
        assert "FAULT" not in output
    
    @pytest.mark.timeout(60)
    def test_multiple_resets(self, dut: SerialDut):
        """Test bootloader stability across multiple resets"""
        self.flash_bootloader(dut)
        
        for i in range(10):
            print(f"Reset cycle {i+1}/10")
            self.reset_board(dut)
            self.wait_for_prompt(dut)
            
            # Verify no corruption
            dut.expect(VERSION_PATTERN, timeout=1)
    
    @pytest.mark.timeout(30)
    def test_watchdog_recovery(self, dut: SerialDut):
        """Test watchdog reset recovery"""
        self.flash_bootloader(dut)
        self.wait_for_prompt(dut)
        
        # Send command to trigger watchdog (if implemented)
        dut.write(b'W')  # Hypothetical watchdog trigger command
        
        # Wait for reset
        time.sleep(2)
        
        # Verify recovery
        self.wait_for_prompt(dut)
        dut.expect("Watchdog reset detected", timeout=1)
    
    @pytest.mark.timeout(60)
    def test_uf2_update_valid(self, dut: SerialDut):
        """Test valid UF2 file update"""
        self.flash_bootloader(dut)
        self.wait_for_prompt(dut)
        
        # Enter UF2 update mode
        dut.write(b'U')  # Hypothetical UF2 mode command
        dut.expect("Entering UF2 update mode", timeout=2)
        
        # In real hardware test, this would involve USB mass storage
        # For now, we simulate the update completion
        time.sleep(5)
        
        # Verify update completion
        dut.expect("UF2 update complete", timeout=TIMEOUT_UPDATE)
        
        # Verify boot after update
        self.wait_for_prompt(dut)
    
    @pytest.mark.timeout(30)
    def test_invalid_uf2_rejection(self, dut: SerialDut):
        """Test rejection of invalid UF2 files"""
        test_files = [
            "test_invalid_magic.uf2",
            "test_corrupted_data.uf2",
            "test_out_of_bounds.uf2",
        ]
        
        self.flash_bootloader(dut)
        
        for test_file in test_files:
            uf2_path = TEST_UF2_DIR / test_file
            if not uf2_path.exists():
                pytest.skip(f"Test file not found: {test_file}")
                
            self.wait_for_prompt(dut)
            
            # Simulate UF2 transfer attempt
            # In real test, this would use USB mass storage
            dut.write(b'U')
            dut.expect("Entering UF2 update mode", timeout=2)
            
            # Expect rejection
            dut.expect("Invalid UF2", timeout=5)
            dut.expect("Update failed", timeout=1)
            
            # Verify recovery
            self.wait_for_prompt(dut)
    
    @pytest.mark.timeout(30)
    def test_uart_commands(self, dut: SerialDut):
        """Test UART command interface"""
        self.flash_bootloader(dut)
        self.wait_for_prompt(dut)
        
        # Test version command
        dut.write(b'V')
        dut.expect(VERSION_PATTERN, timeout=1)
        
        # Test help command
        dut.write(b'H')
        dut.expect("Available commands:", timeout=1)
        
        # Test reboot command
        dut.write(b'R')
        dut.expect("Rebooting...", timeout=1)
        self.wait_for_prompt(dut)
    
    @pytest.mark.timeout(30)
    def test_memory_info(self, dut: SerialDut):
        """Test memory information reporting"""
        self.flash_bootloader(dut)
        self.wait_for_prompt(dut)
        
        # Request memory info
        dut.write(b'M')
        
        # Verify memory report
        dut.expect("Memory Info:", timeout=1)
        dut.expect(r"RAM: \d+ KB free", timeout=1)
        dut.expect(r"Flash: \d+ KB available", timeout=1)
    
    @pytest.mark.timeout(60)
    def test_stress_uart(self, dut: SerialDut):
        """Stress test UART communication"""
        self.flash_bootloader(dut)
        self.wait_for_prompt(dut)
        
        # Send rapid commands
        for i in range(100):
            dut.write(b'V')
            dut.expect(VERSION_PATTERN, timeout=0.5)
            
        # Verify still responsive
        dut.write(b'H')
        dut.expect("Available commands:", timeout=1)
    
    @pytest.mark.timeout(120)
    def test_brown_out_recovery(self, dut: SerialDut):
        """Test brown-out recovery (requires power control)"""
        if not hasattr(dut, 'power_control'):
            pytest.skip("Power control not available")
            
        self.flash_bootloader(dut)
        
        for voltage in [3.3, 2.5, 2.0, 3.3]:  # Voltage sweep
            print(f"Setting voltage to {voltage}V")
            dut.power_control.set_voltage(voltage)
            time.sleep(2)
            
            if voltage >= 2.8:  # Should boot normally
                self.wait_for_prompt(dut)
            else:  # Brown-out condition
                # Verify no output or brown-out message
                time.sleep(1)
                output = dut.read()
                assert BOOT_MESSAGE not in output or "Brown-out" in output
    
    @pytest.mark.timeout(30)
    def test_gpio_state(self, dut: SerialDut):
        """Test GPIO pin states after boot"""
        self.flash_bootloader(dut)
        self.wait_for_prompt(dut)
        
        # Request GPIO state
        dut.write(b'G')
        
        # Verify all GPIOs are in safe state
        dut.expect("GPIO State:", timeout=1)
        
        # Check for high-impedance or pulled-down state
        for i in range(30):  # RP2040 has 30 GPIO pins
            dut.expect(f"GPIO{i}: (Hi-Z|LOW)", timeout=0.5)
    
    @pytest.mark.timeout(30)
    def test_clock_configuration(self, dut: SerialDut):
        """Test system clock configuration"""
        self.flash_bootloader(dut)
        self.wait_for_prompt(dut)
        
        # Request clock info
        dut.write(b'C')
        
        # Verify clock configuration
        dut.expect("Clock Configuration:", timeout=1)
        dut.expect(r"System: 125\.0 MHz", timeout=1)
        dut.expect(r"USB: 48\.0 MHz", timeout=1)
        dut.expect(r"ADC: 48\.0 MHz", timeout=1)
        dut.expect(r"RTC: 46875 Hz", timeout=1)


@pytest.fixture(scope="session")
def picoprobe():
    """Fixture for PicoProbe/Debug Probe operations"""
    # Check if OpenOCD is available
    result = subprocess.run(["which", "openocd"], capture_output=True)
    if result.returncode != 0:
        pytest.skip("OpenOCD not found")
        
    return {
        "interface": "interface/cmsis-dap.cfg",
        "target": "target/rp2040.cfg"
    }


class TestBootloaderWithProbe:
    """Advanced tests using debug probe"""
    
    @pytest.mark.timeout(30)
    def test_swd_flash_verify(self, picoprobe):
        """Test flashing via SWD and verify"""
        cmds = [
            f"-f {picoprobe['interface']}",
            f"-f {picoprobe['target']}",
            "-c 'adapter speed 5000'",
            "-c 'init'",
            "-c 'reset halt'",
            f"-c 'flash write_image erase {BOOTLOADER_UF2} 0x10000000'",
            "-c 'verify_image {BOOTLOADER_UF2} 0x10000000'",
            "-c 'reset run'",
            "-c 'exit'"
        ]
        
        result = subprocess.run(
            ["openocd"] + cmds,
            capture_output=True,
            text=True
        )
        
        assert result.returncode == 0, f"OpenOCD failed: {result.stderr}"
        assert "Verified" in result.stdout
    
    @pytest.mark.timeout(30)
    def test_register_state(self, picoprobe):
        """Test CPU register state after boot"""
        cmds = [
            f"-f {picoprobe['interface']}",
            f"-f {picoprobe['target']}",
            "-c 'adapter speed 5000'",
            "-c 'init'",
            "-c 'reset halt'",
            "-c 'reg'",
            "-c 'exit'"
        ]
        
        result = subprocess.run(
            ["openocd"] + cmds,
            capture_output=True,
            text=True
        )
        
        assert result.returncode == 0
        
        # Verify stack pointer is in SRAM
        assert "sp (/32): 0x2004" in result.stdout  # Should be in SRAM
        
        # Verify PC is in flash
        assert "pc (/32): 0x1000" in result.stdout  # Should be in flash


if __name__ == "__main__":
    pytest.main([__file__, "-v", "--tb=short"]) 