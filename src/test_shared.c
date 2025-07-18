// Shared test utilities for UF2 bootloader tests

#include <stdio.h>
#include <stdint.h>
#include <string.h>

// Mock flash memory
#define MOCK_FLASH_SIZE (2 * 1024 * 1024)  // 2MB
uint8_t mock_flash[MOCK_FLASH_SIZE];

// Mock flash operations
void flash_range_erase(uint32_t addr, size_t size) {
    if (addr + size > MOCK_FLASH_SIZE) {
        printf("ERROR: Flash erase out of bounds: 0x%x + %zu\n", addr, size);
        return;
    }
    memset(&mock_flash[addr], 0xFF, size);
}

void flash_range_program(uint32_t addr, const uint8_t *data, size_t size) {
    if (addr + size > MOCK_FLASH_SIZE) {
        printf("ERROR: Flash program out of bounds: 0x%x + %zu\n", addr, size);
        return;
    }
    memcpy(&mock_flash[addr], data, size);
} 