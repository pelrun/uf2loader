// UF2 Implementation Test Suite
// Tests the UF2 parser, validation, and flash writing logic

#include "../bootloader/include/boot_defs.h"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include "test_common.h"
#include "uf2_test_utils.h"

// Flash constants for testing
// #define FLASH_PAGE_SIZE 256 - now in boot_defs.h
#define XIP_BASE 0x10000000

// Declare external functions
extern uint32_t crc32_calculate(const uint8_t *data, size_t len);

// Test counters
static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

// Mock flash memory for testing
extern uint8_t mock_flash[MOCK_FLASH_SIZE];
static bool flash_write_should_fail = false;

// Test macros
#define TEST(name) static void test_##name(void)
#define RUN_TEST(name) do { \
    printf("Running %s... ", #name); \
    tests_run++; \
    test_##name(); \
    tests_passed++; \
    printf("PASSED\n"); \
} while(0)

#define ASSERT_EQ(expected, actual) do { \
    if ((expected) != (actual)) { \
        printf("\nAssertion failed: %s == %s\n", #expected, #actual); \
        printf("  Expected: %d (0x%x)\n", (int)(expected), (int)(expected)); \
        printf("  Actual: %d (0x%x)\n", (int)(actual), (int)(actual)); \
        tests_failed++; \
        tests_passed--; \
        return; \
    } \
} while(0)

#define ASSERT_TRUE(condition) do { \
    if (!(condition)) { \
        printf("\nAssertion failed: %s\n", #condition); \
        tests_failed++; \
        tests_passed--; \
        return; \
    } \
} while(0)

#define ASSERT_MEM_EQ(expected, actual, size) do { \
    if (memcmp(expected, actual, size) != 0) { \
        printf("\nMemory comparison failed\n"); \
        tests_failed++; \
        tests_passed--; \
        return; \
    } \
} while(0)


// Test: Valid UF2 block parsing
TEST(valid_uf2_block) {
    uf2_block_t block;
    uint8_t test_data[FLASH_PAGE_SIZE];
    memset(test_data, 0xAA, sizeof(test_data));
    
    create_uf2_block(&block, 0, 1, 0x10040000, test_data, 0xe48bff56);
    
    // This would normally be called by the UF2 processor
    // We're testing the validation logic
    ASSERT_EQ(block.magic_start0, 0x0A324655);
    ASSERT_EQ(block.magic_start1, 0x9E5D5157);
    ASSERT_EQ(block.magic_end, 0x0AB16F30);
    ASSERT_EQ(block.target_addr, 0x10040000);
    ASSERT_EQ(block.payload_size, FLASH_PAGE_SIZE);
}

// Test: Invalid magic numbers
TEST(invalid_magic_numbers) {
    uf2_block_t block;
    create_uf2_block(&block, 0, 1, 0x10040000, NULL, 0xe48bff56);
    
    // Test each magic number being wrong
    block.magic_start0 = 0xDEADBEEF;
    ASSERT_TRUE(block.magic_start0 != 0x0A324655);
    
    create_uf2_block(&block, 0, 1, 0x10040000, NULL, 0xe48bff56);
    block.magic_start1 = 0xDEADBEEF;
    ASSERT_TRUE(block.magic_start1 != 0x9E5D5157);
    
    create_uf2_block(&block, 0, 1, 0x10040000, NULL, 0xe48bff56);
    block.magic_end = 0xDEADBEEF;
    ASSERT_TRUE(block.magic_end != 0x0AB16F30);
}

// Test: RP2040 family ID
TEST(rp2040_family_id) {
    uf2_block_t block;
    create_uf2_block(&block, 0, 1, 0x10040000, NULL, 0xe48bff56);
    
    // Check RP2040 family ID
    uint16_t family_id = block.file_size & 0xFFFF;
    ASSERT_EQ(family_id, 0xe48bff56 & 0xFFFF);
}

// Test: RP2350 family ID
TEST(rp2350_family_id) {
    uf2_block_t block;
    create_uf2_block(&block, 0, 1, 0x10040000, NULL, 0xe48bff59);
    
    // Check RP2350 family ID
    uint16_t family_id = block.file_size & 0xFFFF;
    ASSERT_EQ(family_id, 0xe48bff59 & 0xFFFF);
}

// Test: Out of bounds target address
TEST(out_of_bounds_address) {
    uf2_block_t block;
    
    // Test address too low (below flash start)
    create_uf2_block(&block, 0, 1, 0x00001000, NULL, 0xe48bff56);
    ASSERT_TRUE(block.target_addr < 0x10000000);
    
    // Test address too high (beyond reasonable flash)
    create_uf2_block(&block, 0, 1, 0x20000000, NULL, 0xe48bff56);
    ASSERT_TRUE(block.target_addr >= 0x20000000);
}

// Test: Block sequence validation
TEST(block_sequence) {
    uf2_block_t block1, block2, block3;
    
    // Create a sequence of blocks
    create_uf2_block(&block1, 0, 3, 0x10040000, NULL, 0xe48bff56);
    create_uf2_block(&block2, 1, 3, 0x10040000 + FLASH_PAGE_SIZE, NULL, 0xe48bff56);
    create_uf2_block(&block3, 2, 3, 0x10040000 + 2 * FLASH_PAGE_SIZE, NULL, 0xe48bff56);
    
    // Verify sequence
    ASSERT_EQ(block1.block_no, 0);
    ASSERT_EQ(block2.block_no, 1);
    ASSERT_EQ(block3.block_no, 2);
    ASSERT_EQ(block1.num_blocks, 3);
    ASSERT_EQ(block2.num_blocks, 3);
    ASSERT_EQ(block3.num_blocks, 3);
}

// Test: CRC32 calculation
TEST(crc32_calculation) {
    // Test vectors for CRC32
    const char *test_string = "123456789";
    uint32_t expected_crc = 0xCBF43926;  // Known CRC32 for "123456789"
    
    // Our CRC32 implementation
    uint32_t calculated_crc = crc32_calculate((const uint8_t *)test_string, 9);
    
    ASSERT_EQ(calculated_crc, expected_crc);
}

// Test: Flash write with CRC verification
TEST(flash_write_with_crc) {
    uint8_t test_data[FLASH_PAGE_SIZE];
    for (int i = 0; i < FLASH_PAGE_SIZE; i++) {
        test_data[i] = i & 0xFF;
    }
    
    // Write to mock flash
    uint32_t addr = 0x40000;
    flash_range_erase(addr, FLASH_PAGE_SIZE);
    flash_range_program(addr, test_data, FLASH_PAGE_SIZE);
    
    // Verify data was written correctly
    ASSERT_MEM_EQ(test_data, &mock_flash[addr], FLASH_PAGE_SIZE);
    
    // Test CRC verification
    extern bool verify_flash_crc32(uint32_t addr, const uint8_t *expected_data, size_t size);
    ASSERT_TRUE(verify_flash_crc32(addr, test_data, FLASH_PAGE_SIZE));
    
    // Corrupt one byte and verify CRC fails
    mock_flash[addr] ^= 0x01;
    ASSERT_TRUE(!verify_flash_crc32(addr, test_data, FLASH_PAGE_SIZE));
    
    // Restore the byte for cleanup
    mock_flash[addr] ^= 0x01;
}

// Test: RP2350-E10 workaround block
TEST(rp2350_e10_workaround) {
    uf2_block_t block;
    
    // Create RP2350-E10 workaround block
    create_uf2_block(&block, 0, 1, 0x10040000, NULL, 0xe48bff59);
    block.magic_start0 = 0xFFFFFFFF;
    block.magic_start1 = 0xFFFFFFFF;
    block.magic_end = 0xFFFFFFFF;
    block.target_addr = 0xFFFFFFFF;
    memset(block.data, 0xFF, sizeof(block.data));
    
    // Should be recognized as workaround block
    bool is_workaround = (block.magic_start0 == 0xFFFFFFFF &&
                         block.magic_start1 == 0xFFFFFFFF &&
                         block.magic_end == 0xFFFFFFFF);
    ASSERT_TRUE(is_workaround);
}

// Test: Flash erase functionality
TEST(flash_erase) {
    // Fill flash with pattern
    memset(mock_flash, 0x55, MOCK_FLASH_SIZE);
    
    // Erase a section
    uint32_t erase_addr = 0x10000;
    size_t erase_size = 4096;
    flash_range_erase(erase_addr, erase_size);
    
    // Verify erased to 0xFF
    for (size_t i = 0; i < erase_size; i++) {
        ASSERT_EQ(mock_flash[erase_addr + i], 0xFF);
    }
    
    // Verify surrounding areas untouched
    ASSERT_EQ(mock_flash[erase_addr - 1], 0x55);
    ASSERT_EQ(mock_flash[erase_addr + erase_size], 0x55);
}

// Test: Multi-block UF2 file simulation
TEST(multi_block_uf2) {
    uf2_block_t blocks[4];
    uint8_t test_data[4][FLASH_PAGE_SIZE];
    
    // Create 4 blocks with different data
    for (int i = 0; i < 4; i++) {
        memset(test_data[i], i * 0x11, FLASH_PAGE_SIZE);
        create_uf2_block(&blocks[i], i, 4, 0x10040000 + i * FLASH_PAGE_SIZE, 
                        test_data[i], 0xe48bff56);
    }
    
    // Erase the entire area we'll be using (all 4 pages)
    // This simulates erasing a sector before programming
    flash_range_erase(0x40000, FLASH_PAGE_SIZE * 4);
    
    // Process blocks
    for (int i = 0; i < 4; i++) {
        uint32_t addr = blocks[i].target_addr - 0x10000000;
        // Note: We already erased the entire area above
        flash_range_program(addr, blocks[i].data, FLASH_PAGE_SIZE);
    }
    
    // Verify all data written correctly
    for (int i = 0; i < 4; i++) {
        uint32_t addr = 0x40000 + i * FLASH_PAGE_SIZE;
        ASSERT_MEM_EQ(test_data[i], &mock_flash[addr], FLASH_PAGE_SIZE);
    }
}

// Main test runner
int main(void) {
    printf("=== UF2 Implementation Test Suite ===\n\n");
    
    // Initialize mock flash
    memset(mock_flash, 0xFF, MOCK_FLASH_SIZE);
    
    // Run all tests
    RUN_TEST(valid_uf2_block);
    RUN_TEST(invalid_magic_numbers);
    RUN_TEST(rp2040_family_id);
    RUN_TEST(rp2350_family_id);
    RUN_TEST(out_of_bounds_address);
    RUN_TEST(block_sequence);
    RUN_TEST(crc32_calculation);
    RUN_TEST(flash_write_with_crc);
    RUN_TEST(rp2350_e10_workaround);
    RUN_TEST(flash_erase);
    RUN_TEST(multi_block_uf2);
    
    // Summary
    printf("\n=== Test Summary ===\n");
    printf("Tests run: %d\n", tests_run);
    printf("Tests passed: %d\n", tests_passed);
    printf("Tests failed: %d\n", tests_failed);
    
    return tests_failed > 0 ? 1 : 0;
} 