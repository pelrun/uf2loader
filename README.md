# Picocalc SD Bootloader

`Picocalc_SD_Boot` is a custom, high-memory bootloader for the Raspberry Pi Pico (RP2040) and Pico 2 W (RP2350). It allows you to load and flash `.uf2` firmware images directly from an SD card, providing a fast and convenient way to update your device without needing to connect it to a computer.

<div align="center">
    <img src="img/sd_boot.jpg" alt="Picocalc SD Bootloader in action" width="80%">
</div>

## Features

- **Dual-Platform Support:** Fully compatible with both the RP2040 (Pico) and the new RP2350 (Pico 2 W).
- **High-Memory Layout:** The bootloader resides in the upper portion of the flash memory, leaving the standard `0x10000000` address space free for your application.
- **RP2350 ATU Support:** Utilizes the RP2350's Address Translation Unit (ATU) to seamlessly map the application into the standard memory space.
- **Polished User Interface:** A clean, intuitive text-based UI for easy navigation and file selection, complete with a decorative frame and clear on-screen instructions.
- **Robust Flashing:** Features a hardened flash writer with CRC32 checks to ensure data integrity and prevent corrupted updates.
- **SD Card Flexibility:** Supports both SDHC and SDXC cards with FAT32 or exFAT filesystems, including a high-speed mode for faster flashing.

## Building the Bootloader

To build the bootloader, you will need the Pico SDK and a GCC ARM toolchain.

1.  **Clone the repository and initialize submodules:**
    ```bash
    git clone https://github.com/adwuard/Picocalc_SD_Boot.git
    cd Picocalc_SD_Boot
    git submodule update --init --recursive
    ```

2.  **Create a build directory:**
    ```bash
    cd src
    mkdir build && cd build
    ```

3.  **Configure the build for your target platform:**

    *   **For RP2040 (Pico):**
        ```bash
        cmake -D PICO_SDK_PATH=/path/to/pico-sdk ..
        ```

    *   **For RP2350 (Pico 2 W):**
        ```bash
        cmake -D PICO_SDK_PATH=/path/to/pico-sdk -D BUILD_PICO2=ON ..
        ```

4.  **Build the bootloader:**
    ```bash
    make
    ```
    The compiled `.uf2` file will be located in the `src/build/` directory.

## Technical Implementation

### Bootloader Detection and Size Management

The bootloader uses the last 8 bytes of flash to store a magic number (`0xe98cc638`) and the bootloader's start address. This allows applications to detect the presence of the bootloader at runtime and determine the available application space, preventing accidental corruption.

### Flash Update Mechanism

The flash update process is designed for safety and reliability:
- The bootloader itself is never overwritten during an update.
- The application area is erased and programmed using the pico-sdk's `flash_range_erase` and `flash_range_program` functions.
- All data written to flash is verified with a CRC32 checksum to ensure integrity.

## Credits

- **Hiroyuki Oyama:** For the original SD card bootloader mechanism and VFS implementation.
- **TheKiwil:** For contributions to the RP2350 port, including the custom linker script.
- **muzkr:** For the initial high-memory bootloader implementation.

## Further Reading

- **Blog Post:** For a detailed technical write-up, see the [project blog page](https://hsuanhanlai.com/writting-custom-bootloader-for-RPI-Pico/).
- **Forum Discussion:** Join the conversation on the [Clockwork Pi Forum](https://forum.clockworkpi.com/t/i-made-an-app-that-dynamically-load-firmware-from-sd-card/16664/24).

## Local CI/CD Testing with `act`

You can run the full CI/CD pipeline locally using [act](https://github.com/nektos/act). This allows you to test your changes in an environment that mirrors the GitHub Actions runners.

1.  **Install `act`:** Follow the official [installation instructions](https://github.com/nektos/act#installation).

2.  **Build the custom Docker image:**
    ```bash
    docker build -t picocalc-boot-env:latest -f docker/Dockerfile .
    ```

3.  **Run the `emulation-test` job:**
    ```bash
    act -j emulation-test --container-architecture linux/amd64 -P ubuntu-latest=picocalc-boot-env:latest
    ```
    This command will execute the `emulation-test` job defined in `.github/workflows/build.yml` using your local Docker image.
