#include "test_common.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h> // Added for printf

// Mock flash memory
uint8_t mock_flash[MOCK_FLASH_SIZE];

void mock_flash_init(void) {
    // Initialize flash with 0xFF (erased state)
    memset(mock_flash, 0xFF, MOCK_FLASH_SIZE);
}

void mock_flash_erase(uint32_t addr, size_t size) {
    if (addr + size > MOCK_FLASH_SIZE) {
        return; // Out of bounds
    }
    // Set to 0xFF (erased state)
    memset(mock_flash + addr, 0xFF, size);
}

void mock_flash_program(uint32_t addr, const uint8_t *data, size_t size) {
    if (addr + size > MOCK_FLASH_SIZE) {
        return; // Out of bounds
    }
    // Simulate flash programming (can only change 1s to 0s)
    for (size_t i = 0; i < size; i++) {
        mock_flash[addr + i] &= data[i];
    }
}

// Simple mock implementation of crc32_calculate for testing
// This is a basic CRC-32 implementation that matches the expected signature
uint32_t crc32_calculate(const uint8_t *data, size_t size) {
    if (!data || size == 0) {
        return 0xFFFFFFFF; // Standard initial value for CRC-32
    }
    
    // This is a simple CRC-32 implementation (not optimized for performance)
    uint32_t crc = 0xFFFFFFFF;
    static const uint32_t crc_table[16] = {
        0x00000000, 0x1DB71064, 0x3B6E20C8, 0x26D930AC,
        0x76DC4190, 0x6B6B51F4, 0x4DB26158, 0x5005713C,
        0xEDB88320, 0xF00F9344, 0xD6D6A3E8, 0xCB61B38C,
        0x9B64C2B0, 0x86D3D2D4, 0xA00AE278, 0xBDBDF21C
    };
    
    for (size_t i = 0; i < size; i++) {
        uint8_t b = data[i];
        crc = (crc >> 4) ^ crc_table[(crc & 0xF) ^ (b & 0xF)];
        crc = (crc >> 4) ^ crc_table[(crc & 0xF) ^ (b >> 4)];
    }
    
    return ~crc; // Final XOR with 0xFFFFFFFF
}
