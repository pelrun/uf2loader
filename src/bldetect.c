#include <stdint.h>
#include "pico/bootrom.h"
#include "pico/unique_id.h"
#include "hardware/flash.h"

// Add this line to include the necessary header for the pico-sdk interrupt functions
#include "pico/sync.h"

#define JEDEC_READ_ID 0x9F
#define PICOCALC_BL_MAGIC 0xe98cc638
#define PARTITION_TABLE_SIZE 0x1000 // 4K for partition table
#define BOOT_INFO_OFFSET (PICO_FLASH_SIZE_BYTES - PARTITION_TABLE_SIZE)

// Get the size of the available application space on the flash device
uint32_t GetFlashSize(void)
{
#if BUILD_PICO2
    // On RP2350, read the partition table to find the size of the application partition.
    // The partition table is located in the last 4KB of flash.
    const uint8_t *boot_info_ptr = (const uint8_t *)(XIP_BASE + BOOT_INFO_OFFSET);

    // Simple validation of boot info magic
    if (*(uint32_t*)boot_info_ptr == 0x544f4f42) { // "BOOT"
        // Assuming the first partition is the application partition.
        // This is a simplification; a more robust implementation would parse the table.
        // Format: 4 bytes magic, 4 bytes CRC, then partitions.
        // Each partition: 1 byte type, 1 byte flags, 2 bytes reserved, 4 bytes offset, 4 bytes size.
        const uint8_t *first_partition = boot_info_ptr + 8;
        return *(uint32_t*)(first_partition + 8); // Size of the first partition
    }
    // Fallback if boot info is not valid
    return PICO_FLASH_SIZE_BYTES - 0x40000; // Default to Flash size - 256KB bootloader
#else
    // For RP2040, use the JEDEC command to get the total flash size.
    uint32_t flash_size;

    uint8_t txbuf[4] = {JEDEC_READ_ID};
    uint8_t rxbuf[4] = {0};

    // The pico-sdk's flash_do_cmd handles interrupts, so we don't need to disable them here.
    flash_do_cmd(txbuf, rxbuf, 4);
    flash_size = 1 << rxbuf[3];

    return flash_size;
#endif
}

uint32_t GetAppSize(void)
{
#if BUILD_PICO2
    return GetFlashSize();
#else
    uint32_t flash_size = GetFlashSize();
    uint32_t *picocalc_bl_info = (uint32_t*)(XIP_BASE + flash_size - sizeof(uint32_t)*2);

    if (picocalc_bl_info[0] == PICOCALC_BL_MAGIC)
    {
        // picocalc highmem uf2 loader detected; return the size of the application area
        return picocalc_bl_info[1] - XIP_BASE;
    }

    return flash_size - PICO_BOOT_STAGE2_SIZE;
#endif
}
