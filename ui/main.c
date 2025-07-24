/**
 * PicoCalc SD Firmware Loader
 *
 * Originally by : Hsuan Han Lai
 * Email: hsuan.han.lai@gmail.com
 * Website: https://hsuanhanlai.com
 * Year: 2025
 *
 *
 * This project is a bootloader for the PicoCalc device, designed to load and execute
 * firmware applications from an SD card.
 *
 */

#include <pico.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "pico/bootrom.h"
#include "boot/picobin.h"
#include "hardware/gpio.h"
#include "debug.h"
#include "lcdspi.h"
#include <hardware/flash.h>
#include <hardware/watchdog.h>
#include "config.h"

#include "blockdevice/sd.h"
#include "filesystem/fat.h"
#include "filesystem/vfs.h"
#include "text_directory_ui.h"
#include "key_event.h"

#include "proginfo.h"
#include "uf2.h"

#ifdef __clang__
// make this a no-op in the IDE only
#error This should not be compiled!
#undef __not_in_flash_func
#define __not_in_flash_func(x) x
#endif

// Vector and RAM offset
#if PICO_RP2040
#define VTOR_OFFSET M0PLUS_VTOR_OFFSET
#define MAX_RAM 0x20040000
#elif PICO_RP2350
#define VTOR_OFFSET M33_VTOR_OFFSET
#define MAX_RAM 0x20080000
uint8_t __attribute__((aligned(4))) workarea[4 * 1024];
#endif

#define DEBOUNCE_LIMIT 50

// SD card guaranteed present on initial startup
bool sd_insert_state = true;

bool sd_card_inserted(void)
{
  static uint32_t debounce = 0;

  bool new_state = gpio_get(SD_DET_PIN);

  if (!new_state)
  {
    debounce = 0;
    sd_insert_state = false;
  }
  else if (!sd_insert_state && new_state)
  {
    // FIXME: should just use system time instead of a loop count
    if (debounce >= DEBOUNCE_LIMIT)
    {
      sd_insert_state = true;
    }
    else
    {
      debounce++;
    }
  }

  return sd_insert_state;
}

blockdevice_t *sd = NULL;
filesystem_t *fat = NULL;

bool fs_init(void)
{
  DEBUG_PRINT("fs init SD\n");
  sd = blockdevice_sd_create(spi0, SD_MOSI_PIN, SD_MISO_PIN, SD_SCLK_PIN, SD_CS_PIN,
                                            125000000 / 2 / 4,  // 15.6MHz
                                            true);
  fat = filesystem_fat_create();
  int err = fs_mount("/", fat, sd);
  if (err == -1)
  {
    DEBUG_PRINT("mount err: %s\n", strerror(errno));
    return false;
  }
  return true;
}

void fs_deinit(void)
{
  fs_unmount("/");
  if (fat) {
    filesystem_fat_free(fat);
    fat = NULL;
  }
  if (sd)
  {
    blockdevice_sd_free(sd);
    sd = NULL;
  }
}

#if PICO_RP2040

// This function jumps to the application entry point
// It must update the vector table and stack pointer before jumping
void launch_application_from(uint32_t *app_location)
{
  // https://vanhunteradams.com/Pico/Bootloader/Bootloader.html
  uint32_t *new_vector_table = app_location;
  volatile uint32_t *vtor = (uint32_t *)(PPB_BASE + VTOR_OFFSET);
  *vtor = (uint32_t)new_vector_table;
  asm volatile(
      "msr msp, %0\n"
      "bx %1\n"
      :
      : "r"(new_vector_table[0]), "r"(new_vector_table[1])
      :);
}

int launch_application(void)
{
  if (bl_proginfo_valid())
  {
    stdio_deinit_all();
    launch_application_from((void *)XIP_BASE + 0x100);
  }
}

#elif PICO_RP2350

uint32_t app_start_addr = 0, app_size = 0;

bool get_app_partition_info(void)
{
  if (rom_load_partition_table(workarea, sizeof(workarea), false) != BOOTROM_OK)
  {
    return false;
  }

  uint32_t partition_info[3];

  if (rom_get_partition_table_info(
          partition_info, 3,
          PT_INFO_PARTITION_LOCATION_AND_FLAGS | PT_INFO_SINGLE_PARTITION | (0 << 24)) < 0)
  {
    return false;
  }

  uint16_t first_sector_number =
      (partition_info[1] & PICOBIN_PARTITION_LOCATION_FIRST_SECTOR_BITS) >>
      PICOBIN_PARTITION_LOCATION_FIRST_SECTOR_LSB;
  uint16_t last_sector_number = (partition_info[1] & PICOBIN_PARTITION_LOCATION_LAST_SECTOR_BITS) >>
                                PICOBIN_PARTITION_LOCATION_LAST_SECTOR_LSB;
  uint32_t app_end_addr = (last_sector_number + 1) * 0x1000;

  app_start_addr = first_sector_number * 0x1000;
  app_size = app_end_addr - app_start_addr;

  return true;
}

int launch_application(void)
{
  if (bl_proginfo_valid() && get_app_partition_info())
  {
    stdio_deinit_all();
    rom_chain_image(workarea, sizeof(workarea), (XIP_BASE + app_start_addr), app_size);
  }
}

#endif

int load_firmware_by_path(const char *path)
{
  text_directory_ui_set_status("Loading app...");

  // Attempt to load the application from the SD card
  bool load_success = load_application_from_uf2(path);

  if (load_success)
  {
    text_directory_ui_set_status("Launching app...");
    DEBUG_PRINT("launching app\n");
    // Small delay to allow printf to complete
    sleep_ms(100);
    launch_application();
  }
  else
  {
    text_directory_ui_set_status("ERR: No valid app");
    DEBUG_PRINT("no valid app, halting\n");

    sleep_ms(2000);

    // Trigger a watchdog reboot
    watchdog_reboot(0, 0, 0);
  }
}

void final_selection_callback(const char *path)
{
  char status_message[128];
  const char *extension = ".uf2";

  if (path == NULL)
  {
    // Run current app
    launch_application();
    return;
  }

  // Trigger firmware loading with the selected path
  DEBUG_PRINT("selected: %s\n", path);

  size_t path_len = strlen(path);
  size_t ext_len = strlen(extension);

  if (path_len < ext_len || strcmp(path + path_len - ext_len, extension) != 0)
  {
    DEBUG_PRINT("not a uf2: %s\n", path);
    snprintf(status_message, sizeof(status_message), "ERR: File must be .uf2");
    text_directory_ui_set_status(status_message);
    return;
  }

  snprintf(status_message, sizeof(status_message), "SEL: %s", path);
  text_directory_ui_set_status(status_message);

  sleep_ms(200);

  load_firmware_by_path(path);
}

int main()
{
  stdio_init_all();

  uart_init(uart0, 115200);
  uart_set_format(uart0, 8, 1, UART_PARITY_NONE);  // 8-N-1
  uart_set_fifo_enabled(uart0, false);

  // Initialize SD card detection pin
  gpio_init(SD_DET_PIN);
  gpio_set_dir(SD_DET_PIN, GPIO_IN);
  gpio_pull_up(SD_DET_PIN);  // Enable pull-up resistor
  gpio_set_inover(SD_DET_PIN, GPIO_OVERRIDE_INVERT);

  lcd_init();
  lcd_clear();
  //text_directory_ui_pre_init();

  // Initialize filesystem
  if (!fs_init())
  {
    text_directory_ui_set_status("Failed to mount SD card!");
    DEBUG_PRINT("Failed to mount SD card\n");
    sleep_ms(2000);
    watchdog_reboot(0, 0, 0);
  }

  text_directory_ui_init();
  text_directory_ui_set_final_callback(final_selection_callback);

  keypad_init();

  while (keypad_get_key() > 0)
  {
    // drain the keypad input buffer
  }

  text_directory_ui_run();
}
