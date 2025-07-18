#ifndef BOOT_DEFS_H
#define BOOT_DEFS_H

#ifdef __cplusplus
extern "C" {
#endif

// Flash memory layout constants (RP2040)
#define FLASH_PAGE_SIZE      (1u << 8)   // 256 bytes per page
#define FLASH_SECTOR_SIZE    (1u << 12)  // 4096 bytes per sector

// Size of the 2nd-stage bootloader region that must remain untouched
#define BOOT2_SIZE           256u

// UF2 file format magic numbers
#define UF2_MAGIC_START0     0x0A324655u // "UF2\n"
#define UF2_MAGIC_START1     0x9E5D5157u // Randomly chosen
#define UF2_MAGIC_END        0x0AB16F30u

#ifdef __cplusplus
} // extern "C"
#endif

#endif // BOOT_DEFS_H 