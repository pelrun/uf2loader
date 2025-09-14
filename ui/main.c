/**
 * PicoCalc UF2 Firmware Loader
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

#include "hardware/gpio.h"
#include "debug.h"
#include "i2ckbd.h"
#include "lcdspi.h"
#include <hardware/flash.h>
#include <hardware/watchdog.h>
#include "config.h"

#include "ff.h"

#if ENABLE_USB
#include "usb_msc.h"
#include "tusb.h"
#endif

#include "text_directory_ui.h"
#include "key_event.h"

#include "proginfo.h"
#include "uf2.h"
#include "ui.h"

FATFS fat;

#define DEBOUNCE_LIMIT 25

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

void fs_deinit(void) { f_unmount("/"); }

bool fs_init(void)
{
  DEBUG_PRINT("fs init SD\n");
  FRESULT res = f_mount(&fat, "/", 1);
  if (res != FR_OK)
  {
    DEBUG_PRINT("mount err: %s\n", res);
    fs_deinit();
    return false;
  }

  return true;
}

void reboot(void) {
#if ENABLE_USB
  usb_msc_init();
#endif
  fs_deinit();

  watchdog_reboot(0, 0, 0);
}

void load_firmware_by_path(const char *path)
{
  text_directory_ui_set_status("Loading app...");

  // Attempt to load the application from the SD card
  enum uf2_result_e result = load_application_from_uf2(path);

  switch (result)
  {
    case UF2_LOADED:
      text_directory_ui_set_status("Launching...");
      DEBUG_PRINT("launching app\n");
      // trigger a reboot - stage3 will launch the app
      reboot();
      break;
    case UF2_WRONG_PLATFORM:
      text_directory_ui_set_status("ERR: Not for this device");
      DEBUG_PRINT("wrong platform\n");
      break;
    case UF2_BAD:
      text_directory_ui_set_status("ERR: Bad UF2 file");
      DEBUG_PRINT("bad uf2\n");
      break;
    default:
      text_directory_ui_set_status("ERR: Unexpected error");
      DEBUG_PRINT("unexpected error\n");
      break;
  }
}

void final_selection_callback(const char *path)
{
  char status_message[128];
  const char *extension = ".uf2";

  if (path == NULL)
  {
    // Reboot into current app
    reboot();
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

#if PICO_RP2350
  {
    uint8_t __attribute__((aligned(4))) workarea[4 * 1024];
    uintptr_t app_start_offset = 0;
    uint32_t app_size = 0;

    if (!bl_app_partition_get_info(workarea, sizeof(workarea), &app_start_offset, &app_size))
    {
      // FIXME: this should be an explicit infinite loop...
      return false;
    }
    // After this 0x10000000 should be remapped to the start of the app partition
    bl_remap_flash(app_start_offset, app_size);
  }
#endif

  lcd_init();

  // Initialize filesystem
  if (!fs_init())
  {
    text_directory_ui_set_status("Failed to mount SD card!");
    DEBUG_PRINT("Failed to mount SD card\n");
    sleep_ms(2000);
    reboot();
  }

#if ENABLE_USB
  usb_msc_init();
#endif

  text_directory_ui_init();
  text_directory_ui_set_final_callback(final_selection_callback);

  keypad_init();

  while (keypad_get_key() > 0)
  {
    // drain the keypad input buffer
  }

  text_directory_ui_update_title();
  while (true)
  {
    text_directory_ui_run();
#if ENABLE_USB
    tud_task();
#endif
  }
}
