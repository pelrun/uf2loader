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
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/clocks.h"
#include "debug.h"
#include "i2ckbd.h"
#include "lcdspi.h"
#include <hardware/flash.h>
#include <errno.h>
#include <hardware/watchdog.h>
#include "config.h"

#include "blockdevice/sd.h"
#include "filesystem/fat.h"
#include "filesystem/vfs.h"
#include "text_directory_ui.h"
#include "key_event.h"

const uint LEDPIN = 25;

bool sd_card_inserted(void)
{
    // Active low detection - returns true when pin is low
    return !gpio_get(SD_DET_PIN);
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
    int err = fs_mount("/sd", fat, sd);
    if (err == -1)
    {
        DEBUG_PRINT("format /sd\n");
        err = fs_format(fat, sd);
        if (err == -1)
        {
            DEBUG_PRINT("format err: %s\n", strerror(errno));
            return false;
        }
        err = fs_mount("/sd", fat, sd);
        if (err == -1)
        {
            DEBUG_PRINT("mount err: %s\n", strerror(errno));
            return false;
        }
    }
    return true;
}

static bool __not_in_flash_func(is_same_existing_program)(FILE *fp)
{
    uint8_t buffer[FLASH_SECTOR_SIZE] = {0};
    size_t program_size = 0;
    size_t len = 0;
    while ((len = fread(buffer, 1, sizeof(buffer), fp)) > 0)
    {
        uint8_t *flash = (uint8_t *)(XIP_BASE + SD_BOOT_FLASH_OFFSET + program_size);
        if (memcmp(buffer, flash, len) != 0)
            return false;
        program_size += len;
    }
    return true;
}

// This function must run from RAM since it erases and programs flash memory
static bool __not_in_flash_func(load_program)(const char *filename)
{
    FILE *fp = fopen(filename, "r");
    if (fp == NULL)
    {
        DEBUG_PRINT("open %s fail: %s\n", filename, strerror(errno));
        return false;
    }
    if (is_same_existing_program(fp))
    {
        // Program is up to date
    }

    // Check file size to ensure it doesn't exceed the available flash space
    if (fseek(fp, 0, SEEK_END) == -1)
    {
        DEBUG_PRINT("seek err: %s\n", strerror(errno));
        fclose(fp);
        return false;
    }

    long file_size = ftell(fp);
    if (file_size <= 0)
    {
        DEBUG_PRINT("invalid size: %ld\n", file_size);
        fclose(fp);
        return false;
    }

    if (file_size > MAX_APP_SIZE)
    {
        DEBUG_PRINT("file too large: %ld > %d\n", file_size, MAX_APP_SIZE);
        fclose(fp);
        return false;
    }

    DEBUG_PRINT("updating: %ld bytes\n", file_size);
    if (fseek(fp, 0, SEEK_SET) == -1)
    {
        DEBUG_PRINT("seek err: %s\n", strerror(errno));
        fclose(fp);
        return false;
    }

    size_t program_size = 0;
    uint8_t buffer[FLASH_SECTOR_SIZE] = {0};
    size_t len = 0;

    // Erase and program flash in FLASH_SECTOR_SIZE chunks
    while ((len = fread(buffer, 1, sizeof(buffer), fp)) > 0)
    {
        // Ensure we don't write beyond the application area
        if ((program_size + len) > MAX_APP_SIZE)
        {
            DEBUG_PRINT("err: write beyond app area\n");
            fclose(fp);
            return false;
        }

        uint32_t ints = save_and_disable_interrupts();
        flash_range_erase(SD_BOOT_FLASH_OFFSET + program_size, FLASH_SECTOR_SIZE);
        flash_range_program(SD_BOOT_FLASH_OFFSET + program_size, buffer, len);
        restore_interrupts(ints);

        program_size += len;
    }
    DEBUG_PRINT("program loaded\n");
    fclose(fp);
    return true;
}

// This function jumps to the application entry point
// It must update the vector table and stack pointer before jumping
void __not_in_flash_func(launch_application_from)(uint32_t *app_location)
{
    // https://vanhunteradams.com/Pico/Bootloader/Bootloader.html
    uint32_t *new_vector_table = app_location;
    volatile uint32_t *vtor = (uint32_t *)(PPB_BASE + M0PLUS_VTOR_OFFSET);
    *vtor = (uint32_t)new_vector_table;
    asm volatile(
        "msr msp, %0\n"
        "bx %1\n"
        :
        : "r"(new_vector_table[0]), "r"(new_vector_table[1])
        :);
}

// Check if a valid application exists in flash by examining the vector table
static bool is_valid_application(uint32_t *app_location)
{
    // Check that the initial stack pointer is within a plausible RAM region (assumed range for Pico: 0x20000000 to 0x20040000)
    uint32_t stack_pointer = app_location[0];
    if (stack_pointer < 0x20000000 || stack_pointer > 0x20040000)
    {
        return false;
    }

    // Check that the reset vector is within the valid flash application area
    uint32_t reset_vector = app_location[1];
    if (reset_vector < (0x10000000 + SD_BOOT_FLASH_OFFSET) || reset_vector > (0x10000000 + PICO_FLASH_SIZE_BYTES))
    {
        return false;
    }
    return true;
}

int load_firmware_by_path(const char *path)
{
    text_directory_ui_set_status("STAT: loading app...");

    // Attempt to load the application from the SD card
    // bool load_success = load_program(FIRMWARE_PATH);
    bool load_success = load_program(path);

    // Get the pointer to the application flash area
    uint32_t *app_location = (uint32_t *)(XIP_BASE + SD_BOOT_FLASH_OFFSET);

    // Check if there is an already valid application in flash
    bool has_valid_app = is_valid_application(app_location);



    if (load_success || has_valid_app)
    {
        text_directory_ui_set_status("STAT: launching app...");
        DEBUG_PRINT("launching app\n");
        // Small delay to allow printf to complete
        sleep_ms(100);
        launch_application_from(app_location);
    }
    else
    {
        text_directory_ui_set_status("ERR: No valid app");
        DEBUG_PRINT("no valid app, halting\n");

        sleep_ms(2000);

        // Trigger a watchdog reboot
        watchdog_reboot(0, 0, 0);
    }

    // We should never reach here
    while (1)
    {
        tight_loop_contents();
    }
}

void final_selection_callback(const char *path)
{
    // Trigger firmware loading with the selected path
    DEBUG_PRINT("selected: %s\n", path);

    char status_message[128];
    const char *extension = ".bin";
    size_t path_len = strlen(path);
    size_t ext_len = strlen(extension);

    if (path_len < ext_len || strcmp(path + path_len - ext_len, extension) != 0)
    {
        DEBUG_PRINT("not a bin: %s\n", path);
        snprintf(status_message, sizeof(status_message), "Err: FILE is not a .bin file");
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
    char buf[64];
    stdio_init_all();

    uart_init(uart0, 115200);
    uart_set_format(uart0, 8, 1, UART_PARITY_NONE); // 8-N-1
    uart_set_fifo_enabled(uart0, false);

    // Initialize SD card detection pin
    gpio_init(SD_DET_PIN);
    gpio_set_dir(SD_DET_PIN, GPIO_IN);
    gpio_pull_up(SD_DET_PIN); // Enable pull-up resistor

    keypad_init();
    lcd_init();
    lcd_clear();
    text_directory_ui_init();
    
    // Check for SD card presence
    DEBUG_PRINT("Checking for SD card...\n");
    if (!sd_card_inserted())
    {
        DEBUG_PRINT("SD card not detected\n");
        text_directory_ui_set_status("SD card not detected. \nPlease insert SD card.");
        
        // Poll until SD card is inserted
        while (!sd_card_inserted())
        {
            sleep_ms(100);
        }
        
        // Card detected, wait for it to stabilize
        DEBUG_PRINT("SD card detected\n");
        text_directory_ui_set_status("SD card detected. Mounting...");
        sleep_ms(1500); // Wait for card to stabilize
    }
    
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
    text_directory_ui_run();
}
