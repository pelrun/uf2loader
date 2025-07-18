// Extracted testable functions from uf2.c for testing
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>

// Mock XIP_BASE for testing
#define XIP_BASE 0x10000000
#define FLASH_PAGE_SIZE 256

static const uint32_t crc32_table[16] = {
    0x00000000, 0x1db71064, 0x3b6e20c8, 0x26d930ac,
    0x76dc4190, 0x6b6b51f4, 0x4db26158, 0x5005713c,
    0xedb88320, 0xf00f9344, 0xd6d6a3e8, 0xcb61b38c,
    0x9b64c2b0, 0x86d3d2d4, 0xa00ae278, 0xbdbdf21c
};

uint32_t crc32_calculate(const uint8_t *data, size_t size) {
    uint32_t crc = 0xffffffff;
    for (size_t i = 0; i < size; i++) {
        int tbl_idx = crc ^ data[i];
        crc = crc32_table[tbl_idx & 0x0f] ^ (crc >> 4);
        tbl_idx = crc ^ (data[i] >> 4);
        crc = crc32_table[tbl_idx & 0x0f] ^ (crc >> 4);
    }
    return ~crc;
}

bool verify_flash_crc32(uint32_t addr, const uint8_t *expected_data, size_t size) {
    extern uint8_t mock_flash[];
    uint32_t expected_crc = crc32_calculate(expected_data, size);
    uint8_t *flash_ptr = &mock_flash[addr];
    uint32_t flash_crc = crc32_calculate(flash_ptr, size);
    return expected_crc == flash_crc;
}
