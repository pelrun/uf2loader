#include <stdint.h>
#include "hardware/flash.h"

#define JEDEC_READ_ID 0x9F
#define PICOCALC_BL_MAGIC 0xe98cc638

// Get the size of the available application space on the flash device
// TODO: read the current partition size instead if running on an RP2350
uint32_t GetFlashSize(void)
{
    uint32_t flash_size;

    uint8_t txbuf[4] = {JEDEC_READ_ID};
    uint8_t rxbuf[4] = {0};

    disable_interrupts_pico();
    flash_do_cmd(txbuf, rxbuf, 4);
    flash_size = 1 << rxbuf[3];
    enable_interrupts_pico();

    uint32_t *picocalc_bl_info = (uint32_t*)(XIP_BASE + flash_size - sizeof(uint32_t)*2);

    Option.flashmagic = picocalc_bl_info[0];
    Option.flashend = picocalc_bl_info[1];

    if (picocalc_bl_info[0] == PICOCALC_BL_MAGIC)
    {
        // picocalc highmem uf2 loader detected; return the size of the application area
        return picocalc_bl_info[1] - XIP_BASE;
    }

    return flash_size;
}
