*** Settings ***
Library           OperatingSystem
Library           Collections
Library           String
Suite Setup       Setup
Suite Teardown    Teardown
Test Setup        Reset Emulation
Test Teardown     Test Teardown

*** Variables ***
${UART}                       sysbus.uart0
${BOOTLOADER_BIN}            ${CURDIR}/../build/bootloader/picocalc_sd_boot.bin
${BOOTLOADER_ELF}            ${CURDIR}/../build/bootloader/picocalc_sd_boot.elf
${BOOTLOADER_UF2}            ${CURDIR}/../build/bootloader/picocalc_sd_boot.uf2
${TEST_UF2}                  ${CURDIR}/../tools/test_multiblock.uf2
${BOOT_TIMEOUT}              3
${UPDATE_TIMEOUT}            10
${BOOT_MESSAGE}              PicoCalc SD Boot
${VERSION_PATTERN}           v\\d+\\.\\d+\\.\\d+

*** Keywords ***
Setup
    Create Machine RP2040
    
Create Machine RP2040
    [Arguments]    ${name}=rp2040
    Execute Command    include @${CURDIR}/rp2040.resc
    Execute Command    machine LoadPlatformDescriptionFromString "cpu: CPU.CortexM @ sysbus { cpuType: \\"cortex-m0plus\\" }"
    
Load Bootloader
    [Arguments]    ${binary}=${BOOTLOADER_BIN}
    Execute Command    sysbus LoadBinary ${binary} 0x10000000
    Execute Command    cpu PC 0x10000000

Wait For Boot Message
    Wait For Line On UART    ${BOOT_MESSAGE}    timeout=${BOOT_TIMEOUT}
    
Wait For Version
    ${version_line}=    Wait For Line On UART    Version:    timeout=${BOOT_TIMEOUT}    treatAsRegex=true
    Should Match Regexp    ${version_line}    ${VERSION_PATTERN}

Reset Device
    Execute Command    machine Reset
    
Trigger Watchdog
    Execute Command    cpu SetRegisterUnsafe 14 0x40058000    # Set LR to watchdog address
    Execute Command    cpu SetRegisterUnsafe 0 0xDEADBEEF      # Trigger watchdog
    Execute Command    cpu Step 100
    
Simulate Power Loss
    Execute Command    machine Pause
    Sleep    0.5s
    Execute Command    machine Reset
    Execute Command    machine Start
    
Upload UF2 File
    [Arguments]    ${uf2_file}
    # Simulate USB mass storage device enumeration and file copy
    Execute Command    sysbus.usb AttachDevice ${uf2_file}
    Wait For Line On UART    UF2 download started    timeout=${UPDATE_TIMEOUT}
    
Verify Flash Contents
    [Arguments]    ${expected_pattern}
    ${flash_data}=    Execute Command    sysbus.flash ReadBytes 0x10000000 256
    Should Contain    ${flash_data}    ${expected_pattern}

Test Teardown
    ${log}=    Execute Command    sysbus.uart0 GetTransmittedData
    Log    UART Output:\n${log}

*** Test Cases ***
Test Power On Boot
    [Documentation]    Verify bootloader starts correctly on power-on
    [Tags]    smoke    power-on
    Load Bootloader
    Execute Command    machine Start
    Wait For Boot Message
    Wait For Version
    
Test Multiple Resets
    [Documentation]    Verify bootloader handles multiple resets gracefully
    [Tags]    reliability    reset
    Load Bootloader
    Execute Command    machine Start
    FOR    ${i}    IN RANGE    5
        Wait For Boot Message
        Reset Device
    END
    
Test Watchdog Recovery
    [Documentation]    Verify bootloader recovers from watchdog reset
    [Tags]    safety    watchdog
    Load Bootloader
    Execute Command    machine Start
    Wait For Boot Message
    Trigger Watchdog
    Wait For Boot Message
    Wait For Line On UART    Watchdog reset detected
    
Test Brown Out Recovery
    [Documentation]    Verify bootloader handles brown-out conditions
    [Tags]    safety    power
    Load Bootloader
    Execute Command    machine Start
    Wait For Boot Message
    Simulate Power Loss
    Wait For Boot Message
    
Test UF2 Download Basic
    [Documentation]    Verify basic UF2 file download and flash
    [Tags]    update    uf2
    Load Bootloader
    Execute Command    machine Start
    Wait For Boot Message
    Upload UF2 File    ${TEST_UF2}
    Wait For Line On UART    UF2 download complete
    
Test Invalid UF2 Rejection
    [Documentation]    Verify bootloader rejects corrupted UF2 files
    [Tags]    safety    uf2    security
    Load Bootloader
    Execute Command    machine Start
    Wait For Boot Message
    Upload UF2 File    ${CURDIR}/../tools/test_invalid_magic.uf2
    Wait For Line On UART    Invalid UF2 magic
    Wait For Line On UART    Update failed
    
Test Partial UF2 Recovery
    [Documentation]    Verify bootloader handles incomplete UF2 transfers
    [Tags]    safety    uf2    recovery
    Load Bootloader
    Execute Command    machine Start
    Wait For Boot Message
    # Simulate interrupted transfer
    Execute Command    sysbus.usb StartTransfer ${TEST_UF2}
    Sleep    1s
    Execute Command    sysbus.usb AbortTransfer
    Wait For Line On UART    Transfer timeout
    Wait For Boot Message    # Should recover to bootloader
    
Test Flash Protection
    [Documentation]    Verify bootloader protects its own flash region
    [Tags]    security    flash
    Load Bootloader
    Execute Command    machine Start
    Wait For Boot Message
    # Try to write to bootloader region
    Execute Command    sysbus.flash WriteBytes 0x10000000 "DEADBEEF"
    Wait For Line On UART    Flash write protected
    
Test Command Interface
    [Documentation]    Test bootloader command handling
    [Tags]    interface    commands
    Load Bootloader
    Execute Command    machine Start
    Wait For Boot Message
    # Send version command
    Execute Command    uart0 WriteChar 'V'
    Wait For Version
    # Send reboot command
    Execute Command    uart0 WriteChar 'R'
    Wait For Line On UART    Rebooting...
    Wait For Boot Message
    
Test Memory Exhaustion
    [Documentation]    Verify bootloader handles memory exhaustion gracefully
    [Tags]    safety    memory
    Load Bootloader
    Execute Command    machine Start
    Wait For Boot Message
    # Simulate large UF2 that exhausts memory
    Upload UF2 File    ${CURDIR}/../tools/test_oversized.uf2
    Wait For Line On UART    Out of memory
    Wait For Boot Message    # Should recover

Test GPIO State After Boot
    [Documentation]    Verify GPIO pins are in safe state after boot
    [Tags]    safety    gpio
    Load Bootloader
    Execute Command    machine Start
    Wait For Boot Message
    ${gpio_state}=    Execute Command    sysbus.gpio0 Read
    Should Be Equal As Numbers    ${gpio_state}    0x00000000
    
Test Clock Configuration
    [Documentation]    Verify system clocks are configured correctly
    [Tags]    system    clocks
    Load Bootloader
    Execute Command    machine Start
    Wait For Boot Message
    ${sys_clk}=    Execute Command    cpu GetSystemClockFrequency
    Should Be Equal As Numbers    ${sys_clk}    125000000    # 125MHz expected
    
Test DMA Safety
    [Documentation]    Verify DMA channels are properly reset
    [Tags]    safety    dma
    Load Bootloader
    Execute Command    machine Start
    Wait For Boot Message
    ${dma_status}=    Execute Command    sysbus.dma GetChannelStatus 0
    Should Be Equal    ${dma_status}    IDLE
    
Test Stack Overflow Protection
    [Documentation]    Verify stack overflow detection works
    [Tags]    safety    stack
    Load Bootloader
    Execute Command    machine Start
    Wait For Boot Message
    # Force stack overflow
    Execute Command    cpu SP 0x20000000    # Set stack pointer to start of RAM
    Execute Command    cpu Step 1000
    Wait For Line On UART    Stack overflow detected 