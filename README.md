# Picocalc_SD_Boot

`Picocalc_SD_Boot` is a custom bootloader for the Raspberry Pi Pico. This bootloader provides the functionality to load and execute applications from an SD card, designed to enable PicoCalc to load firmware to the Pico using an SD card easily.

<div align="center">
    <img src="img/sd_boot.jpg" alt="sdboot" width="80%">
</div>

## 🚧 Improvement Plans
work in progress plans [Feature Request Post](https://forum.clockworkpi.com/t/i-made-an-app-that-dynamically-load-firmware-from-sd-card/16664/25?u=adwuard)
- [ ] Add transparent app support for Pico 2 [Address Translation (see page 364 of the RP2350 datasheet, section 5.1.19)](https://datasheets.raspberrypi.com/rp2350/rp2350-datasheet.pdf)
- [ ] USB Mass Storage mode for SD card. [Related Demo Code](https://github.com/hathach/tinyusb/tree/master/examples/device/cdc_msc/src)


## Bootloader Build From Scratch
Clone the source code and initialize the submodules.

```bash
git clone https://github.com/adwuard/Picocalc_SD_Boot.git
cd Picocalc_SD_Boot
git submodule update --init --recursive
```

Build the bootloader.

```bash
cd ./src
mkdir build; cd build
PICO_SDK_PATH=/path/to/pico-sdk cmake ..
make
```

## Technical Implementation Notes
### Bootloader detection/size management
As the RP2040 does not have a mechanism for write protecting flash regions, the bootloader can be accidentally corrupted by the application if it writes to the same area. Devs can add the ability to detect the bootloader and it's size at runtime and therefore know exactly how much flash is available to be used.

To do this, read the 8 bytes at the very end of the flash area. This consists of a 32-bit magic number (`0xe98cc638`) at XIP_BASE+0x1ffff8, and the start address of the bootloader at XIP_BASE+0x1ffffc. The application is free to write anywhere below this address.

### Flash Update Mechanism
The bootloader implements a safe update mechanism with the following features:

- The bootloader itself resides at the top of the flash area and is never overwritten during updates
- Only the application region of flash (starting at 256b) is updated using `flash_range_erase` and `flash_range_program`

## Credits
- [Hiroyuki Oyama](https://github.com/oyama/pico-sdcard-boot): Special thanks for the firmware loader mechanism and VFS file system.
  - https://github.com/oyama/pico-sdcard-boot
  - https://github.com/oyama/pico-vfs
- [TheKiwil](https://github.com/TheKiwil/): Special thanks for contributions on supporting pico2 boards with new custom linker script.
- [muzkr](https://github.com/muzkr/hachi/): Special thanks for the initial boot2/high-mem implementation

## Read More
- Blog on this repository, and more technical detail about bootloader. -->[Blog Page](https://hsuanhanlai.com/writting-custom-bootloader-for-RPI-Pico/)
- Fourm Page and Discussion: [Clockwork Pi Fourm](https://forum.clockworkpi.com/t/i-made-an-app-that-dynamically-load-firmware-from-sd-card/16664/24)
