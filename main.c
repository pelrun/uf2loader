/**
 * PicoCalc Hello World
 * https://www.clockworkpi.com/
 */
#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/clocks.h"
#include "i2ckbd.h"
#include "lcdspi.h"
#include <hardware/flash.h>
#include <errno.h>
#include <hardware/watchdog.h>
#include "config.h"


#include "blockdevice/sd.h"
#include "filesystem/fat.h"
#include "filesystem/vfs.h"


const uint LEDPIN = 25;


bool fs_init(void) {
    printf("fs_init FAT on SD card\n");
    blockdevice_t *sd = blockdevice_sd_create(spi0,
                                              SD_MOSI_PIN,
                                              SD_MISO_PIN,
                                              SD_SCLK_PIN,
                                              SD_CS_PIN,
                                              125000000 / 2 / 4,  // 15.6MHz
                                              true);
    filesystem_t *fat = filesystem_fat_create();
    int err = fs_mount("/sd", fat, sd);
    if (err == -1) {
        printf("format /sd with FAT\n");
        err = fs_format(fat, sd);
        if (err == -1) {
            printf("fs_format error: %s", strerror(errno));
            return false;
        }
        err = fs_mount("/sd", fat, sd);
        if (err == -1) {
            printf("fs_mount error: %s", strerror(errno));
            return false;
        }
    }
    return true;
}


static bool __not_in_flash_func(is_same_existing_program)(FILE *fp) {
    uint8_t buffer[FLASH_SECTOR_SIZE] = {0};
    size_t program_size = 0;
    size_t len = 0;
    while ((len = fread(buffer, 1, sizeof(buffer), fp)) > 0) {
        uint8_t *flash = (uint8_t *)(XIP_BASE + SD_BOOT_FLASH_OFFSET + program_size);
        if (memcmp(buffer, flash, len) != 0)
            return false;
        program_size += len;
    }
    return true;
}

// This function must run from RAM since it erases and programs flash memory
static bool __not_in_flash_func(load_program)(const char *filename) {
    FILE *fp = fopen(filename, "r");
    if (fp == NULL) {
        printf("fopen %s failed: %s\n", filename, strerror(errno));
        return false;
    }
    if (is_same_existing_program(fp)) {
        // printf("No update required, program is up to date.\n");
        // return true;
    }

    // Check file size to ensure it doesn't exceed the available flash space
    if (fseek(fp, 0, SEEK_END) == -1) {
        printf("fseek to end failed: %s\n", strerror(errno));
        fclose(fp);
        return false;
    }
    
    long file_size = ftell(fp);
    if (file_size <= 0) {
        printf("Invalid file size: %ld\n", file_size);
        fclose(fp);
        return false;
    }
    
    if (file_size > MAX_APP_SIZE) {
        printf("Firmware file too large (%ld bytes). Maximum size is %d bytes.\n", 
               file_size, MAX_APP_SIZE);
        fclose(fp);
        return false;
    }
    
    printf("Updates found. Updating program (%ld bytes)\n", file_size);
    if (fseek(fp, 0, SEEK_SET) == -1) {
        printf("fseek failed: %s\n", strerror(errno));
        fclose(fp);
        return false;
    }
    
    size_t program_size = 0;
    uint8_t buffer[FLASH_SECTOR_SIZE] = {0};
    size_t len = 0;
    
    // Erase and program flash in FLASH_SECTOR_SIZE chunks
    while ((len = fread(buffer, 1, sizeof(buffer), fp)) > 0) {
        // Ensure we don't write beyond the application area
        if ((program_size + len) > MAX_APP_SIZE) {
            printf("Error: Attempted to write beyond application area\n");
            fclose(fp);
            return false;
        }
        
        uint32_t ints = save_and_disable_interrupts();
        flash_range_erase(SD_BOOT_FLASH_OFFSET + program_size, FLASH_SECTOR_SIZE);
        flash_range_program(SD_BOOT_FLASH_OFFSET + program_size, buffer, len);
        restore_interrupts(ints);

        program_size += len;
    }
    printf("Program loaded successfully!\n");
    fclose(fp);
    return true;
}

// This function jumps to the application entry point
// It must update the vector table and stack pointer before jumping
void __not_in_flash_func(launch_application_from)(uint32_t *app_location) {
    // https://vanhunteradams.com/Pico/Bootloader/Bootloader.html
    uint32_t *new_vector_table = app_location;
    volatile uint32_t *vtor = (uint32_t *)(PPB_BASE + M0PLUS_VTOR_OFFSET);
    *vtor = (uint32_t)new_vector_table;
    asm volatile (
        "msr msp, %0\n"
        "bx %1\n"
        :
        : "r" (new_vector_table[0]), "r" (new_vector_table[1])
        : );
}

// Check if a valid application exists in flash by examining the vector table
static bool is_valid_application(uint32_t *app_location) {
    // Check that the initial stack pointer is within a plausible RAM region (assumed range for Pico: 0x20000000 to 0x20040000)
    uint32_t stack_pointer = app_location[0];
    if (stack_pointer < 0x20000000 || stack_pointer > 0x20040000) {
        return false;
    }

    // Check that the reset vector is within the valid flash application area
    uint32_t reset_vector = app_location[1];
    if (reset_vector < (0x10000000 + SD_BOOT_FLASH_OFFSET) || reset_vector > (0x10000000 + PICO_FLASH_SIZE_BYTES)) {
        return false;
    }
    return true;
}




int main() {
    char buf[64];
    
    // set_sys_clock_khz(133000, true);
    stdio_init_all();

    // Wait I/O and SD power on and initialized
    sleep_ms(200);
    

    uart_init(uart0, 115200);

    uart_set_format(uart0, 8, 1, UART_PARITY_NONE);  // 8-N-1
    uart_set_fifo_enabled(uart0, false);


    keypad_init();
    lcd_init();
    fs_init();
    sleep_ms(500);
    printf("SD Card Bootloader starting...\n");
    

    gpio_init(LEDPIN);
    gpio_set_dir(LEDPIN, GPIO_OUT);

    lcd_clear();
    lcd_print_string("PicoCalc SD Bootloader\n");

    gpio_put(LEDPIN, 1);
    sleep_ms(500);
    gpio_put(LEDPIN, 0);


    // Attempt to load the application from the SD card
    bool load_success = load_program(FIRMWARE_PATH);

    // Get the pointer to the application flash area
    uint32_t *app_location = (uint32_t *)(XIP_BASE + SD_BOOT_FLASH_OFFSET);

    // Check if there is an already valid application in flash
    bool has_valid_app = is_valid_application(app_location);

    if (load_success || has_valid_app)
    {
        lcd_print_string("Launching application...\n");
    
        printf("Launching application...\n");
        // Small delay to allow printf to complete
        sleep_ms(100);
        launch_application_from(app_location);
    }
    else
    {
        lcd_print_string("No valid application found. Halting.\n");
        printf("No valid application found. Halting.\n");
        
        sleep_ms(2000);


        while(1){
            int key =  keypad_get_key();
            printf("Key event %x\n", key);
            sleep_ms(100);
        }


        // Trigger a watchdog reboot
        watchdog_reboot(0, 0, 0);
    }

    // We should never reach here
    while (1)
    {
        tight_loop_contents();
    }

    // while (1) {

    //     int c = lcd_getc(0);
    //     if(c != -1 && c > 0) {
    //         lcd_putc(0,c);
    //     }
    //     sleep_ms(10);
    // }
}

