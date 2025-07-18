# Picocalc SD Bootloader Test Matrix

This document outlines the system-level test plan for the Picocalc SD Bootloader, ensuring its reliability and compatibility across the RP2040 and RP2350 microcontrollers.

## 1. Test Platforms

| Platform      | Board       | Key Features Tested     |
|---------------|-------------|-------------------------|
| **RP2040**    | Pico        | Standard memory layout, JEDEC flash detection, core bootloader functionality. |
| **RP2350**    | Pico 2 W    | High-memory layout (ATU), partitioned flash, Wi-Fi coexistence (if applicable), core bootloader functionality. |

## 2. Test Scenarios

### 2.1. SD Card Compatibility

| Test Case                | Description                                                                    | Expected Result                                                                                     |
|--------------------------|--------------------------------------------------------------------------------|-----------------------------------------------------------------------------------------------------|
| **SDHC Card (FAT32)**    | Test with a standard SDHC card formatted with FAT32.                           | The bootloader successfully mounts the card, lists `.uf2` files, and flashes a selected firmware image. |
| **SDXC Card (exFAT)**    | Test with a high-capacity SDXC card formatted with exFAT.                        | The bootloader successfully mounts the card, lists `.uf2` files, and flashes a selected firmware image. |
| **High-Speed Mode**      | Test with an SD card that supports high-speed mode (up to 50 MHz).               | The bootloader correctly negotiates and switches to the high-speed mode, improving firmware flashing speed. |
| **No SD Card**           | Boot the device without an SD card inserted.                                   | The UI displays "Error: SD card not found" and prompts the user to insert a card. The option to run the last-flashed application should be available. |
| **SD Card Removal/Insertion** | Remove and re-insert the SD card while the bootloader is running.          | The bootloader detects the change, gracefully handles the event, and successfully remounts the card, allowing normal operation to resume. |

### 2.2. Firmware Flashing

| Test Case                  | Description                                                                       | Expected Result                                                                                                    |
|----------------------------|-----------------------------------------------------------------------------------|--------------------------------------------------------------------------------------------------------------------|
| **Valid UF2 Image**        | Flash a valid `.uf2` firmware image for the corresponding platform (RP2040/RP2350). | The firmware is flashed successfully, and the device boots into the new application. The UI shows the new app as the "last-flashed" option. |
| **Corrupted UF2 Image**    | Attempt to flash a corrupted or incomplete `.uf2` file.                           | The bootloader's CRC32 check detects the corruption and displays an error message. The flashing process is aborted, and no changes are made to the existing application. |
| **Incorrect Platform Image** | Attempt to flash an RP2040 image on an RP2350, and vice-versa.                   | The bootloader's UF2 parser should ideally reject the image based on the family ID. If not, the application will likely fail to boot, but the bootloader itself should remain functional. |
| **Large Firmware Image**   | Flash a firmware image that is close to the maximum allowed application size.     | The bootloader successfully flashes the entire image without errors.                                             |

### 2.3. User Interface

| Test Case                     | Description                                                                      | Expected Result                                                                                                |
|-------------------------------|----------------------------------------------------------------------------------|----------------------------------------------------------------------------------------------------------------|
| **Directory Navigation**      | Navigate through nested directories on the SD card.                              | The UI correctly displays the contents of each directory, and the user can navigate up and down the hierarchy. |
| **Long Filenames**            | Test with filenames that exceed the display width.                               | The UI correctly scrolls the selected long filename, ensuring full visibility.                                 |
| **Empty Directory**           | Navigate to an empty directory (or the root of an empty SD card).                | The UI displays a "No .uf2 files found" message.                                                               |
| **Run Last-Flashed App**      | Use the UI option to run the application that was last flashed to the device.    | The device successfully boots into the stored application.                                                     |

## 3. Test Execution and Sign-off

Each test case will be executed on both the RP2040 and RP2350 platforms. The results will be documented, and any failures will be investigated and resolved before the final release.

| Test Case ID | Platform | Result (Pass/Fail) | Notes |
|--------------|----------|--------------------|-------|
| SC-1.1       | RP2040   |                    |       |
| SC-1.1       | RP2350   |                    |       |
| SC-1.2       | RP2040   |                    |       |
| ...          | ...      |                    |       |

**Approved by:** _________________________

**Date:** _________________________ 