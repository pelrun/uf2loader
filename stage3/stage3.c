#include <pico.h>
#include <pico/time.h>
#include "pico/bootrom.h"
#include "boot/picobin.h"
#include "hardware/flash.h"

#include "i2ckbd.h"
#include "proginfo.h"

#include "pff.h"
#include "uf2.h"

#include "debug.h"

void _Noreturn infinite_loop(void)
{
  while (1)
  {
    tight_loop_contents();
  }
}

#if PICO_RP2040

#include "hardware/regs/m0plus.h"

#define LOADER "BOOT2040.UF2"

// This function jumps to the application entry point
// It must update the vector table and stack pointer before jumping
void _Noreturn launch_application_from(uint32_t *app_location)
{
  // https://vanhunteradams.com/Pico/Bootloader/Bootloader.html
  uint32_t *new_vector_table = app_location;
  volatile uint32_t *vtor    = (uint32_t *)(PPB_BASE + M0PLUS_VTOR_OFFSET);
  *vtor                      = (uint32_t)new_vector_table;
  asm volatile(
      "msr msp, %0\n"
      "bx %1\n"
      :
      : "r"(new_vector_table[0]), "r"(new_vector_table[1])
      :);

  infinite_loop();
}

void launch_application(void)
{
  if (bl_proginfo_valid())
  {
#if ENABLE_DEBUG
    stdio_deinit_all();
#endif
    launch_application_from((void *)XIP_BASE + 0x100);
  }
}

void launch_application_from_ram(void)
{
#if ENABLE_DEBUG
  stdio_deinit_all();
#endif
  launch_application_from((void *)(SRAM_BASE + 0x100));
}

#elif PICO_RP2350

#define LOADER "BOOT2350.UF2"

uint8_t __attribute__((aligned(4))) workarea[4 * 1024];

uintptr_t app_start_offset = 0;
uint32_t app_size          = 0;

void launch_application(void)
{
  {
#if ENABLE_DEBUG
    stdio_deinit_all();
#endif
    rom_chain_image(workarea, sizeof(workarea), (XIP_BASE + app_start_offset), app_size);
  }
}

void launch_application_from_ram(void)
{
  rom_chain_image(workarea, sizeof(workarea), SRAM_BASE, 0x1000);
}

#endif

enum bootmode_e
{
  BOOT_DEFAULT,
  BOOT_SD,
  BOOT_UPDATE,
};

enum bootmode_e read_bootmode()
{
  // TODO: if keyboard isn't available, return BOOT_UPDATE
  // so we can just do bootsel without having to turn the picocalc on

  init_i2c_kbd();

  int key = read_i2c_kbd();
  if (key == 0)
  {
    return BOOT_DEFAULT;
  }

  switch (key)
  {
    case 0xb5:  // Arrow Up
      return BOOT_SD;
    case 0xb6:  // Arrow Down
      return BOOT_UPDATE;
    default:
      break;
  }

  return BOOT_DEFAULT;
}

int main()
{
#ifdef ENABLE_DEBUG
  stdio_init_all();
#endif

// FIXME: check SD card insert?

  enum bootmode_e mode = read_bootmode();

#if PICO_RP2350
  if (!bl_app_partition_get_info(workarea, sizeof(workarea), &app_start_offset, &app_size))
  {
    // Can't function without a partition table, so force drop into BOOTSEL
    mode = BOOT_UPDATE;
  }
#endif

  if (mode == BOOT_UPDATE)
  {
    DEBUG_PRINT("Entering BOOTSEL mode\n");
    reset_usb_boot(0, 0);
  }

  // Boot application from flash
  if (mode == BOOT_DEFAULT)
  {
    DEBUG_PRINT("Boot application from flash\n");
    launch_application();
  }

  // Load UI from SD card
  DEBUG_PRINT("Loading UI\n");

  FATFS fs;
  FRESULT fr = FR_NOT_READY;

  // seems to take a couple of attempts from cold start
  for (int retry = 5; retry > 0; retry--)
  {
    fr = pf_mount(&fs);

    if (fr == FR_OK)
    {
      break;
    }

    sleep_ms(500);
  }

  if (fr == FR_OK)
  {
    if (load_application_from_uf2(LOADER))
    {
      DEBUG_PRINT("Launch UI\n");
      // launch ui app now in ram
      launch_application_from_ram();
    }
  }

  // fall back to application
  DEBUG_PRINT("Failed to run UI, boot flash\n");
  launch_application();

  // boot failure
  infinite_loop();
}
