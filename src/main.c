/**
 * PicoCalc SD Firmware Loader
 *
 * Author: Hsuan Han Lai
 * Email: hsuan.han.lai@gmail.com
 * Website: https://hsuanhanlai.com
 * Year: 2025
 *
 *
 * This project is a bootloader for the PicoCalc device, designed to load and execute
 * firmware applications from an SD card.
 *
 */

#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "pico/bootrom.h"
#include "hardware/gpio.h"
#include "debug.h"
#include "i2ckbd.h"
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


// Vector and RAM offset
#if PICO_RP2040
#define VTOR_OFFSET M0PLUS_VTOR_OFFSET
#define MAX_RAM 0x20040000
#elif PICO_RP2350
#error RP2350 is not currently supported
#define VTOR_OFFSET M33_VTOR_OFFSET
#define MAX_RAM 0x20080000
#endif

uint8_t status_flag;//0 no sdcard ,1 has sd card
bool sd_card_inserted(void)
{
    status_flag = !gpio_get(SD_DET_PIN);
    // Active low detection - returns true when pin is low
    return (bool)status_flag;
}

bool fs_init(void)
{
    DEBUG_PRINT("fs init SD\n");
    blockdevice_t *sd = blockdevice_sd_create(spi0,
                                              SD_MOSI_PIN,
                                              SD_MISO_PIN,
                                              SD_SCLK_PIN,
                                              SD_CS_PIN,
                                              125000000 / 2 / 4, // 15.6MHz
                                              true);
    filesystem_t *fat = filesystem_fat_create();
    int err = fs_mount("/", fat, sd);
    if (err == -1)
    {
        DEBUG_PRINT("format /\n");
        err = fs_format(fat, sd);
        if (err == -1)
        {
            DEBUG_PRINT("format err: %s\n", strerror(errno));
            return false;
        }
        err = fs_mount("/", fat, sd);
        if (err == -1)
        {
            DEBUG_PRINT("mount err: %s\n", strerror(errno));
            return false;
        }
    }
    return true;
}

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
    if (check_prog_info())
    {
        launch_application_from((void*)get_prog_info()->prog_addr);
    }
}

void boot_fwupdate()
{
    DEBUG_PRINT("entering boot_fwupdate\n");
    lcd_init();
    lcd_clear();

    draw_rect_spi(20, 140, 300, 180, WHITE);
    lcd_set_cursor(30, 150);
    lcd_print_string_color((char *)"FIRMWARE UPDATE", BLACK, WHITE);

    sleep_ms(2000);

    uint gpio_mask = 0u;
    reset_usb_boot(gpio_mask, 0);
}

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

int read_bootmode()
{
    int key = keypad_get_key();
    int _x;
    DEBUG_PRINT("read_bootmode key = %d\n", key);
    while((_x = keypad_get_key()) > 0) {
        // drain the keypad input buffer
        DEBUG_PRINT("read_bootmode subsequent key = %d\n", _x);
    }
    return key;
}

int main()
{
    stdio_init_all();

    uart_init(uart0, 115200);
    uart_set_format(uart0, 8, 1, UART_PARITY_NONE); // 8-N-1
    uart_set_fifo_enabled(uart0, false);

    // Initialize SD card detection pin
    gpio_init(SD_DET_PIN);
    gpio_set_dir(SD_DET_PIN, GPIO_IN);
    gpio_pull_up(SD_DET_PIN); // Enable pull-up resistor

    keypad_init();

    // Check bootmode now: 0=default, 1=sdcard, 2=fwupdate
    int bootmode = read_bootmode();
    DEBUG_PRINT("bootmode = %d\n", bootmode);
    switch(bootmode) {
      case KEY_ARROW_UP:
        // BOOTMODE_SDCARD
        break;
      case KEY_ARROW_DOWN:
        // BOOTMODE_FWUPDATE
        boot_fwupdate();
        break;
      default:
        // BOOTMODE_DEFAULT
        launch_application();
        break;
    }

    // BEGIN SDCARD BOOT

    lcd_init();
    lcd_clear();
	text_directory_ui_pre_init();

    // Check for SD card presence
    DEBUG_PRINT("Checking for SD card...\n");
    if (!sd_card_inserted())
    {
        DEBUG_PRINT("SD card not detected\n");
        text_directory_ui_set_status("No SD card found, please insert");

        // Poll until SD card is inserted
        while (!sd_card_inserted())
        {
            sleep_ms(100);
        }
    }

    // Card detected, wait for it to stabilize
    DEBUG_PRINT("SD card detected\n");
    text_directory_ui_set_status("Mounting SD card...");
    sleep_ms(1500); // Wait for card to stabilize

    // Initialize filesystem
    if (!fs_init())
    {
        text_directory_ui_set_status("Failed to mount SD card!");
        DEBUG_PRINT("Failed to mount SD card\n");
        sleep_ms(2000);
        watchdog_reboot(0, 0, 0);
    }

    sleep_ms(500);
    lcd_clear();

    text_directory_ui_init();
    text_directory_ui_set_final_callback(final_selection_callback);

    while(keypad_get_key() > 0) {
        // drain the keypad input buffer
    }

    text_directory_ui_run();
}
